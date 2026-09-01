//
// Created by uv on 23/08/2026.
//

#ifndef BEARMETAL_W25Q_SPI_DRIVER_H
#define BEARMETAL_W25Q_SPI_DRIVER_H

#include <stdint.h>
#include "stm32g474xx.h"

/* --- SPI Driver Status Codes --- */
typedef enum {
    SPI_OK             = 0x00,
    SPI_ERROR_TIMEOUT  = 0x01,
    SPI_ERROR_OVERRUN  = 0x02,
    SPI_ERROR_BUSY     = 0x03,
    SPI_ERROR_PARAM    = 0x04
} w25q_spi_status_t;

/* --- Hardware Handle (Dependency Injection) --- */
typedef struct {
    SPI_TypeDef   *spi;    /* E.g., SPI1, SPI2 (CMSIS structures) */
    GPIO_TypeDef  *cs_port;      /* GPIO Port for Chip Select (e.g., GPIOA) */
    uint16_t       cs_pin;       /* GPIO Pin for Chip Select (e.g., GPIO_BSRR_BS_4) */
} w25q_spi_handle_t;

/* --- Core SPI Peripheral API Methods --- */

/**
 * @brief  Initializes the physical SPI registers and gates the clocks.
 * @param  hspi: Pointer to the driver handle configuration.
 * @retval w25q_spi_status_t: Configuration state outcome.
 */
w25q_spi_status_t w25q_spi_init(const w25q_spi_handle_t *hspi);

/**
 * @brief  Drives the configured Chip Select (CS) pin low.
 * @param  hspi: Pointer to the hardware configuration structure.
 */
void w25q_spi_cs_assert(const w25q_spi_handle_t *hspi);

/**
 * @brief  Drives the configured Chip Select (CS) pin high.
 * @param  hspi: Pointer to the hardware configuration structure.
 */
void w25q_spi_cs_deassert(const w25q_spi_handle_t *hspi);

/**
 * @brief  Transmits a single byte and clocks back a byte simultaneously (Blocking).
 * @param  hspi: Pointer to the active SPI handle.
 * @param  tx_data: Byte to shift out over MOSI.
 * @param  rx_data: Pointer to capture shifted-in byte over MISO.
 * @retval w25q_spi_status_t: Outcome of transmission timing loops.
 */
w25q_spi_status_t w25q_spi_transfer_byte(const w25q_spi_handle_t *hspi, uint8_t tx_data, uint8_t* rx_data);

/**
 * @brief  Sends a block of data across MOSI. Discards resulting MISO inputs.
 * @param  hspi: Pointer to the active SPI handle.
 * @param  buffer: Memory address holding target array.
 * @param  length: Total bytes to stream.
 * @retval w25q_spi_status_t: Hardware transfer verification status.
 */
w25q_spi_status_t w25q_spi_transmit(const w25q_spi_handle_t* hspi, const uint8_t* buffer, uint32_t length);

/**
 * @brief  Receives a block of data across MISO by writing standard 0xFF dummy bytes.
 * @param  hspi: Pointer to the active SPI handle.
 * @param  buffer: Target destination memory allocation.
 * @param  length: Total bytes expected to read.
 * @retval w25q_spi_status_t: Hardware shift success code.
 */
w25q_spi_status_t w25q_spi_receive(const w25q_spi_handle_t* hspi, uint8_t* buffer, uint32_t length);

#endif //BEARMETAL_W25Q_SPI_DRIVER_H
