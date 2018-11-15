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
#include "shiftx3_api.h"

#include "logging.h"
#include "system_LED.h"
#include "system_display.h"
#include "settings.h"
#include "ch.h"
#include "hal.h"
#define _LOG_PFX "API:         "

static struct AlertThreshold g_alert_threshold[ALERT_COUNT][ALERT_THRESHOLDS];
static uint16_t g_current_alert_value[ALERT_COUNT];

static struct LinearGraphConfig g_linear_graph_config;
static struct LinearGraphThreshold g_linear_graph_threshold[LINEAR_GRAPH_THRESHOLDS];

static uint16_t g_current_linear_graph_value;
static struct LedFlashConfig g_flash_config[LED_COUNT];

static struct ConfigGroup1 g_config_group_1 = {DEFAULT_BRIGHTNESS, DEFAULT_LIGHT_SENSOR_SCALING, DEFAULT_ORIENTATION};

static bool g_provisioned = false;

static void _set_led_multi(size_t index, size_t length, uint8_t red, uint8_t green, uint8_t blue, uint8_t flash)
{
    size_t i;
    for (i = 0; i < length; i++) {
        set_led(index + i, red, green, blue);
        set_flash_config(index + i, flash);
    }
}

static void _update_alert_value(uint8_t alert_id)
{
    uint16_t current_value = g_current_alert_value[alert_id];

    size_t i;
    struct AlertThreshold * t = NULL;
    for (i = 0; i < ALERT_THRESHOLDS; i++) {
        struct AlertThreshold * ttest = &g_alert_threshold[alert_id][i];
        if (current_value >= ttest->threshold && (ttest->threshold > 0 || i == 0)) {
            t = ttest;
        }
    }
    uint8_t red = 0, green = 0, blue = 0, flash = 0;
    if (t) {
        red = t->red;
        green = t->green;
        blue = t->blue;
        flash = t->flash_hz;
    }
    uint8_t disp_led_idx = (get_orientation() == DISPLAY_BOTTOM) ? alert_id : (ALERT_COUNT - alert_id - 1);
    set_led(ALERT_OFFSET + disp_led_idx, red, green, blue);
    set_flash_config(ALERT_OFFSET + disp_led_idx, flash);
}

static struct LinearGraphThreshold * _select_linear_threshold(uint16_t value)
{
    size_t i;
    struct LinearGraphThreshold  * t = NULL;
    for (i = 0; i < LINEAR_GRAPH_THRESHOLDS; i++) {
        struct LinearGraphThreshold  * ttest = &g_linear_graph_threshold[i];
        if (value >= ttest->threshold && (ttest->threshold > 0 || i == 0)) {
            t = ttest;
        }
    }
    return t;
}

static void _update_linear_graph(uint16_t value, uint16_t range, struct LinearGraphThreshold * threshold, uint8_t graph_size, uint8_t start_offset, enum linear_style lstyle, bool left_right)
{
    uint8_t graph_length = 0;

    if (lstyle == LINEAR_STYLE_STEPPED) {
        graph_length = threshold->segment_length;
    } else {
        /* percentage of range */
        uint32_t pct = (value * 100) / range;
        graph_length = (graph_size * pct) / 100;
    }

    /* rail to max number of LEDs, for safety */
    graph_length = graph_length > graph_size ? graph_size : graph_length;

    uint32_t red = threshold->red;
    uint32_t green = threshold->green;
    uint32_t blue = threshold->blue;
    uint8_t flash = threshold->flash_hz;

    size_t i;
    uint8_t led_index = 0;
    bool render_lr = left_right != (get_orientation() == DISPLAY_TOP); // Logical XOR
    for (i = 0; i < graph_size; i++) {
        if (render_lr) {
            led_index = start_offset + i;
        } else {
            led_index = (start_offset + graph_size - 1) - i;
        }

        if (i < graph_length) {
            set_led(led_index, red, green, blue);
        } else {
            set_led(led_index, 0, 0, 0);
        }
        set_flash_config(led_index, flash);
    }
    /* linerally dim the next LED based on the fractional amount */
    if (lstyle == LINEAR_STYLE_SMOOTH && graph_length < graph_size) {
        uint32_t pct = (value * 100) / range;
        uint32_t remainder = (graph_size * pct) % 100;
        red = red * remainder / 100;
        green = green * remainder / 100;
        blue = blue * remainder / 100;
        led_index = (render_lr) ?
                    start_offset + graph_length :
                    (start_offset + graph_size - 1) - graph_length;
        set_led(led_index, red, green, blue);
        set_flash_config(led_index, flash);
    }
}

