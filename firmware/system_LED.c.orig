/*
 * OBD2CAN firmware
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
#include "system_LED.h"
#include "system_SPI.h"
#include "logging.h"
#include "shiftx2_api.h"
#include "system_ADC.h"

#define _LOG_PFX "LED:     "

#define DEMO_DURATION_MS 30000
/*
 * LED buffer
 */
static uint8_t txbuf[TXBUF_LEN];

static void _init_leds(uint8_t default_brightness, uint8_t default_red, uint8_t default_green, uint8_t default_blue)
{
        size_t i;
        /* initialize start frame */
        for (i = 0; i < 4; i++) {
                txbuf[i] = 0x00;
        }
        /* initialize led data */
        for (i = 0; i < APA102_LED_DATA_BYTES ; i+=4) {
                txbuf[APA102_LED_DATA_START + i] = APA102_GLOBAL_PREAMBLE + default_brightness;
                txbuf[APA102_LED_DATA_START + i + 1] = default_blue;
                txbuf[APA102_LED_DATA_START + i + 2] = default_green;
                txbuf[APA102_LED_DATA_START + i + 3] = default_red;
        }
        /* initialize end frame */
        for (i=0; i < 4; i++) {
                txbuf[TXBUF_LEN - 1 - i] = 0xFF;
        }
}

void set_led(size_t index, uint8_t red, uint8_t green, uint8_t blue)
{
        if (index > LED_COUNT)
                return;
        txbuf[APA102_LED_DATA_START + (APA102_BYTES_PER_LED * index) + 1] = blue;
        txbuf[APA102_LED_DATA_START + (APA102_BYTES_PER_LED * index) + 2] = green;
        txbuf[APA102_LED_DATA_START + (APA102_BYTES_PER_LED * index) + 3] = red;
}

void set_led_brightness(size_t index, uint8_t brightness)
{
        if (index > LED_COUNT)
                return;
        brightness = brightness > APA102_MAX_BRIGHTNESS ? APA102_MAX_BRIGHTNESS : brightness;
        txbuf[APA102_LED_DATA_START + (APA102_BYTES_PER_LED * index)] = APA102_GLOBAL_PREAMBLE + brightness;
}

void led_worker(void)
{
        log_info(_LOG_PFX "Starting LED worker\r\n");
        chRegSetThreadName("LED worker");
        api_initialize();
        spi_init();
        _init_leds(APA102_DEFAULT_BRIGHTNESS, 0x00, 0x00, 0x00);
        while(!chThdShouldTerminateX()) {
                spi_send_buffer(txbuf, sizeof(txbuf));
                chThdSleepMilliseconds(1);
        }
}

uint8_t _calculate_auto_brightness(void)
{
        uint16_t light_sensor = system_adc_sample();
        uint8_t scaling = get_light_sensor_scaling();
        uint16_t brightness = light_sensor * scaling / 100;
        brightness = brightness > APA102_MAX_BRIGHTNESS ? APA102_MAX_BRIGHTNESS : brightness;
        brightness = brightness < APA102_MIN_BRIGHTNESS ? APA102_MIN_BRIGHTNESS : brightness;
        log_trace(_LOG_PFX "Auto brightness: Sensor ADC/scaling/brightness %d/%d/%d", light_sensor, scaling, brightness);
        return (uint8_t)brightness;
}

/* Main worker for receiving CAN messages */
void led_flash_worker(void)
{
        log_info(_LOG_PFX "Starting flash worker\r\n");
        chRegSetThreadName("flash worker");

        uint32_t interval = 0;
        while(!chThdShouldTerminateX()) {
                uint8_t brightness = get_brightness();
                if (brightness == 0) {
                        brightness = _calculate_auto_brightness();
                }
                size_t i;
                for (i = 0; i < LED_COUNT; i++) {
                        struct LedFlashConfig * flash_config = get_flash_config(i);
                        uint8_t working_brightness = brightness;
                        uint8_t flash_hz = flash_config->flash_hz;
                        uint8_t current_state = flash_config->current_state;
                        if (flash_hz > 0) {
                                if (interval % (10 / flash_hz) == 0) {
                                        current_state = current_state == 1 ? 0 : 1;
                                        flash_config->current_state = current_state;
                                }
                                working_brightness = current_state ? working_brightness : 0;
                        }
                        set_led_brightness(i, working_brightness);
                }
                chThdSleepMilliseconds(100);
                interval += 1;
        }
}

static void set_linear_point(size_t index, uint8_t red, uint8_t green, uint8_t blue)
{
        int i;
        for (i = 0; i < LED_COUNT; i++) {
                int fade = max(0, (abs(i-index) * 100));
                set_led(i, max(0, (int)red - fade), max(0, (int)green - fade), max(0, (int)blue - fade));
        }
}

static void larson_scan(uint8_t red, uint8_t green, uint8_t blue)
{
        int l;
        for (l = 0; l < LED_COUNT; l++) {
                if (api_is_provisoned())
                        return;
                set_linear_point(l, red, green, blue);
                if (l < LINEAR_GRAPH_COUNT - 1)
                        chThdSleepMilliseconds(100);
        }
        for (l = LED_COUNT - 1; l >= 0; l--) {
                if (api_is_provisoned())
                        return;
                set_linear_point(l, red, green, blue);
                if (l > 0)
                        chThdSleepMilliseconds(100);
        }
}

/* Run Larson Scanner for the duration of the timeout, or until we receive a recognized message */
void startup_light_show(void)
{
        chThdSleepMilliseconds(1000);

        systime_t start = chVTGetSystemTimeX();
        while(chVTTimeElapsedSinceX(start) < MS2ST(DEMO_DURATION_MS) && !api_is_provisoned()) {
                larson_scan(255, 0, 0);
                larson_scan(0, 255, 0);
                larson_scan(0, 0, 255);
        }
        int l;
        for (l = 0; l < LED_COUNT; l++) {
                set_led(l, 0, 0, 0);
        }
}

/* Worker that runs the startup demo */
void startup_demo_worker(void)
{
        startup_light_show();
}
