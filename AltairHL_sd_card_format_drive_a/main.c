/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * This example is built on the Azure Sphere DevX library.
 *   1. DevX is an Open Source community-maintained implementation of the Azure Sphere SDK samples.
 *   2. DevX is a modular library that simplifies common development scenarios.
 *        - You can focus on your solution, not the plumbing.
 *   3. DevX documentation is maintained at https://github.com/gloveboxes/AzureSphereDevX/wiki
 *	 4. The DevX library is not a substitute for understanding the Azure Sphere SDK Samples.
 *          - https://github.com/Azure/azure-sphere-samples
 *
 * DEVELOPER BOARD SELECTION
 *
 * The following developer boards are supported.
 *
 *	 1. AVNET Azure Sphere Starter Kit.
 *   2. AVNET Azure Sphere Starter Kit Revision 2.
 *	 3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	 4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 * ENABLE YOUR DEVELOPER BOARD
 *
 * Each Azure Sphere developer board manufacturer maps pins differently. You need to select the
 *    configuration that matches your board.
 *
 * Follow these steps:
 *
 *	 1. Open CMakeLists.txt.
 *	 2. Uncomment the set command that matches your developer board.
 *	 3. Click File, then Save to auto-generate the CMake Cache.
 *
 *
 *
 * The intercore communications labs require multiple instances of VS Code to be running
 *
 * It is recommended to install the VS Code Peacock extension for the intercore communications labs.
 * The Peacock extension allows you to change the color of your Visual Studio Code workspace.
 * Ideal when you have multiple VS Code instances
 *
 * Install the Peacock extension from
 * https://marketplace.visualstudio.com/items?itemName=johnpapa.vscode-peacock
 *
 * The following colours have been set:
 * The VS Code instance attached to the Real-Time core will be red. Real-time is red, as in racing
 * red. The VS Code instance attached to the High-level core is blue. High-level is blue, as in sky
 * is high and blue. You can change the default colours to match your preferences.
 *
 *
 * Intercore messaging.
 *
 * There needs to be a shared understanding of the data structure being shared between the real-time
 * and high-level apps This shared understanding is declared in the intercore_contract.h file.  This
 * file can be found in the IntercoreContract directory.
 *
 *************************************************************************************************************************************/

// Hardware definition
#include "hw/azure_sphere_learning_path.h"

// Learning Path Libraries
#include "dx_config.h"
#include "app_exit_codes.h"
#include "dx_intercore.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_gpio.h"

#include "../IntercoreContract/intercore_contract.h"

// System Libraries
#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <applibs/storage.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define REAL_TIME_COMPONENT_ID_APP_ONE "005180bc-402f-4cb3-a662-72937dbcde47"

// Forward signatures
static void button_a_handler(EventLoopTimer *eventLoopTimer);

INTERCORE_DISK_DATA_BLOCK_T ic_control_block_app_one = {.disk_ic_msg_type = DISK_IC_UNKNOWN, .cached = false, .sector_number = 0};

DX_INTERCORE_BINDING intercore_app_one = {.sockFd = -1,
                                          .nonblocking_io = false,
                                          .rtAppComponentId = REAL_TIME_COMPONENT_ID_APP_ONE,
                                          .interCoreCallback = NULL,
                                          .intercore_recv_block = &ic_control_block_app_one,
                                          .intercore_recv_block_length = sizeof(ic_control_block_app_one)};

