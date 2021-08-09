#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "88dcdd.h"
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <applibs/log.h>
#include <string.h>
#include "utils.h"

#define DISK_DEBUG
// #define DISK_DEBUG_VERBOSE
// #define VDISK_TRACE

static VDISK_SECTOR_T vdisk_sector;
static pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//static pthread_cond_t intercore_disk_cond = PTHREAD_COND_INITIALIZER;
//static pthread_mutex_t intercore_disk_lock = PTHREAD_MUTEX_INITIALIZER;

static vdisk_mqtt_write_sector_t write_sector;
static bool read_from_cache = false;
static uint32_t sector_requested = __UINT32_MAX__;

//static uint16_t cal_crc(uint8_t *sector)
//{
//    uint16_t crc = 0x0000;
//    for (int x = 0; x < 137; x++) {
//        crc += intercore_disk_block.sector[x];
//        crc &= 0xffff;
//    }
//
//    return crc;
//}

static void vdisk_cache_write_sector(uint8_t *sectorData, uint16_t sector_number)
{
    intercore_disk_block.disk_ic_msg_type = DISK_IC_WRITE;
    intercore_disk_block.sector_number = sector_number;
    memcpy(intercore_disk_block.sector, sectorData, SECTOR_SIZE);

    dx_intercorePublish(&intercore_disk_cache_ctx, &intercore_disk_block, sizeof(INTERCORE_DISK_DATA_BLOCK_T));
}

static void vdisk_cache_read_sector(disk_t *pDisk)
{
    read_from_cache = false;
    uint16_t requested_sector_number = (uint16_t)(pDisk->diskPointer / 137);

    // prepare and send intercore message requesting offset from cache
    intercore_disk_block.disk_ic_msg_type = DISK_IC_READ;
    intercore_disk_block.sector_number = requested_sector_number;
    intercore_disk_block.cached = false;

    if (!dx_intercorePublish(&intercore_disk_cache_ctx, &intercore_disk_block, 3)) {
        // false will be return if the RT core app is not running
        return;
    }

    //// NOTE, the cache server on M4 must be compiled in released mode, optimised for speed to service read requests in time
    //// measure using finish clock_gettime(CLOCK_MONOTONIC, &finish); - start clock_gettime(CLOCK_MONOTONIC, &start);
    //// measurements were timed using release build with debugger attached for log_debug time stats
    //// Measurements averaged over 1000 read requests from the cache
    //// Average response time measured before request sent
    //// Avg cache read response 208279 nanoseconds
    //// Avg cache read response 211613 nanoseconds
    //// Avg cache read response 210598 nanoseconds
    //// Avg cache read response 211290 nanoseconds

    //// Average response time measured after request sent
    //// Avg cache read response 45421 nanoseconds
    //// Avg cache read response 45469 nanoseconds
    //// Avg cache read response 45774 nanoseconds
    //// Avg cache read response 45781 nanoseconds
    //// Avg cache read response 45772 nanoseconds

    if (dx_intercoreRead(&intercore_disk_cache_ctx) == -1) {
        return;
    }

    if (intercore_disk_block.cached && intercore_disk_block.sector_number == requested_sector_number) {
        memcpy(pDisk->sectorData, intercore_disk_block.sector, SECTOR_SIZE);
        pDisk->sectorPointer = 0;
        read_from_cache = true;
        (*(int *)dt_diskCacheHits.propertyValue)++;
    } else {
        (*(int *)dt_diskCacheMisses.propertyValue)++;
    }
}

void vdisk_mqtt_response_cb(uint8_t *sector)
{
    // first 4 bytes are the sector number in the response from the virtual disk server
    if (*(uint32_t *)(sector) == sector_requested) {
        sector_requested = __UINT32_MAX__;

        pthread_mutex_lock(&lock);

        // Skip the first 4 bytes as they are the sector data
        memcpy(vdisk_sector.data, sector + 4, 137);
        vdisk_sector.dirty = true;

        pthread_cond_signal(&cond1);
        pthread_mutex_unlock(&lock);
    }
}

