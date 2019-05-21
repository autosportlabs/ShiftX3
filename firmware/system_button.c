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

#include "system_button.h"
#include "logging.h"
#define _LOG_PFX "BUTTON:      "
#include "system_CAN.h"
#include "shiftx3_api.h"
#include "settings.h"

#define LEFT_BUTTON_PORT 8
#define RIGHT_BUTTON_PORT 7


#define LEFT_NAV_BUTTON 0
#define RIGHT_NAV_BUTTON 1
#define BUTTON_COUNT 2
static bool g_button_states[BUTTON_COUNT];

void button_init(void)
{
    /* Init CAN jumper GPIOs for determining base address offset */
    palSetPadMode(GPIOB, LEFT_BUTTON_PORT, PAL_STM32_MODE_INPUT | PAL_STM32_PUPDR_PULLDOWN);
    palSetPadMode(GPIOB, RIGHT_BUTTON_PORT, PAL_STM32_MODE_INPUT | PAL_STM32_PUPDR_PULLUP);
    log_info(_LOG_PFX "buttons init\r\n");
}

static void _broadcast_button_state(uint8_t button_id, bool pressed)
{
    CANTxFrame can_stats;
    prepare_can_tx_message(&can_stats, CAN_IDE_EXT, get_can_base_id() + API_ALERT_BUTTON_STATES);

    uint8_t report_id = (get_orientation() == DISPLAY_BOTTOM) ? button_id : BUTTON_COUNT - button_id - 1;
    /* these values reserved for future use */
    can_stats.data8[0] = pressed;
    can_stats.data8[1] = report_id;
    can_stats.DLC = 2;
    canTransmit(&CAND1, CAN_ANY_MAILBOX, &can_stats, MS2ST(CAN_TRANSMIT_TIMEOUT));
    log_trace(_LOG_PFX "Broadcast button_states\r\n");
}

static bool _button_is_pressed(size_t button_id)
{
    switch (button_id) {
    case 0:
        return palReadPad(GPIOB, LEFT_BUTTON_PORT) == PAL_HIGH;
    case 1:
        return palReadPad(GPIOB, RIGHT_BUTTON_PORT) == PAL_LOW;
    default:
        log_info(_LOG_PFX "invalid button requested: %d\r\n", button_id);
        return false;
    }
}

void button_check_broadcast_state(void)
{
    for (size_t i = 0; i < BUTTON_COUNT; i++) {
        bool is_pressed = _button_is_pressed(i);
        if (is_pressed != g_button_states[i]) {
            log_info(_LOG_PFX "button state: %d = %d\r\n", i, is_pressed);
            _broadcast_button_state(i, is_pressed);
            g_button_states[i] = is_pressed;
        }
    }
}



