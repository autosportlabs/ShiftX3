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

#include "system_SPI.h"
#include "logging.h"
#include "system_serial.h"
#include "settings.h"
#include "system.h"

#define _LOG_PFX "SYS_SPI:     "

/*
 * Low speed SPI configuration (140.625kHz, CPHA=0, CPOL=0, MSb first).
 */
static const SPIConfig ls_spicfg = {
        NULL,
        GPIOB,
        1,
        SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0 | SPI_CR1_CPHA | SPI_CR1_CPOL,
        SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0
};

/* Initialize our CAN peripheral */
void spi_init(void)
{
        /* SPI1_SCK       */
        palSetPadMode(GPIOA, 5, PAL_STM32_MODE_ALTERNATE | PAL_STM32_ALTERNATE(0));
        /* SPI1_MISO      */
        palSetPadMode(GPIOA, 7, PAL_STM32_MODE_ALTERNATE | PAL_STM32_ALTERNATE(0));
}


void spi_send_buffer(uint8_t *buffer, size_t length)
{
        spiAcquireBus(&SPID1);              /* Acquire ownership of the bus.    */
        spiStart(&SPID1, &ls_spicfg);       /* Setup transfer parameters.       */
        spiSelect(&SPID1);                  /* Slave Select assertion.          */
        spiSend(&SPID1, length, buffer);    /* Atomic transfer operations.      */
        spiUnselect(&SPID1);                /* Slave Select de-assertion.       */
        spiReleaseBus(&SPID1);              /* Ownership release.               */
//    log_info(_LOG_PFX "Broadcasting SPI\r\n");
}


