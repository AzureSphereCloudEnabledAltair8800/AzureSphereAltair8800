#include "intercore.h"
#include "intercore_contract.h"

#if defined(OEM_AVNET)
#include "IMU_lib/imu_temp_pressure.h"
#endif 

#include "hw/azure_sphere_learning_path.h"
#include "utils.h"

#include "os_hal_uart.h"
#include "os_hal_gpio.h"
#include "os_hal_gpt.h"
#include "nvic.h"


#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum LEDS { RED, GREEN, BLUE, UNKNOWN };
static enum LEDS current_led = RED;
os_hal_gpio_pin led_rgb[] = { LED_RED, LED_GREEN, LED_BLUE };

// static int32_t previous_temperature = INT32_MAX;
static int32_t desired_temperature = 0;
static bool desired_temperature_updated = false;

INTERCORE_ENVIRONMENT_DATA_BLOCK_T ic_outbound_data;
INTERCORE_ENVIRONMENT_DATA_BLOCK_T *ic_inbound_data;


uint8_t mbox_local_buf[MBOX_BUFFER_LEN_MAX];
BufferHeader* outbound, * inbound;
volatile u8  blockDeqSema;
volatile u8  blockFifoSema;
volatile bool refresh_data_trigger;

struct os_gpt_int gpt0_int;
struct os_gpt_int gpt3_int;

/* Bitmap for IRQ enable. bit_0 and bit_1 are used to communicate with HL_APP */
uint32_t mbox_irq_status = 0x3;
size_t payloadStart = 20; /* UUID 16B, Reserved 4B */

u32 mbox_shared_buf_size = 0;

static const uint8_t uart_port_num = OS_HAL_UART_ISU3;

/******************************************************************************/
/* Timers */
/******************************************************************************/
static const uint8_t gpt_task_scheduler = OS_HAL_GPT0;
static const uint32_t gpt_task_scheduler_timer_val = 1; /* 1ms */

/******************************************************************************/
/* Applicaiton Hooks */
/******************************************************************************/
/* Hook for "printf". */
void _putchar(char character) {
    mtk_os_hal_uart_put_char(uart_port_num, character);
    if (character == '\n')
        mtk_os_hal_uart_put_char(uart_port_num, '\r');
}



bool initialize_hardware(void)
{
    mtk_os_hal_gpio_set_direction(LED_RED, OS_HAL_GPIO_DIR_OUTPUT);
    mtk_os_hal_gpio_set_direction(LED_GREEN, OS_HAL_GPIO_DIR_OUTPUT);
    mtk_os_hal_gpio_set_direction(LED_BLUE, OS_HAL_GPIO_DIR_OUTPUT);

    mtk_os_hal_gpio_set_output(LED_RED, OS_HAL_GPIO_DATA_HIGH);
    mtk_os_hal_gpio_set_output(LED_GREEN, OS_HAL_GPIO_DATA_HIGH);
    mtk_os_hal_gpio_set_output(LED_BLUE, OS_HAL_GPIO_DATA_HIGH);

#if defined(OEM_AVNET)

    bool status = (lp_imu_initialize());
    ic_outbound_data.ic_msg_type = ALTAIR_IC_ENVIRONMENT;

    if (status) {
        // Prime the temperature and humidity sensors
        // Observed the first few readings on startup may return NaN
        for (size_t i = 0; i < 6; i++) {
            ic_outbound_data.environment.temperature = lp_get_temperature_lps22h();
            ic_outbound_data.environment.pressure = lp_get_pressure();

            if (!isnan(ic_outbound_data.environment.temperature) && !isnan(ic_outbound_data.environment.pressure)) {
                break;
            }
            // tx_thread_sleep(MS_TO_TICK(100));
            Gpt3_WaitUs(100000);
        }
    }
    return status;
}
#else
    return true;
}
#endif


static void send_intercore_msg(void *data, size_t length) {
    uint32_t dataSize;
    memcpy((void*)mbox_local_buf, &hlAppId, sizeof(hlAppId));	// copy high level appid to first 20 bytes
    memcpy(mbox_local_buf + payloadStart, data, length);
    dataSize = payloadStart + length;
    EnqueueData(inbound, outbound, mbox_shared_buf_size, mbox_local_buf, dataSize);
}

static inline bool in_range(int low, int high, int x) {
    return  low <= x && x <= high;
}

