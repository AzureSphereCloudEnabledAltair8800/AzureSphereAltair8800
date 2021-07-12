/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "iotc_manager.h"

static const char* msgTemplate = "{ \"Temperature\":%3.2f }";
static const char* memoryTemplate = "{ \"TotalMemoryUsage\":%d,\"PeakUserModeMemoryUsage\":%d }";


/// <summary>
/// Device Twin Handler to set the desired temperature value
/// </summary>
void device_twin_set_cpu_state_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
#ifdef ENABLE_WEB_TERMINAL
	if (is_mqtt_connected()) {
		cpu_operating_mode = *(bool*)deviceTwinBinding->twinState ? CPU_RUNNING : CPU_STOPPED;
	}
#else
	cpu_operating_mode = *(bool*)deviceTwinBinding->twinState ? CPU_RUNNING : CPU_STOPPED;
#endif // ENABLE_WEB_TERMINAL
	dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
}

void device_twin_set_local_serial_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	local_serial = *(bool*)deviceTwinBinding->twinState;
	dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
}

/// <summary>
/// Device Twin Handler to set the brightness of MAX7219 8x8 LED panel8x8
/// </summary>
void device_twin_set_led_brightness_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	if (*(int*)deviceTwinBinding->twinState < 0 || (*(int*)deviceTwinBinding->twinState) > 15) {
		dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_ERROR);
	} else {
#ifdef ALTAIR_FRONT_PANEL_CLICK
		max7219_set_brightness(&panel8x8, (unsigned char)*(int*)deviceTwinBinding->twinState);
#endif // ALTAIR_FRONT_PANEL_CLICK
		dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
	}
}

/// <summary>
/// Device Twin Handler to set the mqtt channel id
/// </summary>
void device_twin_set_channel_id_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	write_channel_id_to_storage(*(int*)deviceTwinBinding->twinState);
	dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
}

/// <summary>
/// Report back to IoT Central device memory usage
/// </summary>
/// <param name="eventLoopTimer"></param>
void memory_diagnostics_handler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	snprintf(msgBuffer, sizeof(msgBuffer), memoryTemplate,
		Applications_GetTotalMemoryUsageInKB(),
		Applications_GetPeakUserModeMemoryUsageInKB()
	);
	dx_azurePublish(msgBuffer, strlen(msgBuffer), NULL, 0, NULL);
}

void publish_telemetry(float temperature) {
	if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, temperature) > 0) {
		//Log_Debug("%s\n", msgBuffer);
		dx_azurePublish(msgBuffer, strlen(msgBuffer), NULL, 0, NULL);
	}
}

/// <summary>
/// Restart the Device
/// </summary>
void delay_restart_device_handler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	PowerManagement_ForceSystemReboot();
}

/// <summary>
/// Start Device Power Restart Direct Method
/// </summary>
DX_DIRECT_METHOD_RESPONSE_CODE RestartDeviceHandler(JSON_Value* json, DX_DIRECT_METHOD_BINDING* directMethodBinding, char** responseMsg) {
	*responseMsg = NULL;
	dx_timerOneShotSet(&restartDeviceOneShotTimer, &(struct timespec){2, 0});  // restart the device in 2 seconds
	return DX_METHOD_SUCCEEDED;
}