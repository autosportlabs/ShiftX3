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
#include "system_display.h"
#include "system_SPI.h"
#include "logging.h"
#include "system_ADC.h"
#include "ch.h"
#include "hal.h"
#include "shiftx3_api.h"

#define _LOG_PFX "DISPLAY: "

#define DISPLAY_DIGITS 1

#define DISPLAY_SEGMENT_A_PORT GPIOB
#define DISPLAY_SEGMENT_B_PORT GPIOB
#define DISPLAY_SEGMENT_C_PORT GPIOB
#define DISPLAY_SEGMENT_D_PORT GPIOA
#define DISPLAY_SEGMENT_E_PORT GPIOB
#define DISPLAY_SEGMENT_F_PORT GPIOB
#define DISPLAY_SEGMENT_G_PORT GPIOA
#define DISPLAY_SEGMENT_DP_PORT GPIOA

#define DISPLAY_SEGMENT_A_PIN 6
#define DISPLAY_SEGMENT_B_PIN 5
#define DISPLAY_SEGMENT_C_PIN 4
#define DISPLAY_SEGMENT_D_PIN 15
#define DISPLAY_SEGMENT_E_PIN 3
#define DISPLAY_SEGMENT_F_PIN 1
#define DISPLAY_SEGMENT_G_PIN 6
#define DISPLAY_SEGMENT_DP_PIN 8

#define DISPLAY_PWM_CONTROL_PORT GPIOB
#define DISPLAY_PWM_CONTROL_PIN 0
#define DISPLAY_MIN_BRIGHTNESS 125
#define DISPLAY_MAX_BRIGHTNESS 1000

#define DISPLAY_PWM_CLOCK_FREQUENCY 200000
#define DISPLAY_PWM_PERIOD 1000
#define DISPLAY_PWM_SCALING 2
#define DISPLAY_PWM_OFFSET 125
#define DISPLAY_PWM_PERCENT_SCALING 35

/* Brightness averaging buffer */
#define BRIGHTNESS_AVG_BUFFER 20
static uint16_t brightness_avg_buffer[BRIGHTNESS_AVG_BUFFER] = {0};
static size_t brightness_avg_index = 0;


static PWMConfig pwmcfg = {
    DISPLAY_PWM_CLOCK_FREQUENCY, /* 200Khz PWM clock frequency*/
    DISPLAY_PWM_PERIOD,
    NULL, /* No callback */
    /* Enabled channels */
    {
        {PWM_OUTPUT_ACTIVE_LOW, NULL},
        {PWM_OUTPUT_ACTIVE_LOW, NULL},
        {PWM_OUTPUT_ACTIVE_LOW, NULL},
        {PWM_OUTPUT_ACTIVE_LOW, NULL}
    },
    0,
    0
};

struct port_pin {
    stm32_gpio_t * port;
    uint8_t pin;
};

#define DISPLAY_SEGMENT_COUNT 7
#define DISPLAY_SEGMENT_BOTTOM_MAPPING {{DISPLAY_SEGMENT_A_PORT, DISPLAY_SEGMENT_A_PIN}, \
                           {DISPLAY_SEGMENT_B_PORT, DISPLAY_SEGMENT_B_PIN}, \
                           {DISPLAY_SEGMENT_C_PORT, DISPLAY_SEGMENT_C_PIN}, \
                           {DISPLAY_SEGMENT_D_PORT, DISPLAY_SEGMENT_D_PIN}, \
                           {DISPLAY_SEGMENT_E_PORT, DISPLAY_SEGMENT_E_PIN}, \
                           {DISPLAY_SEGMENT_F_PORT, DISPLAY_SEGMENT_F_PIN}, \
                           {DISPLAY_SEGMENT_G_PORT, DISPLAY_SEGMENT_G_PIN}, \
}
#define DISPLAY_SEGMENT_TOP_MAPPING {{DISPLAY_SEGMENT_D_PORT, DISPLAY_SEGMENT_D_PIN}, \
                           {DISPLAY_SEGMENT_E_PORT, DISPLAY_SEGMENT_E_PIN}, \
                           {DISPLAY_SEGMENT_F_PORT, DISPLAY_SEGMENT_F_PIN}, \
                           {DISPLAY_SEGMENT_A_PORT, DISPLAY_SEGMENT_A_PIN}, \
                           {DISPLAY_SEGMENT_B_PORT, DISPLAY_SEGMENT_B_PIN}, \
                           {DISPLAY_SEGMENT_C_PORT, DISPLAY_SEGMENT_C_PIN}, \
                           {DISPLAY_SEGMENT_G_PORT, DISPLAY_SEGMENT_G_PIN}, \
}

static const struct port_pin display_port_mappings[DISPLAY_ORIENTATIONS][DISPLAY_SEGMENT_COUNT] = 
    {DISPLAY_SEGMENT_BOTTOM_MAPPING, DISPLAY_SEGMENT_TOP_MAPPING};