int write_virtual_sector(disk_t *pDisk)
{
    // The sector size presented to this API is 138 bytes
    // This includes a terminating control byte
    // Altair 8inch floopy sector size is 137 so use SECTOR_SIZE = 137 bytes
    write_sector.offset = pDisk->diskPointer;
    memcpy(write_sector.data, pDisk->sectorData, SECTOR_SIZE);

    vdisk_cache_write_sector(pDisk->sectorData, (uint16_t)(pDisk->diskPointer / 137));
    vdisk_mqtt_write_sector(&write_sector);

    (*(int *)dt_diskTotalWrites.propertyValue)++;

    return 0;
}

bool read_virtual_sector(disk_t *pDisk)
{
    struct timespec now = {0, 0};
    vdisk_sector.dirty = false;
    bool result = false;

    vdisk_cache_read_sector(pDisk);

    if (read_from_cache) {
        result = true;
    } else {

        // save sector requested to be compared with inbound mqtt sector
        sector_requested = pDisk->diskPointer;

        // data not found in cache - try reading from vdisk storage
        vdisk_mqtt_read_sector(pDisk->diskPointer);

        // allow up to 8 seconds for the io to complete
        clock_gettime(CLOCK_REALTIME, &now);
        now.tv_sec += 8;

        pthread_mutex_lock(&lock);
        pthread_cond_timedwait(&cond1, &lock, &now);
        pthread_mutex_unlock(&lock);

        if (vdisk_sector.dirty) {
            memcpy(pDisk->sectorData, vdisk_sector.data, SECTOR_SIZE);
            pDisk->sectorPointer = 0;

            vdisk_cache_write_sector(pDisk->sectorData, (uint16_t)(pDisk->diskPointer / 137));

            result = true;

        } else {
            // Log_Debug("VDISK Read Fail\n");
            (*(int *)dt_diskTotalErrors.propertyValue)++;
        }
    }

    return result;
}

typedef enum { TRACK_MODE, SECTOR_MODE } DISK_SELECT_MODE;

void writeSector(disk_t *disk);

disks disk_drive;

void set_status(uint8_t bit)
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

    disk_drive.current->status &= (uint8_t)~bit;
}

void clear_status(uint8_t bit)
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

    disk_drive.current->status |= bit;
}

void disk_select(uint8_t b)
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

    uint8_t select = b & 0xf;
    disk_drive.currentDisk = select;

    if (select == 0) {
        disk_drive.current = &disk_drive.disk1;
    } else if (select == 1) {
        disk_drive.current = &disk_drive.disk2;
    } else {
        disk_drive.current = &disk_drive.nodisk;
    }
}

uint8_t disk_status()
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

#ifdef DISK_DEBUG
#ifdef ARDUINO
    Serial.print("Returning status ");
    Serial.print(disk_drive.current->status);
    Serial.println(" for disk");
#else
#ifdef DISK_DEBUG_VERBOSE
    Log_Debug("Returning status %d for disk %d\n", disk_drive.current->status, disk_drive.currentDisk);
#endif
#endif
#endif
    return disk_drive.current->status;
}

void disk_function(uint8_t b)
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

#ifdef DISK_DEBUG
#ifdef ARDUINO
    Serial.print("Disk function ");
    Serial.println(b);
#else
#ifdef DISK_DEBUG_VERBOSE
    Log_Debug("Disk function %d, disk %d\n", b, disk_drive.currentDisk);
