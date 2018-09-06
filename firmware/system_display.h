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


#ifndef SYSTEM_DISPLAY_H_
#define SYSTEM_DISPLAY_H_
#include "ch.h"
#include "hal.h"
#include "settings.h"
#include <stdlib.h>

void display_set_value(const uint8_t digit, char value);
void display_set_segment(uint8_t digit, uint8_t segment, bool enabled);
void system_display_init(void);
void display_update_brightness(void);

#endif /* SYSTEM_LED_H_ */
