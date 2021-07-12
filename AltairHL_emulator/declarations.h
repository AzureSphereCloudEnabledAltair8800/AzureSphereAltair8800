#pragma once

// DevX Libraries
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_intercore.h"
#include "dx_version.h"
#include "../IntercoreContract/intercore_contract.h"

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

static bool loadRomImage(char *romImageName, uint16_t loadAddress);
static void *altair_thread(void *arg);
static void connection_status_led_off_handler(EventLoopTimer *eventLoopTimer);
static void connection_status_led_on_handler(EventLoopTimer *eventLoopTimer);
static void device_twin_set_temperature_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void intercore_disk_cache_receive_msg_handler(void *data_block, ssize_t message_length);
static void intercore_environment_receive_msg_handler(void *data_block, ssize_t message_length);
static void measure_sensor_handler(EventLoopTimer *eventLoopTimer);
static void mqtt_dowork_handler(EventLoopTimer *eventLoopTimer);
static void panel_refresh_handler(EventLoopTimer *eventLoopTimer);
static void process_control_panel_commands(void);
static void WatchdogMonitorTimerHandler(EventLoopTimer *eventLoopTimer);

const uint8_t reverse_lut[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

INTERCORE_ENVIRONMENT_DATA_BLOCK_T intercore_send_block;
INTERCORE_ENVIRONMENT_DATA_BLOCK_T intercore_recv_block;
INTERCORE_ENVIRONMENT_T current_environment;

INTERCORE_DISK_DATA_BLOCK_T intercore_disk_block;

DX_INTERCORE_BINDING intercore_environment_ctx = {.sockFd = -1,
                                                  .nonblocking_io = true,
                                                  .rtAppComponentId = CORE_ENVIRONMENT_COMPONENT_ID,
                                                  .interCoreCallback = intercore_environment_receive_msg_handler,
                                                  .intercore_recv_block = &intercore_recv_block,
                                                  .intercore_recv_block_length = sizeof(intercore_recv_block)};

DX_INTERCORE_BINDING intercore_disk_cache_ctx = {.sockFd = -1,
                                                 .nonblocking_io = true,
                                                 .rtAppComponentId = CORE_DISK_CACHE_COMPONENT_ID,
                                                 .interCoreCallback = intercore_disk_cache_receive_msg_handler,
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
DX_TIMER_BINDING restartDeviceOneShotTimer = {.period = {0, 0}, .name = "restartDeviceOneShotTimer", .handler = delay_restart_device_handler};
static DX_TIMER_BINDING connectionStatusLedOffTimer = {.period = {0, 0}, .name = "connectionStatusLedOffTimer", .handler = connection_status_led_off_handler};
static DX_TIMER_BINDING connectionStatusLedOnTimer = {.period = {0, 0}, .name = "connectionStatusLedOnTimer", .handler = connection_status_led_on_handler};
static DX_TIMER_BINDING measure_sensor_timer = {.period = {5, 0}, .name = "measure_sensor_timer", .handler = measure_sensor_handler};
static DX_TIMER_BINDING memory_diagnostics_timer = {.period = {60, 0}, .name = "memory_diagnostics_timer", .handler = memory_diagnostics_handler};
static DX_TIMER_BINDING mqtt_do_work_timer = {.period = {0, 300 * OneMS}, .name = "mqtt_do_work_timer", .handler = mqtt_dowork_handler};
static DX_TIMER_BINDING panel_refresh_timer = {.period = {0, 20 * OneMS}, .name = "panel_refresh_timer", .handler = panel_refresh_handler};
static DX_TIMER_BINDING watchdogMonitorTimer = {.period = {5, 0}, .name = "watchdogMonitorTimer", .handler = WatchdogMonitorTimerHandler};

// Azure IoT Central Properties (Device Twins)
DX_DEVICE_TWIN_BINDING dt_channelId = {.twinProperty = "DesiredChannelId", .twinType = DX_TYPE_INT, .handler = device_twin_set_channel_id_handler};
DX_DEVICE_TWIN_BINDING dt_diskCacheHits = {.twinProperty = "DiskCacheHits", .twinType = DX_TYPE_INT};
DX_DEVICE_TWIN_BINDING dt_diskCacheMisses = {.twinProperty = "DiskCacheMisses", .twinType = DX_TYPE_INT};
DX_DEVICE_TWIN_BINDING dt_diskTotalErrors = {.twinProperty = "DiskTotalErrors", .twinType = DX_TYPE_INT};
DX_DEVICE_TWIN_BINDING dt_diskTotalWrites = {.twinProperty = "DiskTotalWrites", .twinType = DX_TYPE_INT};
static DX_DEVICE_TWIN_BINDING dt_desiredCpuState = {.twinProperty = "DesiredCpuState", .twinType = DX_TYPE_BOOL, .handler = device_twin_set_cpu_state_handler};
static DX_DEVICE_TWIN_BINDING dt_desiredLedBrightness = {.twinProperty = "DesiredLedBrightness", .twinType = DX_TYPE_INT, .handler = device_twin_set_led_brightness_handler};
static DX_DEVICE_TWIN_BINDING dt_desiredLocalSerial = {.twinProperty = "DesiredLocalSerial", .twinType = DX_TYPE_BOOL, .handler = device_twin_set_local_serial_handler};
static DX_DEVICE_TWIN_BINDING dt_desiredTemperature = {.twinProperty = "DesiredTemperature", .twinType = DX_TYPE_INT, .handler = device_twin_set_temperature_handler};
static DX_DEVICE_TWIN_BINDING dt_reportedDeviceStartTime = {.twinProperty = "ReportedDeviceStartTime", .twinType = DX_TYPE_STRING};
static DX_DEVICE_TWIN_BINDING dt_reportedTemperature = {.twinProperty = "ReportedTemperature", .twinType = DX_TYPE_INT};
static DX_DEVICE_TWIN_BINDING dt_softwareVersion = {.twinProperty = "SoftwareVersion", .twinType = DX_TYPE_STRING};

// Azure IoT Central Commands (Direct Methods)
static DX_DIRECT_METHOD_BINDING dm_restartDevice = {.methodName = "RestartDevice", .handler = RestartDeviceHandler};

// Initialize Sets
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

static DX_TIMER_BINDING *timerSet[] = {&connectionStatusLedOnTimer,
                                       &connectionStatusLedOffTimer,
                                       &memory_diagnostics_timer,
                                       &measure_sensor_timer,
                                       &restartDeviceOneShotTimer,
                                       &mqtt_do_work_timer,
                                       &panel_refresh_timer,
                                       &watchdogMonitorTimer
#if defined(ALTAIR_FRONT_PANEL_CLICK) || defined(ALTAIR_FRONT_PANEL_RETRO_CLICK)
                                       ,
                                       &turnOffNotificationsTimer
#endif // ALTAIR_FRONT_PANEL_CLICK
};

static DX_DEVICE_TWIN_BINDING *deviceTwinBindingSet[] = {&dt_reportedDeviceStartTime, &dt_channelId,          &dt_desiredCpuState,     &dt_desiredLedBrightness,
                                                         &dt_desiredLocalSerial,      &dt_desiredTemperature, &dt_reportedTemperature, &dt_diskCacheHits,
                                                         &dt_diskCacheMisses,         &dt_diskTotalWrites,    &dt_diskTotalErrors};

DX_DIRECT_METHOD_BINDING *directMethodBindingSet[] = {&dm_restartDevice};