/*
 * ShiftX3 firmware
 *
 * Copyright (C) 2018 Autosport Labs
 *
 * This file is part of the Race Capture firmware suite
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should
 * have received a copy of the GNU General Public License along with
 * this code. If not, see <http://www.gnu.org/licenses/>.
 */

#include "system_CAN.h"
#include "logging.h"
#include "system_serial.h"
#include "settings.h"
#include "shiftx3_api.h"
#include "system.h"
#include "stm32f042x6.h"

#define _LOG_PFX "SYS_CAN:     "

#define CAN_WORKER_STARTUP_DELAY 500
#define ADR1_ADDRESS_PORT 0
#define ADR2_BAUD_PORT 4
static uint32_t g_can_base_address = SHIFTX3_CAN_BASE_ID;

/*
 * 500K baud; 36MHz clock
 */
static const CANConfig cancfg_500K = {
    CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP | CAN_MCR_NART,
    CAN_BTR_SJW(1) |
    CAN_BTR_TS1(11) | CAN_BTR_TS2(2) | CAN_BTR_BRP(5)
};

/*
 * 1M baud; 36MHz clock
 */
static const CANConfig cancfg_1MB = {
    CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP | CAN_MCR_NART,
    CAN_BTR_SJW(1) |
    CAN_BTR_TS1(11) | CAN_BTR_TS2(2) | CAN_BTR_BRP(2)
};

static const CANConfig * _select_can_configuration(void)
{
    return palReadPad(GPIOA, ADR2_BAUD_PORT) == PAL_HIGH ? &cancfg_500K : &cancfg_1MB;
}
/*
 * Initialize our CAN peripheral
 */
static void init_can_gpio(void)
{
    /* CAN RX.       */
    palSetPadMode(GPIOA, 11, PAL_STM32_MODE_ALTERNATE | PAL_STM32_ALTERNATE(4));
    /* CAN TX.       */
    palSetPadMode(GPIOA, 12, PAL_STM32_MODE_ALTERNATE | PAL_STM32_ALTERNATE(4));

    /* Activates the CAN driver */
    canStart(&CAND1, _select_can_configuration());

    // Disable CAN filtering for now until we can verify proper operation / settings.
    // CANFilter shiftx3_can_filter = {1, 0, 1, 0, 0x000E3700, 0x1FFFFF00}; // g_can_base_address, SHIFTX3_CAN_FILTER_MASK
    //canSTM32SetFilters(1, 1, &shiftx3_can_filter);

}

/*
 * Initialize our runtime parameters
 */
static void init_can_operating_parameters(void)
{
    /* Init CAN jumper GPIOs for determining base address offset */
    palSetPadMode(GPIOA, ADR1_ADDRESS_PORT, PAL_STM32_MODE_INPUT | PAL_STM32_PUPDR_PULLUP);
    palSetPadMode(GPIOA, ADR2_BAUD_PORT, PAL_STM32_MODE_INPUT | PAL_STM32_PUPDR_PULLUP);

    if (palReadPad(GPIOA, ADR1_ADDRESS_PORT) == PAL_HIGH) {
        g_can_base_address += SHIFTX3_CAN_API_RANGE;
    }
}

void system_can_init(void)
{
    init_can_operating_parameters();
    init_can_gpio();
}

/*
 * Dispatch an incoming CAN message
 */
static bool dispatch_can_rx(CANRxFrame *rx_msg)
{
    int32_t can_id = rx_msg->IDE == CAN_IDE_EXT ? rx_msg->EID : rx_msg->SID;
    bool got_config_message = false;
    switch (can_id - g_can_base_address) {
    case API_SET_CONFIG_GROUP_1:
        api_set_config_group_1(rx_msg);
        got_config_message = true;
        break;
    case API_SET_DISCRETE_LED:
        api_set_discrete_led(rx_msg);
        break;
    case API_SET_ALERT_LED:
        api_set_alert_led(rx_msg);
        break;
    case API_SET_ALERT_THRESHOLD:
        api_set_alert_threshold(rx_msg);
        got_config_message = true;
        break;
    case API_SET_CURRENT_ALERT_VALUE:
        api_set_current_alert_value(rx_msg);
        break;
    case API_CONFIG_LINEAR_GRAPH:
        api_config_linear_graph(rx_msg);
        got_config_message = true;
        break;
    case API_SET_LINEAR_THRESHOLD:
        api_set_linear_threshold(rx_msg);
        got_config_message = true;
        break;
    case API_SET_CURRENT_LINEAR_GRAPH_VALUE:
        api_set_current_linear_graph_value(rx_msg);
        break;
    case API_SET_DISPLAY_VALUE:
        api_set_display_value(rx_msg);
        break;
    case API_SET_DISPLAY_SEGMENT:
        api_set_display_segment(rx_msg);
        break;
    default:
        return false;
    }
    /* if we received a configuration message then we are provisioned */
    if (got_config_message)
        set_api_is_provisioned(got_config_message);
    return true;
}

uint32_t get_can_base_id(void)
{
    return g_can_base_address;
}

/* Main worker for receiving CAN messages */
void can_worker(void)
{
    event_listener_t el;
    CANRxFrame rx_msg;
    chRegSetThreadName("CAN receiver");
    chEvtRegister(&CAND1.rxfull_event, &el, 0);

    chThdSleepMilliseconds(CAN_WORKER_STARTUP_DELAY);
    log_info(_LOG_PFX "CAN base address: %u\r\n", g_can_base_address);

    api_send_announcement();

    systime_t last_message = chVTGetSystemTimeX();

    while(!chThdShouldTerminateX()) {
        /* check if we've timed out after we've been active */
        if (api_is_provisoned() && chVTTimeElapsedSinceX(last_message) > MS2ST(NO_ACTIVITY_TIMEOUT)) {
            log_info(_LOG_PFX "No activity after %u ms, resetting system\r\n", NO_ACTIVITY_TIMEOUT);
            reset_system();
        }

        if (chEvtWaitAnyTimeout(ALL_EVENTS, MS2ST(1000)) == 0) {
            /* continue to send announcements until we are provisioned */
            if (!api_is_provisoned())
                api_send_announcement();
            continue;
        }
        while (canReceive(&CAND1, CAN_ANY_MAILBOX, &rx_msg, TIME_IMMEDIATE) == MSG_OK) {
            /* Process message.*/
            log_CAN_rx_message(_LOG_PFX, &rx_msg);
            if (dispatch_can_rx(&rx_msg)) {
                last_message = chVTGetSystemTime();
            }
        }
    }
    chEvtUnregister(&CAND1.rxfull_event, &el);
}

/* Prepare a CAN message with the specified CAN ID and type */
void prepare_can_tx_message(CANTxFrame *tx_frame, uint8_t can_id_type, uint32_t can_id)
{
    tx_frame->IDE = can_id_type;
    if (can_id_type == CAN_IDE_EXT) {
        tx_frame->EID = can_id;
    } else {
        tx_frame->SID = can_id;
    }
    tx_frame->RTR = CAN_RTR_DATA;
    tx_frame->DLC = 8;
    tx_frame->data8[0] = 0x55;
    tx_frame->data8[1] = 0x55;
    tx_frame->data8[2] = 0x55;
    tx_frame->data8[3] = 0x55;
    tx_frame->data8[4] = 0x55;
    tx_frame->data8[5] = 0x55;
    tx_frame->data8[6] = 0x55;
    tx_frame->data8[7] = 0x55;
}