static void _update_center_graph(uint16_t value, uint16_t range, struct LinearGraphThreshold * threshold, enum linear_style lstyle)
{
    /* assuming odd number of LEDs */
    uint8_t center_led = (LINEAR_GRAPH_COUNT / 2);
    uint8_t graph_count = center_led + 1;
    uint32_t center_value = range / 2;

    size_t index;

    if (value > center_value) { /* left to right rendering */
        /*turn off LEDs left of center */
        for (index = 0; index < center_led; index++) {
            set_led(index, 0, 0, 0);
        }
        uint8_t offset = (get_orientation() == DISPLAY_BOTTOM) ? center_led : 0;
        _update_linear_graph(value - center_value, range / 2, threshold, graph_count, offset, lstyle, true);
    } else { /* right to left rendering */
        /* turn off LEDs right of center */
        for (index = center_led; index < LINEAR_GRAPH_COUNT; index++) {
            set_led(index, 0, 0, 0);
        }
        uint8_t offset = (get_orientation() == DISPLAY_BOTTOM) ? 0 : center_led;
        _update_linear_graph(center_value - value, range / 2, threshold, graph_count, offset, lstyle, false);
    }
}

static void _update_linear_graph_value(void)
{
    uint32_t low_range = g_linear_graph_config.low_range;
    uint32_t high_range = g_linear_graph_config.high_range;
    uint32_t range = high_range - low_range;

    uint16_t current_value = g_current_linear_graph_value;
    /* offset to zero */
    current_value -= low_range;

    enum linear_style lstyle = g_linear_graph_config.linear_style;
    enum render_style rstyle = g_linear_graph_config.render_style;
    bool left_right = rstyle == RENDER_STYLE_LEFT_RIGHT;
    struct LinearGraphThreshold * threshold = _select_linear_threshold(current_value);
    if (!threshold) {
        /* disable graph if no threshold selected*/
        _set_led_multi(LINEAR_GRAPH_OFFSET, LINEAR_GRAPH_COUNT, 0, 0, 0, 0);
        return;
    }
    switch (rstyle) {
    case RENDER_STYLE_CENTER:
        _update_center_graph(current_value, range, threshold, lstyle);
        break;
    case RENDER_STYLE_LEFT_RIGHT:
    case RENDER_STYLE_RIGHT_LEFT:
        _update_linear_graph(current_value, range, threshold, LINEAR_GRAPH_COUNT, LINEAR_GRAPH_OFFSET, lstyle, left_right);
        break;
    default:
        log_info(_LOG_PFX "Invalid render style (%i)\r\n", rstyle);
        break;
    }
}

bool api_is_provisoned(void)
{
    return g_provisioned;
}

void set_api_is_provisioned(bool provisioned)
{
    g_provisioned = provisioned;
}

