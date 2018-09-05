/*
 * system_button.c
 *
 *  Created on: Feb 7, 2017
 *      Author: brent
 */

#include "system_button.h"
#include "logging.h"
#define _LOG_PFX "BUTTON:      "
#include "system_CAN.h"
#include "shiftx2_api.h"
#include "settings.h"

#define BUTTON_PORT 8

static bool g_button_is_pressed = false;

void button_init(void)
{
        /* Init CAN jumper GPIOs for determining base address offset */
        palSetPadMode(GPIOB, BUTTON_PORT, PAL_STM32_MODE_INPUT | PAL_STM32_PUPDR_PULLDOWN);
        log_info(_LOG_PFX "button init\r\n");
}

static void _broadcast_button_state(bool pressed)
{
        CANTxFrame can_stats;
        prepare_can_tx_message(&can_stats, CAN_IDE_EXT, get_can_base_id() + API_ALERT_BUTTON_STATES);

        /* these values reserved for future use */
        can_stats.data8[0] = pressed;
        can_stats.DLC = 1;
        canTransmit(&CAND1, CAN_ANY_MAILBOX, &can_stats, MS2ST(CAN_TRANSMIT_TIMEOUT));
        log_trace(_LOG_PFX "Broadcast button_states\r\n");
}

bool button_is_pressed(void)
{
        bool pressed = palReadPad(GPIOB, BUTTON_PORT) == PAL_HIGH;
        log_trace(_LOG_PFX "button state: %d\r\n", pressed);
        return pressed;
}

void button_check_broadcast_state(void)
{
        bool is_currently_pressed = button_is_pressed();
        if (is_currently_pressed != g_button_is_pressed) {
                _broadcast_button_state(is_currently_pressed);
        }
        g_button_is_pressed = is_currently_pressed;
}