#endif
#endif
#endif

    if (b & CONTROL_STEP_IN) {
        disk_drive.current->track++;
        disk_drive.current->sector = 0;

        if (disk_drive.current->track != 0)
            clear_status(STATUS_TRACK_0);
#ifdef ARDUINO
        disk_drive.current->fp.seek(TRACK * disk_drive.current->track);
#else
        if (disk_drive.currentDisk != 0 && disk_drive.current->sectorDirty) {
            writeSector(disk_drive.current);
        }

        uint32_t seek_offset = TRACK * disk_drive.current->track;

#ifdef VDISK_TRACE
        Log_Debug("Track Step In - Disk %d, Track %d, Offset 0x%08lx\n", disk_drive.currentDisk, disk_drive.current->track, seek_offset);
#endif

        if (disk_drive.currentDisk == 0) {
            lseek(disk_drive.current->fp, seek_offset, SEEK_SET);
        } else {
            disk_drive.current->diskPointer = seek_offset;
            disk_drive.current->haveSectorData = false;
            disk_drive.current->sectorPointer = 0;
        }
#endif

#ifdef DISK_DEBUG
#ifdef ARDUINO
        Serial.print("Track seek to : ");
        Serial.println(TRACK * disk_drive.current->track);
#else
        //		Log_Debug("Track (%d) - seek to : %d, disk %d\n", disk_drive.current->track, TRACK * disk_drive.current->track, disk_drive.currentDisk);
#endif
#endif
    }
    if (b & CONTROL_STEP_OUT) {
        if (disk_drive.current->track > 0)
            disk_drive.current->track--;
        if (disk_drive.current->track == 0)
            set_status(STATUS_TRACK_0);

        disk_drive.current->sector = 0;

#ifdef ARDUINO
        disk_drive.current->fp.seek(TRACK * disk_drive.current->track);
#else
        if (disk_drive.currentDisk != 0 && disk_drive.current->sectorDirty) {
            writeSector(disk_drive.current);
        }

        uint32_t seek_offset = TRACK * disk_drive.current->track;

#ifdef VDISK_TRACE
        Log_Debug("Track Step Out - Disk %d, Track %d, Offset 0x%08lx\n", disk_drive.currentDisk, disk_drive.current->track, seek_offset);
#endif

        if (disk_drive.currentDisk == 0) {
            lseek(disk_drive.current->fp, seek_offset, SEEK_SET);
        } else {
            disk_drive.current->diskPointer = seek_offset;
            disk_drive.current->haveSectorData = false;
            disk_drive.current->sectorPointer = 0;
        }
#endif
#ifdef DISK_DEBUG
#ifdef ARDUINO
        Serial.print("Track seek to : ");
        Serial.println(TRACK * disk_drive.current->track);
#else
        //                Log_Debug("Track seek to : %d, disk %d\n", TRACK * disk_drive.current->track, disk_drive.currentDisk);
#endif
#endif
    }
    if (b & CONTROL_HEAD_LOAD) {
        set_status(STATUS_HEAD);
        set_status(STATUS_NRDA);
    }
    if (b & CONTROL_HEAD_UNLOAD) {
        clear_status(STATUS_HEAD);
    }
    if (b & CONTROL_IE) {
    }
    if (b & CONTROL_ID) {
    }
    if (b & CONTROL_HCS) {
    }
    if (b & CONTROL_WE) {
        set_status(STATUS_ENWD);
        disk_drive.current->write_status = 0;
    }
}

uint8_t sector()
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

    uint32_t seek_offset;
    uint8_t ret_val;

    if (disk_drive.current->sector == 32) {
        disk_drive.current->sector = 0;
    }

    if (disk_drive.currentDisk != 0 && disk_drive.current->sectorDirty) {
        writeSector(disk_drive.current);
    }

    // current_sector = current_sector % 32;
    // seek = disk_drive.current->track * TRACK + disk_drive.current->sector * (SECTOR);
#ifdef ARDUINO
    disk_drive.current->fp.seek(seek);
#else
    // _lseek(disk_drive.current->fp, seek, SEEK_SET);

    seek_offset = disk_drive.current->track * TRACK + disk_drive.current->sector * (SECTOR_SIZE);
    disk_drive.current->sectorPointer = 0;

#ifdef VDISK_TRACE
    Log_Debug("Sector - Disk %d: Track %d: Sector %d - Offset 0x%08lx\n", disk_drive.currentDisk, disk_drive.current->track, disk_drive.current->sector, seek_offset);
#endif

    if (disk_drive.currentDisk == 0) {
        lseek(disk_drive.current->fp, seek_offset, SEEK_SET);
    } else {
        disk_drive.current->diskPointer = seek_offset;
        disk_drive.current->sectorPointer = 0; // needs to be set here for write operation (read fetches sector data and resets the pointer).
        disk_drive.current->haveSectorData = false;
    }
#endif

    ret_val = (uint8_t)(disk_drive.current->sector << 1);
#ifdef DISK_DEBUG
#ifdef ARDUINO
    Serial.print("Current sector: ");
    Serial.print(disk_drive.current->sector);
    Serial.print(" (");
    Serial.print(ret_val, HEX);
    Serial.print(") (bytes per track: ");
    Serial.print(TRACK);
    Serial.println(")");
#else
    // printf("sector (%d), Track (%d), Seek (0x%08lx)\n", disk_drive.current->sector, disk_drive.current->track, seek);
    // printf("sector > Current sector: %d (0x%02x) (bytes per track: %d)\n", disk_drive.current->sector, ret_val, TRACK);