void api_initialize(void)
{
    size_t i;
    /* Init flash configuration */
    for (i = 0; i < LED_COUNT; i++) {
        g_flash_config[i].current_state = 0;
        g_flash_config[i].flash_hz = 0;
    }

    /* Init alert configuration */
    for (i = 0; i < ALERT_COUNT; i++) {
        g_current_alert_value[i] = 0;
    }

    for (i = 0; i < ALERT_COUNT; i++) {
        size_t ii;
        for (ii = 0; ii < ALERT_THRESHOLDS; ii++) {
            struct AlertThreshold * t = & g_alert_threshold[i][ii];
            t->red = 0;
            t->green = 0;
            t->blue = 0;
            t->threshold = 0;
            t->flash_hz = 0;
        }
    }

    /* Set default linear graph configuration */
    g_linear_graph_config.render_style = RENDER_STYLE_LEFT_RIGHT;
    g_linear_graph_config.linear_style = LINEAR_STYLE_SMOOTH;
    g_linear_graph_config.low_range = 0;
    g_linear_graph_config.high_range = 10000;

    /* Set default linear graph thresholds */
    g_linear_graph_threshold[0].threshold = 3000;
    g_linear_graph_threshold[0].segment_length = 3;
    g_linear_graph_threshold[0].red = 0;
    g_linear_graph_threshold[0].green = 255;
    g_linear_graph_threshold[0].blue = 0;
    g_linear_graph_threshold[0].flash_hz = 0;

    g_linear_graph_threshold[1].threshold = 5000;
    g_linear_graph_threshold[1].segment_length = 5;
    g_linear_graph_threshold[1].red = 255;
    g_linear_graph_threshold[1].green = 127;
    g_linear_graph_threshold[1].blue = 0;
    g_linear_graph_threshold[1].flash_hz = 0;

    g_linear_graph_threshold[2].threshold = 7000;
    g_linear_graph_threshold[2].segment_length = 7;
    g_linear_graph_threshold[2].red = 255;
    g_linear_graph_threshold[2].green = 0;
    g_linear_graph_threshold[2].blue = 0;
    g_linear_graph_threshold[2].flash_hz = 5;
}

static void _set_brightness(uint8_t brightness)
{
    g_config_group_1.brightness = brightness;
}

uint8_t get_brightness(void)
{
    return g_config_group_1.brightness;
}

static void _set_light_sensor_scaling(uint8_t scaling)
{
    g_config_group_1.light_sensor_scaling = scaling;
}

uint8_t get_light_sensor_scaling(void)
{
    return g_config_group_1.light_sensor_scaling;
}

static void _set_orientation(enum orientation orientation) 
{
    g_config_group_1.orientation = orientation;
}

enum orientation get_orientation(void)
{
    return g_config_group_1.orientation;
}

struct LedFlashConfig * get_flash_config(size_t led_index)
{
    return &g_flash_config[led_index];
}

void set_flash_config(size_t led_index, uint8_t flash_hz)
{
    g_flash_config[led_index].flash_hz = flash_hz;
}


void api_set_config_group_1(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 1) {
        log_info(_LOG_PFX "Invalid params for set config group 1\r\n");
        return;
    }
    uint32_t brightness = rx_msg->data8[0];
    if (brightness > 0) {
        /**
         * User specified a brightness.
         * Scale percentage to internal APA102 brightness factor and rail to limits
         */
        brightness = APA102_MAX_BRIGHTNESS * brightness / 100;
        brightness = min(APA102_MAX_BRIGHTNESS, brightness);
        brightness = max(1, brightness);
    }

    _set_brightness(brightness);
    log_trace(_LOG_PFX "Set config group 1 : brightness(%i)\r\n", brightness);

    if (rx_msg->DLC >= 2) {
        uint8_t scaling = rx_msg->data8[1];
        _set_light_sensor_scaling(scaling);
        log_trace(_LOG_PFX "Set config group 1: light sensor scaling: %i\r\n", scaling);
    }

    if (rx_msg->DLC >= 3) {

        uint8_t orientation = rx_msg->data8[2];
        if (orientation > DISPLAY_TOP) {
            log_info(_LOG_PFX "Invalid display orientation %i specified\r\n", orientation);
        } else {
            _set_orientation(orientation);
            log_trace(_LOG_PFX "Set config group 1: orientation: %i\r\n", orientation);
        }
    }
}

void api_set_discrete_led(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 6) {
        log_info(_LOG_PFX "Invalid params for set discrete LED\r\n");
        return;
    }
    /* data validation on LED index */
    uint8_t index = rx_msg->data8[0];
    index = index < LED_COUNT ? index : LED_COUNT - 1;

    /* perform data validation on LED count */
    uint8_t length = rx_msg->data8[1];
    length = length == 0 ? LED_COUNT : length;
    length = length <= LED_COUNT - index ? length : LED_COUNT - index;

    uint8_t red =  rx_msg->data8[2];
    uint8_t green =  rx_msg->data8[3];
    uint8_t blue =  rx_msg->data8[4];
    uint8_t flash = rx_msg->data8[5];

    log_trace(_LOG_PFX "Set Discrete LED : (%i) length(%i) rgb(%i, %i, %i) flash(%i)\r\n", index, length, red, green, blue, flash);
    _set_led_multi(index, length, red, green, blue, flash);
}

