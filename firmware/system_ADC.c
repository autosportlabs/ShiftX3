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

#include "system_ADC.h"
#include "logging.h"
#define _LOG_PFX "ADC:         "

#define ADC_GRP1_NUM_CHANNELS   1
#define ADC_GRP1_BUF_DEPTH      1
#define SAMPLE_BUFFER_SIZE ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH
static adcsample_t samples1[SAMPLE_BUFFER_SIZE];
/*
 * ADC streaming callback.
 */
static void adccallback(ADCDriver *adcp, adcsample_t *buffer, size_t n)
{
    (void)adcp;
    (void)buffer;
    (void)n;
    samples1[0] = buffer[0];
    //log_info(_LOG_PFX " ADC %i\r\n", samples1[0]);
}

static void adcerrorcallback(ADCDriver *adcp, adcerror_t err)
{
    (void)adcp;
    (void)err;
    //log_info(_LOG_PFX " ADC err\r\n");
}

/*
 * ADC conversion group.
 * Mode:        Continuous, 16 samples of 8 channels, SW triggered.
 * Channels:    IN10, IN11, Sensor, VRef.
 */
static const ADCConversionGroup adcgrpcfg1 = {
    FALSE,
    ADC_GRP1_NUM_CHANNELS,
    adccallback,
    adcerrorcallback,
    ADC_CFGR1_RES_12BIT,             /* CFGR1 */
    ADC_TR(0, 0),                                     /* TR */
    ADC_SMPR_SMP_28P5,                                /* SMPR */
    ADC_CHSELR_CHSEL1
};

void system_adc_init(void)
{
    size_t i;
    for (i=0; i < SAMPLE_BUFFER_SIZE; i++) {
        samples1[i] = 0;
    }
    /*
     * Setting up analog inputs used by the demo.
     */
    palSetGroupMode(GPIOA, PAL_PORT_BIT(1), 0, PAL_MODE_INPUT_ANALOG);
    adcStart(&ADCD1, NULL);
    //  adcSTM32SetCCR(ADC_CCR_VBATEN | ADC_CCR_TSEN | ADC_CCR_VREFEN);
    /* start continuous conversion */
    log_info("adc init\r\n");
}

uint16_t system_adc_sample(void)
{
    adcStartConversion(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
    return samples1[0];
}