#define CHARMAP { \
        {'0',0x7E}, \
        {'1', 0x30}, \
        {'2', 0x6D}, \
        {'3', 0x79}, \
        {'4', 0x33}, \
        {'5', 0x5B}, \
        {'6', 0x5F}, \
        {'7', 0x70}, \
        {'8', 0x7F}, \
        {'9', 0x7B}, \
        {' ', 0x00}, \
        {'A', 0x77}, \
        {'a', 0x7D}, \
        {'B', 0x7F}, \
        {'b', 0x1F}, \
        {'C', 0x4E}, \
        {'c', 0x0D}, \
        {'D', 0x7E}, \
        {'d', 0x3D}, \
        {'E', 0x4F}, \
        {'e', 0x6f}, \
        {'F', 0x47}, \
        {'f', 0x47}, \
        {'G', 0x5E}, \
        {'g', 0x7B}, \
        {'H', 0x37}, \
        {'h', 0x17}, \
        {'I', 0x30}, \
        {'i', 0x10}, \
        {'J', 0x3C}, \
        {'j', 0x38}, \
        {'K', 0x37}, \
        {'k', 0x17}, \
        {'L', 0x0E}, \
        {'l', 0x06}, \
        {'M', 0x55}, \
        {'m', 0x55}, \
        {'N', 0x15}, \
        {'n', 0x15}, \
        {'O', 0x7E}, \
        {'o', 0x1D}, \
        {'P', 0x67}, \
        {'p', 0x67}, \
        {'Q', 0x73}, \
        {'q', 0x73}, \
        {'R', 0x77}, \
        {'r', 0x05}, \
        {'S', 0x5B}, \
        {'s', 0x5B}, \
        {'T', 0x46}, \
        {'t', 0x0F}, \
        {'U', 0x3E}, \
        {'u', 0x1C}, \
        {'V', 0x27}, \
        {'v', 0x23}, \
        {'W', 0x3F}, \
        {'w', 0x2B}, \
        {'X', 0x25}, \
        {'x', 0x25}, \
        {'Y', 0x3B}, \
        {'y', 0x33}, \
        {'Z', 0x6D}, \
        {'z', 0x6D}, \
        {'-', 0x01}, \
        {'=', 0x41}, \
        {'_', 0x08}, \
        {'~', 0x49} \
}

struct char_segment {
    char character;
    uint8_t bitmask;
};


static const struct char_segment character_mappings[] = CHARMAP;

#define CHARMAP_COUNT (sizeof(character_mappings) / sizeof(struct char_segment))

void display_set_segment(const uint8_t digit, const uint8_t segment, const bool enabled)
{
    const enum orientation orientation = get_orientation();
    log_trace(_LOG_PFX "set segment %d: %d %d %d\r\n", digit, segment, enabled, orientation);
    if (segment >= DISPLAY_SEGMENT_COUNT)
        return;

    const struct port_pin *mapping = &display_port_mappings[orientation][segment];

    if (enabled) {
        palClearPad(mapping->port, mapping->pin);
    } else {
        palSetPad(mapping->port, mapping->pin);
    }
}

void display_set_value(const uint8_t digit, const char value)
{
    log_trace(_LOG_PFX "set value %d: %c\r\n", digit, value);

    for (size_t i = 0; i < CHARMAP_COUNT; i++) {
        const struct char_segment * mapping = & character_mappings[i];
        if (mapping->character == value) {
            uint8_t bitmask = mapping->bitmask;
            uint8_t segment = DISPLAY_SEGMENT_COUNT;
            while (segment > 0) {
                segment--;
                display_set_segment(digit, segment, bitmask & 0x01);
                bitmask = bitmask >> 1;
            }
            break;
        }
    }
}

static void _clear_display(void)
{
    for (size_t d = 0; d < DISPLAY_DIGITS; d++) {
        for (size_t i = 0; i < DISPLAY_SEGMENT_COUNT; i++) {
            display_set_segment(d, i, false);
        }
    }
}

void system_display_init(void)
{

    /* init ports for segments */
    for (size_t i = 0; i < DISPLAY_SEGMENT_COUNT; i++) {
        const struct port_pin *mapping = &display_port_mappings[DISPLAY_BOTTOM][i];
        palSetPadMode(mapping->port, mapping->pin, PAL_MODE_OUTPUT_OPENDRAIN);
    }

    /* init PWM for display brightness control */
    palSetPadMode(DISPLAY_PWM_CONTROL_PORT, DISPLAY_PWM_CONTROL_PIN, PAL_MODE_ALTERNATE(1));
    pwmStart(&PWMD3, &pwmcfg);
    _clear_display();
}

void display_update_brightness(void)
{
    uint16_t brightness = get_brightness();
    if (brightness) {
        brightness = DISPLAY_PWM_OFFSET + (brightness * DISPLAY_PWM_PERCENT_SCALING);
        log_trace(_LOG_PFX "User brightness: %d\r\n", brightness);
    } else {
        uint16_t light_sensor = system_adc_sample();
        uint8_t user_scaling = get_light_sensor_scaling();
        brightness = DISPLAY_PWM_OFFSET + (light_sensor * user_scaling / DISPLAY_PWM_SCALING);
        log_trace(_LOG_PFX "Auto brightness: Sensor ADC/scaling/brightness %d/%d/%d\r\n", light_sensor, user_scaling, brightness);
    }
    brightness = brightness > DISPLAY_MAX_BRIGHTNESS ? DISPLAY_MAX_BRIGHTNESS : brightness;
    brightness = brightness < DISPLAY_MIN_BRIGHTNESS ? DISPLAY_MIN_BRIGHTNESS : brightness;

    /* update the averaging buffer */
    brightness_avg_buffer[brightness_avg_index] = brightness;
    brightness_avg_index = brightness_avg_index >= BRIGHTNESS_AVG_BUFFER - 1 ? 0 : brightness_avg_index + 1;

    /* calculate the average */
    uint32_t acc = 0;
    for (size_t i = 0; i < BRIGHTNESS_AVG_BUFFER; i++) {
            acc += brightness_avg_buffer[i];
    }
    brightness = acc / BRIGHTNESS_AVG_BUFFER;

    pwmEnableChannel(&PWMD3, 2, brightness);
}