void api_set_alert_led(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 5) {
        log_info(_LOG_PFX "Invalid param count for set alert LED\r\n");
        return;
    }
    /* data validation on LED index */
    uint8_t alert_id = rx_msg->data8[0];
    if (alert_id >= ALERT_COUNT) {
        log_info(_LOG_PFX "Invalid alert id for set alert LED\r\n");
        return;
    }

    uint8_t red =  rx_msg->data8[1];
    uint8_t green =  rx_msg->data8[2];
    uint8_t blue =  rx_msg->data8[3];
    uint8_t flash = rx_msg->data8[4];

    log_trace(_LOG_PFX "Set Alert LED (%i) : rgb(%i, %i, %i) flash(%i)\r\n", alert_id, red, green, blue, flash);
    uint8_t disp_led_idx = (get_orientation() == DISPLAY_BOTTOM) ? alert_id : (ALERT_COUNT - alert_id - 1);
    set_led(ALERT_OFFSET + disp_led_idx, red, green, blue);
    set_flash_config(ALERT_OFFSET + disp_led_idx, flash);
}

void api_set_alert_threshold(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 8) {
        log_info(_LOG_PFX "Invalid param count for set alert threshold\r\n");
        return;
    }

    uint8_t alert_id = rx_msg->data8[0];
    if (alert_id >= ALERT_COUNT) {
        log_info(_LOG_PFX "Invalid alert id for set alert threshold\r\n");
        return;
    }

    uint8_t threshold_id = rx_msg->data8[1];
    if (threshold_id >= ALERT_THRESHOLDS) {
        log_info(_LOG_PFX "Invalid threshold id for set alert threshold\r\n");
        return;
    }

    struct AlertThreshold * t = &g_alert_threshold[alert_id][threshold_id];
    uint16_t threshold = rx_msg->data8[2] + (rx_msg->data8[3] * 256);
    uint8_t red = rx_msg->data8[4];
    uint8_t green = rx_msg->data8[5];
    uint8_t blue = rx_msg->data8[6];
    uint8_t flash = rx_msg->data8[7];
    t->threshold = threshold;
    t->red = red;
    t->green = green;
    t->blue = blue;
    t->flash_hz = flash;
    log_trace(_LOG_PFX "Set Alert Threshold : alert_id(%i) threshold_id(%i) threshold(%i) rgb(%i, %i, %i) flash(%i)\r\n",
              alert_id, threshold_id, threshold, red, green, blue, flash);
}

void api_set_current_alert_value(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 3) {
        log_info(_LOG_PFX "Invalid param count for set current alert value\r\n");
        return;
    }

    uint8_t alert_id = rx_msg->data8[0];
    if (alert_id >= ALERT_COUNT) {
        log_info(_LOG_PFX "Invalid alert id for set current alert value\r\n");
        return;
    }

    uint16_t current_value = rx_msg->data8[1] + (rx_msg->data8[2] * 256);
    g_current_alert_value[alert_id] = current_value;
    log_trace(_LOG_PFX "Set current alert value : alert_id(%i) value(%i)\r\n", alert_id, current_value);
    _update_alert_value(alert_id);
}