#endif
#endif

    disk_drive.current->sector++;
    return ret_val;
}

void disk_write(uint8_t b)
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

#ifdef DISK_DEBUG
#ifdef ARDUINO
    Serial.print("Write ");
    Serial.print(b);
    Serial.print(" (byte in sector: ");
    Serial.print(disk_drive.current->write_status);
    Serial.println(")");
#else
    // Log_Debug("Write %d (byte in sector: %d)\n", b, disk_drive.current->write_status);
#endif
#endif
#ifdef ARDUINO
    disk_drive.current->fp.write(&b, 1);
#else
    //int wrRet = -1;

    if (disk_drive.currentDisk != 0) {
        // calculate file offset from TRACK and offset.
        disk_drive.current->sectorData[disk_drive.current->sectorPointer++] = b;
        disk_drive.current->sectorDirty = true;

#ifdef VDISK_TRACE
        Log_Debug("Write - [0x%02x] Disk %d: Track %d: Sector %d: Offset 0x%08lx\n", b, disk_drive.currentDisk, disk_drive.current->track, disk_drive.current->sector,
                  disk_drive.current->diskPointer + disk_drive.current->sectorPointer);
#endif
    }
    // write(disk_drive.current->fp, &b, 1);

#endif
    if (disk_drive.current->write_status == 137) {
        disk_drive.current->write_status = 0;
        clear_status(STATUS_ENWD);
#ifdef DISK_DEBUG
#ifdef ARDUINO
        Serial.println("Disabling clear");
#else
        // Log_Debug("Disabling clear\n");
#endif
#endif
    } else
        disk_drive.current->write_status++;
}

uint8_t disk_read()
{
#ifdef FUNCTION_TRACE
    Log_Debug(">>> %s\n", __func__);
#endif

    static uint32_t bytes = 0;
    uint8_t b = 0;

#ifdef ARDUINO
    b = disk_drive.current->fp.read();
#else
    //bool result = false;

    if (disk_drive.currentDisk == 0) {
        int readval = read(disk_drive.current->fp, &b, (size_t)1);
        if (readval != 1) {
            Log_Debug("read byte, disk 0 failed\n");
        }
        //#ifdef VDISK_TRACE
        //		uint32_t offset = lseek(disk_drive.current->fp, 0x00, SEEK_CUR);
        //		Log_Debug("Read - [0x%02x] Disk %d: Track %d: Sector %d: Offset 0x%08lx\n", b, disk_drive.currentDisk, disk_drive.current->track, disk_drive.current->sector,
        //offset); #endif
    } else {
        if (!disk_drive.current->haveSectorData) {
            disk_drive.current->haveSectorData = true;
            disk_drive.current->sectorPointer = 0;
            if (!read_virtual_sector(disk_drive.current)) {
                Log_Debug("Virtual disk sector read failed\n");
                b = 0;
            }
        }
        b = disk_drive.current->sectorData[disk_drive.current->sectorPointer++];
#ifdef VDISK_TRACE
        Log_Debug("Read - [0x%02x] Disk %d: Track %d: Sector %d: Offset 0x%08lx\n", b, disk_drive.currentDisk, disk_drive.current->track, disk_drive.current->sector,
                  disk_drive.current->diskPointer + disk_drive.current->sectorPointer);
#endif
    }

    // int readval=_read(disk_drive.current->fp, &b, (size_t)1);
    // if (readval == -1)
    //{
    //	printf("failed to read\n");
    //}
    // fread(&b, 1, 1, disk_drive.current->fp);
#endif

    bytes++;

#ifdef DISK_DEBUG
#ifdef ARDUINO
    Serial.print("Reading byte ");
    Serial.print(bytes);
    Serial.print(" (");
    Serial.print(b, HEX);
    Serial.println(")");
#else
#ifdef DISK_DEBUG_VERBOSE
    Log_Debug("Reading byte %d (0x%02x [%c])\n", trackDataPointer, b, b < 0x20 || b > 0x80 ? ' ' : b);
#endif

#endif
#endif
    return b;
}

void writeSector(disk_t *pDisk)
{
    write_virtual_sector(pDisk);

    pDisk->sectorPointer = 0;
    pDisk->sectorDirty = false;
}
