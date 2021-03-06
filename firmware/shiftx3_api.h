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

#ifndef SHIFTX3_API_H_
#define SHIFTX3_API_H_
#include "ch.h"
#include "hal.h"
#include "system_CAN.h"

#define ALERT_THRESHOLDS 5

struct AlertThreshold {
    uint16_t threshold;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t flash_hz;
};

enum render_style {
    RENDER_STYLE_LEFT_RIGHT = 0,
    RENDER_STYLE_CENTER,
    RENDER_STYLE_RIGHT_LEFT
};

enum linear_style {
    LINEAR_STYLE_SMOOTH = 0,
    LINEAR_STYLE_STEPPED
};

enum orientation {
    DISPLAY_BOTTOM = 0,
    DISPLAY_TOP
};

/*Linear Graph Configuration */
#define LINEAR_GRAPH_THRESHOLDS 5
#define DEFAULT_LINEAR_GRAPH_COLOR_RED  0
#define DEFAULT_LINEAR_GRAPH_COLOR_GREEN  255
#define DEFAULT_LINEAR_GRAPH_COLOR_BLUE  0
#define DEFAULT_LINEAR_GRAPH_FLASH  0

struct LedFlashConfig {
    uint8_t current_state;
    uint8_t flash_hz;
};

struct LinearGraphConfig {
    enum render_style render_style;
    enum linear_style linear_style;
    uint16_t low_range;
    uint16_t high_range;
};

struct LinearGraphThreshold {
    uint8_t segment_length;
    uint16_t threshold;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t flash_hz;
};

#define DEFAULT_BRIGHTNESS              0
#define DEFAULT_LIGHT_SENSOR_SCALING    61
#define DISPLAY_ORIENTATIONS            2
#define DEFAULT_ORIENTATION             DISPLAY_BOTTOM

struct ConfigGroup1 {
    uint8_t brightness;
    uint8_t light_sensor_scaling;
    enum orientation orientation;
};

/* API offsets */
#define SHIFTX3_CAN_BASE_ID     0xE3600
#define SHIFTX3_CAN_API_RANGE   256
#define SHIFTX3_CAN_FILTER_MASK 0x1FFFFF00

#define API_ANNOUNCEMENT                    0
#define API_RESET_DEVICE                    1
#define API_STATS                           2
#define API_SET_CONFIG_GROUP_1              3

/* Configuration and Runtime */
/* Direct control messages */
#define API_SET_DISCRETE_LED                10

/* Alert configuration and control messages */
#define API_SET_ALERT_LED                   20
#define API_SET_ALERT_THRESHOLD             21
#define API_SET_CURRENT_ALERT_VALUE         22

/* Linear graph configuration and control messages */
#define API_CONFIG_LINEAR_GRAPH             40
#define API_SET_LINEAR_THRESHOLD            41
#define API_SET_CURRENT_LINEAR_GRAPH_VALUE  42

#define API_ALERT_BUTTON_STATES             60

#define API_SET_DISPLAY_VALUE               50
#define API_SET_DISPLAY_SEGMENT             51

uint8_t get_brightness(void);

uint8_t get_light_sensor_scaling(void);

enum orientation get_orientation(void);

struct LedFlashConfig * get_flash_config(size_t index);
void set_flash_config(size_t led_index, uint8_t flash_hz);


/* Base API functions */
bool api_is_provisoned(void);
void set_api_is_provisioned(bool);
void api_initialize(void);
void api_set_config_group_1(CANRxFrame *rx_msg);
void api_set_discrete_led(CANRxFrame *rx_msg);

/* Alert related functions */
void api_set_alert_led(CANRxFrame *rx_msg);
void api_set_alert_threshold(CANRxFrame *rx_msg);
void api_set_current_alert_value(CANRxFrame *rx_msg);

/* Linear graph related functions */
void api_config_linear_graph(CANRxFrame *rx_msg);
void api_set_linear_threshold(CANRxFrame *rx_msg);
void api_set_current_linear_graph_value(CANRxFrame *rx_msg);

/* 7 segment display related functions */
void api_set_display_value(CANRxFrame *rx_msg);
void api_set_display_segment(CANRxFrame *rx_msg);

void api_send_announcement(void);

#endif /* SHIFTX3_API_H_ */
