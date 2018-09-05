/*
* ShiftX2 firmware
*
* Copyright (C) 2016 Autosport Labs
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


#ifndef SYSTEM_LED_H_
#define SYSTEM_LED_H_
#include "ch.h"
#include "hal.h"
#include "settings.h"
#include <stdlib.h>
#define min(X,Y) ((X) < (Y) ? (X) : (Y))
#define max(X,Y) ((X) > (Y) ? (X) : (Y))

/* Total number of LEDs in device */
#define LED_COUNT SETTINGS_LED_COUNT

/* Configuration of linear graph */
#define LINEAR_GRAPH_OFFSET SETTINGS_LINEAR_GRAPH_OFFSET
#define LINEAR_GRAPH_COUNT SETTINGS_LINEAR_GRAPH_COUNT

/* Configuration of alert indicators */
#define ALERT_OFFSET SETTINGS_ALERT_OFFSET
#define ALERT_COUNT SETTINGS_ALERT_COUNT

#define APA102_MIN_BRIGHTNESS 1
#define APA102_MAX_BRIGHTNESS 31

#define APA102_START_FRAME_BYTES 4
#define APA102_LED_DATA_START APA102_START_FRAME_BYTES
#define APA102_BYTES_PER_LED 4
#define APA102_END_FRAME_BYTES 4
#define APA102_LED_DATA_BYTES (APA102_BYTES_PER_LED * LED_COUNT)
#define APA102_GLOBAL_PREAMBLE 0xE0
#define TXBUF_LEN APA102_START_FRAME_BYTES + APA102_LED_DATA_BYTES + APA102_END_FRAME_BYTES
#define APA102_DEFAULT_BRIGHTNESS 0

void set_led(size_t index, uint8_t red, uint8_t green, uint8_t blue);
void set_led_brightness(size_t index, uint8_t brightness);

void led_worker(void);
void led_flash_worker(void);

void startup_demo_worker(void);
#endif /* SYSTEM_LED_H_ */
