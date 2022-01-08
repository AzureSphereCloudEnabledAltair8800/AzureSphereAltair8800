#pragma once

// DevX Libraries
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_gpio.h"
#include "dx_i2c.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_intercore.h"
#include "dx_version.h"
#include "../IntercoreContract/intercore_contract.h"
#include "onboard_sensors.h"

#ifdef ALTAIR_FRONT_PANEL_CLICK
#include "front_panel_click.h"
#endif

#ifdef ALTAIR_FRONT_PANEL_RETRO_CLICK
#include "front_panel_retro_click.h"
#endif

#ifdef ALTAIR_FRONT_PANEL_KIT
#include "front_panel_kit.h"
#endif // ALTAIR_FRONT_PANEL_KIT

#ifdef ALTAIR_FRONT_PANEL_NONE
#include "front_panel_none.h"
#endif // ALTAIR_FRONT_PANEL_NONE

#define CORE_ENVIRONMENT_COMPONENT_ID "2e319eae-7be5-4a0c-ba47-9353aa6ca96a"
#define CORE_DISK_CACHE_COMPONENT_ID "9b684af8-21b9-42aa-91e4-621d5428e497"
#define CORE_SD_CARD_COMPONENT_ID "005180bc-402f-4cb3-a662-72937dbcde47"

typedef struct timespec timespec;

static DX_DECLARE_DEVICE_TWIN_HANDLER(device_twin_set_temperature_handler);
static DX_DECLARE_TIMER_HANDLER(connection_status_led_off_handler);
static DX_DECLARE_TIMER_HANDLER(connection_status_led_on_handler);
static DX_DECLARE_TIMER_HANDLER(device_stats_handler);
static DX_DECLARE_TIMER_HANDLER(measure_sensor_handler);
static DX_DECLARE_TIMER_HANDLER(mqtt_work_scheduler_handler);
static DX_DECLARE_TIMER_HANDLER(panel_refresh_handler);
static DX_DECLARE_TIMER_HANDLER(WatchdogMonitorTimerHandler);
static void* altair_thread(void* arg);
static void process_control_panel_commands(void);