static DX_GPIO_BINDING gpio_button_a = {.pin = BUTTON_A, .name = "button a", .direction = DX_INPUT};
static DX_GPIO_BINDING gpio_led_red = {.pin = LED_RED, .name = "red led", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING gpio_led_green = {.pin = LED_GREEN, .name = "green led", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};
DX_GPIO_BINDING *gpio_binding_sets[] = {&gpio_button_a, &gpio_led_red, &gpio_led_green};

// Timers
static DX_TIMER_BINDING tmr_button_a_handler = {.period = {0, 100000000}, .name = "tmr_button_handler", .handler = button_a_handler};

DX_TIMER_BINDING *timerSet[] = {&tmr_button_a_handler};

static uint16_t cal_crc(uint8_t *data)
{
    uint16_t crc = 0x0000;
    for (int x = 0; x < 137; x++) {
        crc += data[x];
        crc &= 0xffff;
    }
    return crc;
}

static bool write_drive(const char *filename, uint8_t drive_number)
{
    off_t sector_number = 0;
    uint16_t sd_crc, st_crc;
    bool success = true;
    Log_Debug("Writing: %s\n", filename);

    int disk_fd = Storage_OpenFileInImagePackage(filename);
    if (disk_fd != -1) {

        while (true) {

            dx_gpioOn(&gpio_led_green);
            dx_gpioOff(&gpio_led_red);

            ssize_t ret = pread(disk_fd, ic_control_block_app_one.sector, sizeof(ic_control_block_app_one.sector), sector_number * 137);
            if (ret == 0) {
                break;
            }
            ic_control_block_app_one.cached = false;
            ic_control_block_app_one.success = false;
            ic_control_block_app_one.drive_number = drive_number;
            ic_control_block_app_one.success = false;
            ic_control_block_app_one.sector_number = (uint16_t)sector_number;
            ic_control_block_app_one.disk_ic_msg_type = DISK_IC_WRITE;
            dx_intercorePublish(&intercore_app_one, &ic_control_block_app_one, sizeof(ic_control_block_app_one));

            dx_intercoreRead(&intercore_app_one);
            if (ic_control_block_app_one.success) {
                Log_Debug("block written: %d\n", (int)sector_number);
            } else {
                Log_Debug("block NOT written\n");
            }

            sector_number++;
        }

        sector_number = 0;

        // now verify writes to sd card
        while (true) {

            ssize_t ret = pread(disk_fd, ic_control_block_app_one.sector, sizeof(ic_control_block_app_one.sector), sector_number * 137);
            if (ret == 0) {
                break;
            }

            Log_Debug("Reading sector %d\n", (int)sector_number);

            st_crc = cal_crc(ic_control_block_app_one.sector);

            memset(ic_control_block_app_one.sector, 0x00, sizeof(ic_control_block_app_one.sector));
            ic_control_block_app_one.cached = false;
            ic_control_block_app_one.success = false;
            ic_control_block_app_one.drive_number = drive_number;
            ic_control_block_app_one.sector_number = (uint16_t)sector_number;
            ic_control_block_app_one.disk_ic_msg_type = DISK_IC_READ;
            dx_intercorePublish(&intercore_app_one, &ic_control_block_app_one, sizeof(ic_control_block_app_one));

            dx_intercoreRead(&intercore_app_one);
            if (ic_control_block_app_one.success) {

                sd_crc = cal_crc(ic_control_block_app_one.sector);

                if (sd_crc != st_crc) {
                    success = false;
                    Log_Debug("CRCs don't match for sector %d\n", (int)sector_number);
                }

            } else {
                Log_Debug("block NOT read\n");
            }

            sector_number++;
        }
    }

    if (!success) {
        Log_Debug("Error: SD Card verify failed!\n");
        dx_gpioOn(&gpio_led_red);
        dx_gpioOff(&gpio_led_green);
    } else {
        Log_Debug("SD Card verify passed!\n");
        dx_gpioOff(&gpio_led_green);
    }

    return success;
}

static void button_a_handler(EventLoopTimer *eventLoopTimer)
{
    static GPIO_Value_Type buttonAState;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    if (dx_gpioStateGet(&gpio_button_a, &buttonAState)) {
        write_drive("Disks/cpm63k.dsk", 0);
    }
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralAndHandlers(void)
{
    dx_gpioSetOpen(gpio_binding_sets, NELEMS(gpio_binding_sets));
    dx_timerSetStart(timerSet, NELEMS(timerSet));

    // Initialize Intercore Communications for core one
    dx_intercoreConnect(&intercore_app_one);
}

/// <summary>
/// Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralAndHandlers(void)
{
    dx_timerSetStop(timerSet, NELEMS(timerSet));
    dx_timerEventLoopStop();
}

int main(int argc, char *argv[])
{
    dx_registerTerminationHandler();
    InitPeripheralAndHandlers();

    // Main loop
    while (!dx_isTerminationRequired()) {
        int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == -1 && errno != EINTR) {
            dx_terminate(DX_ExitCode_Main_EventLoopFail);
        }
    }

    ClosePeripheralAndHandlers();

    Log_Debug("Application exiting.\n");
    return dx_getTerminationExitCode();
}