void api_config_linear_graph(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 6) {
        log_info(_LOG_PFX "Invalid param count for config linear graph\r\n");
        return;
    }

    uint8_t rstyle = rx_msg->data8[0];
    if (rstyle > RENDER_STYLE_RIGHT_LEFT) {
        log_info(_LOG_PFX "Invalid render style %i specified for config linear graph\r\n", rstyle);
        return;
    }

    uint8_t lstyle = rx_msg->data8[1];
    if (lstyle > LINEAR_STYLE_STEPPED) {
        log_info(_LOG_PFX "Invalid linear style %i specified for config linear graph\r\n", lstyle);
        return;
    }
    uint16_t low_range = rx_msg->data16[1];
    uint16_t high_range = rx_msg->data16[2];

    if (lstyle != LINEAR_STYLE_STEPPED) {
        if (high_range <= low_range) {
            log_info(_LOG_PFX "Invalid low/high range (%i/%i) \r\n", low_range, high_range);
            return;
        }
    }

    g_linear_graph_config.render_style = rstyle;
    g_linear_graph_config.linear_style = lstyle;
    g_linear_graph_config.low_range = low_range;
    g_linear_graph_config.high_range = high_range;

    log_trace(_LOG_PFX "Config linear graph : render style(%i) linear style(%i) low range(%i) high range(%i)\r\n", rstyle, lstyle, low_range, high_range);
}

void api_set_linear_threshold(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 8) {
        log_info(_LOG_PFX "Invalid param count for set linear graph threshold\r\n");
        return;
    }

    uint8_t threshold_id = rx_msg->data8[0];
    if (threshold_id >= LINEAR_GRAPH_THRESHOLDS) {
        log_info(_LOG_PFX "Invalid threshold id for set linear graph threshold\r\n");
        return;
    }

    uint8_t segment_length = rx_msg->data8[1];
    if (segment_length > LINEAR_GRAPH_COUNT) {
        log_info(_LOG_PFX "Invalid segment count for set linear graph threshold\r\n");
        return;
    }

    struct LinearGraphThreshold * t = &g_linear_graph_threshold[threshold_id];

    uint16_t threshold = rx_msg->data16[1];
    uint8_t red = rx_msg->data8[4];
    uint8_t green = rx_msg->data8[5];
    uint8_t blue = rx_msg->data8[6];
    uint8_t flash = rx_msg->data8[7];
    t->segment_length = segment_length;
    t->threshold = threshold;
    t->red = red;
    t->green = green;
    t->blue = blue;
    t->flash_hz = flash;
    log_trace(_LOG_PFX "Set Linear Graph Threshold : threshold_id(%i) threshold(%i) rgb(%i, %i, %i) flash(%i)\r\n",
              threshold_id, threshold, red, green, blue, flash);
}

void api_set_current_linear_graph_value(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 2) {
        log_info(_LOG_PFX "Invalid param count for set current linear graph value\r\n");
        return;
    }

    uint16_t current_value = rx_msg->data16[0];
    g_current_linear_graph_value = current_value;
    _update_linear_graph_value();
}

void api_send_announcement(void)
{
    CANTxFrame announce;
    prepare_can_tx_message(&announce, CAN_IDE_EXT, get_can_base_id());
    announce.data8[0] = LED_COUNT;
    announce.data8[1] = ALERT_COUNT;
    announce.data8[2] = LINEAR_GRAPH_COUNT;
    announce.data8[3] = MAJOR_VER;
    announce.data8[4] = MINOR_VER;
    announce.data8[5] = PATCH_VER;
    announce.DLC = 6;
    canTransmit(&CAND1, CAN_ANY_MAILBOX, &announce, MS2ST(CAN_TRANSMIT_TIMEOUT));
    log_info(_LOG_PFX "Broadcast announcement\r\n");
}

void api_set_display_value(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 2) {
        log_info(_LOG_PFX "Invalid param count for set display value\r\n");
        return;
    }
    uint8_t digit = rx_msg->data8[0];
    char value = (char)rx_msg->data8[1];
    display_set_value(digit, value);
}

void api_set_display_segment(CANRxFrame *rx_msg)
{
    if (rx_msg->DLC < 8) {
        log_info(_LOG_PFX "Invalid param count for set display segment\r\n");
        return;
    }
    uint8_t digit = rx_msg->data8[0];

    for (size_t i = 1; i < 8; i++) {
        uint8_t segment = rx_msg->data8[i];
        bool enabled = segment != 0;
        display_set_segment(digit, i - 1, enabled);
    }
}