const uint8_t reverse_lut[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

typedef enum { HVAC_MODE_UNKNOWN, HVAC_MODE_HEATING, HVAC_MODE_GREEN, HVAC_MODE_COOLING } HVAC_OPERATING_MODE;

typedef struct {
    ONBOARD_TELEMETRY latest;
    ONBOARD_TELEMETRY previous;
    bool updated;
    HVAC_OPERATING_MODE latest_operating_mode;
    HVAC_OPERATING_MODE previous_operating_mode;
} ENVIRONMENT;

ENVIRONMENT onboard_telemetry;

INTERCORE_DISK_DATA_BLOCK_T intercore_disk_block;

DX_INTERCORE_BINDING intercore_disk_cache_ctx = {.sockFd = -1,
                                                 .nonblocking_io = true,
                                                 .rtAppComponentId = CORE_DISK_CACHE_COMPONENT_ID,
                                                 .interCoreCallback = NULL,
                                                 .intercore_recv_block = &intercore_disk_block,
                                                 .intercore_recv_block_length = sizeof(intercore_disk_block)};

DX_INTERCORE_BINDING intercore_sd_card_ctx = {.sockFd = -1,
                                              .nonblocking_io = false,
                                              .rtAppComponentId = CORE_SD_CARD_COMPONENT_ID,
                                              .interCoreCallback = NULL,
                                              .intercore_recv_block = &intercore_disk_block,
                                              .intercore_recv_block_length = sizeof(intercore_disk_block)};

#ifdef ALTAIR_FRONT_PANEL_CLICK

CLICK_4X4_BUTTON_MODE click_4x4_key_mode = CONTROL_MODE;

matrix8x8_t panel8x8 = {.interfaceId = MT3620_ISU1_SPI, .chipSelectId = MT3620_SPI_CS_B, .busSpeed = 10000000, .handle = -1, .bitmap = {0}};

key4x4_t key4x4 = {.interfaceId = MT3620_ISU1_SPI, .chipSelectId = MT3620_SPI_CS_A, .busSpeed = 10000000, .handle = -1, .bitmap = 0, .debouncePeriodMilliseconds = 500};

DX_GPIO_BINDING buttonB = {.pin = BUTTON_B, .direction = DX_INPUT, .detect = DX_GPIO_DETECT_LOW, .name = "buttonB"};

// turn off notifications
DX_TIMER_BINDING turnOffNotificationsTimer = {.period = {0, 0}, .name = "turnOffNotificationsTimer", .handler = turn_off_notifications_handler};

#endif //  ALTAIR_FRONT_PANEL_CLICK

#ifdef ALTAIR_FRONT_PANEL_RETRO_CLICK

CLICK_4X4_BUTTON_MODE click_4x4_key_mode = CONTROL_MODE;
DX_GPIO_BINDING buttonB = {.pin = BUTTON_B, .direction = DX_INPUT, .detect = DX_GPIO_DETECT_LOW, .name = "buttonB"};
as1115_t retro_click = {.interfaceId = ISU2, .handle = -1, .bitmap64 = 0, .keymap = 0, .debouncePeriodMilliseconds = 500};

// turn off notifications
DX_TIMER_BINDING turnOffNotificationsTimer = {.period = {0, 0}, .name = "turnOffNotificationsTimer", .handler = turn_off_notifications_handler};

#endif //  ALTAIR_FRONT_PANEL_RETRO_CLICK

#ifdef ALTAIR_FRONT_PANEL_KIT

// static DX_GPIO memoryCS = { .pin = MEMORY_CS, .direction = DX_OUTPUT, .initialState =
// GPIO_Value_High, .invertPin = false, .name = "memory CS" }; static DX_GPIO sdCS = { .pin = SD_CS,
// .direction = DX_OUTPUT, .initialState = GPIO_Value_High, .invertPin = false, .name = "SD_CS" };

DX_GPIO_BINDING switches_chip_select = {.pin = SWITCHES_CHIP_SELECT, .direction = DX_OUTPUT, .initialState = GPIO_Value_High, .invertPin = false, .name = "switches CS"};

DX_GPIO_BINDING switches_load = {.pin = SWITCHES_LOAD, .direction = DX_OUTPUT, .initialState = GPIO_Value_High, .invertPin = false, .name = "switchs Load"};

DX_GPIO_BINDING led_store = {.pin = LED_STORE, .direction = DX_OUTPUT, .initialState = GPIO_Value_High, .invertPin = false, .name = "LED store"};

static DX_GPIO_BINDING led_master_reset = {.pin = LED_MASTER_RESET, .direction = DX_OUTPUT, .initialState = GPIO_Value_High, .invertPin = false, .name = "LED master reset"};

static DX_GPIO_BINDING led_output_enable = {
    .pin = LED_OUTPUT_ENABLE, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = false, .name = "LED output enable"}; // set OE initial state low

#endif // ALTAIR_FRONT_PANEL_KIT

static DX_GPIO_BINDING buttonA = {.pin = BUTTON_A, .direction = DX_INPUT, .detect = DX_GPIO_DETECT_LOW, .name = "buttonA"};
static DX_GPIO_BINDING azure_iot_connected_led = {
    .pin = AZURE_CONNECTED_LED, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "azure_iot_connected_led"};

// Common Timers
DX_TIMER_BINDING tmr_restart_device = {.name = "tmr_restart_device", .handler = delay_restart_device_handler};
static DX_TIMER_BINDING tmr_connection_status_led_off = {.name = "tmr_connection_status_led_off", .handler = connection_status_led_off_handler};
static DX_TIMER_BINDING tmr_connection_status_led_on = {.name = "tmr_connection_status_led_on", .handler = connection_status_led_on_handler};
static DX_TIMER_BINDING tmr_measure_sensor = {.repeat = &(timespec){30, 0}, .name = "tmr_measure_sensor", .handler = measure_sensor_handler};
static DX_TIMER_BINDING tmr_memory_diagnostics = {.repeat = &(timespec){60, 0}, .name = "tmr_memory_diagnostics", .handler = memory_diagnostics_handler};
static DX_TIMER_BINDING tmr_device_stats = {.repeat = &(timespec){45, 0}, .name = "tmr_device_stats", .handler = device_stats_handler};
static DX_TIMER_BINDING tmr_mqtt_do_work = {.repeat = &(timespec){0, 250 * ONE_MS}, .name = "tmr_mqtt_do_work", .handler = mqtt_work_scheduler_handler};
static DX_TIMER_BINDING tmr_panel_refresh = {.repeat = &(timespec){0, 20 * OneMS}, .name = "tmr_panel_refresh", .handler = panel_refresh_handler};
static DX_TIMER_BINDING tmr_watchdog_monitor = {.repeat = &(timespec){5, 0}, .name = "tmr_watchdog_monitor", .handler = WatchdogMonitorTimerHandler};

// Azure IoT Central Properties (Device Twins)
DX_DEVICE_TWIN_BINDING dt_channelId = {.propertyName = "DesiredChannelId", .twinType = DX_DEVICE_TWIN_INT, .handler = device_twin_set_channel_id_handler};
DX_DEVICE_TWIN_BINDING dt_diskCacheHits = {.propertyName = "DiskCacheHits", .twinType = DX_DEVICE_TWIN_INT};
DX_DEVICE_TWIN_BINDING dt_diskCacheMisses = {.propertyName = "DiskCacheMisses", .twinType = DX_DEVICE_TWIN_INT};
DX_DEVICE_TWIN_BINDING dt_diskTotalErrors = {.propertyName = "DiskTotalErrors", .twinType = DX_DEVICE_TWIN_INT};
DX_DEVICE_TWIN_BINDING dt_diskTotalWrites = {.propertyName = "DiskTotalWrites", .twinType = DX_DEVICE_TWIN_INT};
static DX_DEVICE_TWIN_BINDING dt_desiredCpuState = {.propertyName = "DesiredCpuState", .twinType = DX_DEVICE_TWIN_BOOL, .handler = device_twin_set_cpu_state_handler};
static DX_DEVICE_TWIN_BINDING dt_desiredLedBrightness = {.propertyName = "DesiredLedBrightness", .twinType = DX_DEVICE_TWIN_INT, .handler = device_twin_set_led_brightness_handler};
static DX_DEVICE_TWIN_BINDING dt_desiredLocalSerial = {.propertyName = "DesiredLocalSerial", .twinType = DX_DEVICE_TWIN_BOOL, .handler = device_twin_set_local_serial_handler};
static DX_DEVICE_TWIN_BINDING dt_desiredTemperature = {.propertyName = "DesiredTemperature", .twinType = DX_DEVICE_TWIN_INT, .handler = device_twin_set_temperature_handler};
static DX_DEVICE_TWIN_BINDING dt_reportedDeviceStartTime = {.propertyName = "ReportedDeviceStartTime", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_reportedTemperature = {.propertyName = "ReportedTemperature", .twinType = DX_DEVICE_TWIN_INT};
static DX_DEVICE_TWIN_BINDING dt_softwareVersion = {.propertyName = "SoftwareVersion", .twinType = DX_DEVICE_TWIN_STRING};

// Azure IoT Central Commands (Direct Methods)
static DX_DIRECT_METHOD_BINDING dm_restartDevice = {.methodName = "RestartDevice", .handler = RestartDeviceHandler};

// Initialize Sets
static DX_GPIO_BINDING *ledRgb[] = {&(DX_GPIO_BINDING){.pin = LED_RED, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "red led"},
                                    &(DX_GPIO_BINDING){.pin = LED_GREEN, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "green led"},
                                    &(DX_GPIO_BINDING){.pin = LED_BLUE, .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true, .name = "blue led"}};


static DX_GPIO_BINDING *gpioSet[] = {&azure_iot_connected_led,
                                     &buttonA
#if defined(ALTAIR_FRONT_PANEL_CLICK) || defined(ALTAIR_FRONT_PANEL_RETRO_CLICK)
                                     ,
                                     &buttonB
#endif // ALTAIR_FRONT_PANEL_CLICK

#ifdef ALTAIR_FRONT_PANEL_KIT
                                     ,
                                     &switches_load,
                                     &switches_chip_select,
                                     &led_master_reset,
                                     &led_store,
                                     &led_output_enable
//&memoryCS, &sdCS
#endif // ALTAIR_FRONT_PANEL_KIT
};

#ifdef ALTAIR_FRONT_PANEL_RETRO_CLICK
    DX_I2C_BINDING i2c_as1115_retro = {.interfaceId = ISU2, .speedInHz = I2C_BUS_SPEED_FAST_PLUS, .name = "i2c_as1115_retro"};
    DX_I2C_BINDING i2c_onboard_sensors = {.interfaceId = ISU2, .speedInHz = I2C_BUS_SPEED_FAST_PLUS, .name = "i2c_onboard_sensors"};
    static DX_I2C_BINDING *i2c_bindings[] = { &i2c_as1115_retro, &i2c_onboard_sensors };
#else
    #ifdef OEM_AVNET
        DX_I2C_BINDING i2c_onboard_sensors = {.interfaceId = ISU2, .speedInHz = I2C_BUS_SPEED_FAST_PLUS, .name = "i2c_onboard_sensors"};
        static DX_I2C_BINDING *i2c_bindings[] = { &i2c_onboard_sensors };
    #else
        static DX_I2C_BINDING *i2c_bindings[] = {};
    #endif
#endif // ALTAIR_FRONT_PANEL_RETRO_CLICK

static DX_TIMER_BINDING *timerSet[] = {&tmr_connection_status_led_on,
                                       &tmr_connection_status_led_off,
                                       &tmr_memory_diagnostics,
                                       &tmr_device_stats,
                                       &tmr_measure_sensor,
                                       &tmr_restart_device,
                                       &tmr_mqtt_do_work,
                                       &tmr_panel_refresh,
                                       &tmr_watchdog_monitor
#if defined(ALTAIR_FRONT_PANEL_CLICK) || defined(ALTAIR_FRONT_PANEL_RETRO_CLICK)
                                       ,
                                       &turnOffNotificationsTimer
#endif // ALTAIR_FRONT_PANEL_CLICK
};

static DX_DEVICE_TWIN_BINDING *deviceTwinBindingSet[] = {&dt_reportedDeviceStartTime, &dt_channelId,          &dt_desiredCpuState,     &dt_desiredLedBrightness,
                                                         &dt_desiredLocalSerial,      &dt_desiredTemperature, &dt_reportedTemperature, &dt_diskCacheHits,
                                                         &dt_diskCacheMisses,         &dt_diskTotalWrites,    &dt_diskTotalErrors};

DX_DIRECT_METHOD_BINDING *directMethodBindingSet[] = {&dm_restartDevice};