/// <summary>
/// Set the temperature status led. 
/// Red if HVAC needs to be turned on to get to desired temperature. 
/// Blue to turn on cooler. 
/// Green equals just right, no action required.
/// </summary>
void set_hvac_status_colour(int32_t temperature) {
    static enum LEDS previous_led = UNKNOWN;

    // No desired temperature device twin update to date so return 
    if (!desired_temperature_updated) { return; }

    current_led = in_range(desired_temperature - 1, desired_temperature + 1, temperature)
        ? GREEN : temperature > desired_temperature + 1 ? BLUE : RED;

    if (previous_led != current_led) {
        if (previous_led != UNKNOWN) {
            mtk_os_hal_gpio_set_output(led_rgb[(int)previous_led], OS_HAL_GPIO_DATA_HIGH); // turn off old current colour
        }
        previous_led = current_led;
    }
    mtk_os_hal_gpio_set_output(led_rgb[(int)previous_led], OS_HAL_GPIO_DATA_LOW);
}

static void process_inbound_message() {
    u32 mbox_local_buf_len;
    int result;

    mbox_local_buf_len = MBOX_BUFFER_LEN_MAX;
    result = DequeueData(outbound, inbound, mbox_shared_buf_size, mbox_local_buf, &mbox_local_buf_len);

    if (result == 0 && mbox_local_buf_len > payloadStart) {

        ic_inbound_data = (INTERCORE_ENVIRONMENT_DATA_BLOCK_T*)(mbox_local_buf + payloadStart);

        switch (ic_inbound_data->ic_msg_type) {

        case ALTAIR_IC_ENVIRONMENT:
            send_intercore_msg(&ic_outbound_data, sizeof(INTERCORE_ENVIRONMENT_DATA_BLOCK_T));
            break;
        case ALTAIR_IC_THERMOSTAT:
            if (in_range(-20, 80, ic_inbound_data->environment.desired_temperature)) {
                desired_temperature_updated = true;
                desired_temperature = ic_inbound_data->environment.desired_temperature;
            }
            break;
        default:
            break;
        }
    }
}

// sensor read
#if defined(OEM_AVNET)
static void refresh_data(void)
{
    ic_outbound_data.ic_msg_type = ALTAIR_IC_ENVIRONMENT;

    ic_outbound_data.environment.temperature = lp_get_temperature_lps22h();
    ic_outbound_data.environment.pressure = lp_get_pressure();

    set_hvac_status_colour((int32_t)ic_outbound_data.environment.temperature);
}
#else
void refresh_data(void)
{
    int rand_number;

    ic_outbound_data.ic_msg_type = ALTAIR_IC_ENVIRONMENT;

    rand_number = (rand() % 10);
    ic_outbound_data.environment.temperature = (float)(15.0 + rand_number);

    rand_number = (rand() % 100);
    ic_outbound_data.environment.pressure = (float)(950.0 + rand_number);

    set_hvac_status_colour((int32_t)ic_outbound_data.environment.temperature);
}
#endif

static void task_scheduler(void* cb_data) {
    static size_t refresh_data_tick_counter = SIZE_MAX;

    if (refresh_data_tick_counter++ >= 2000)  // 2 seconds
    {
        refresh_data_tick_counter = 0;
        refresh_data_trigger = true;
    }
}

_Noreturn void RTCoreMain(void) {
    

    /* Init Vector Table */
    NVIC_SetupVectorTable();

    srand((unsigned int)time(NULL)); // seed the random number generator for fake telemetry

    /* Init GPT */
    gpt0_int.gpt_cb_hdl = task_scheduler;
    gpt0_int.gpt_cb_data = NULL;
    mtk_os_hal_gpt_init();

    /* configure GPT0 clock speed (as 1KHz) */
	/* and register GPT0 user interrupt callback handle and user data. */
	mtk_os_hal_gpt_config(gpt_task_scheduler, false, &gpt0_int);

	/* configure GPT0 timeout as 1ms and repeat mode. */
	mtk_os_hal_gpt_reset_timer(gpt_task_scheduler, gpt_task_scheduler_timer_val, true);



    // gpt3_int.gpt_cb_hdl = TimerHandlerGpt3;
	// gpt3_int.gpt_cb_data = NULL;
    // mtk_os_hal_gpt_config(gpt_timer_button, false, &gpt3_int);

    initialise_intercore_comms();
    initialize_hardware();

    /* start timer */
	mtk_os_hal_gpt_start(gpt_task_scheduler);

    for (;;) {

        if (blockDeqSema > 0) {
            process_inbound_message();
        }

        if (refresh_data_trigger) {
            refresh_data_trigger = false;
            refresh_data();
        }
    }
}
