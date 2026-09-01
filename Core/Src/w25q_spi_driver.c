//
// Created by uv on 23/08/2026.
//

#include "w25q_spi_driver.h"

#include <stddef.h>

#define SPI_IO_CYCLES_LIMIT 100000U

/* --- Core SPI Peripheral API Methods --- */
/**
 * @brief  Initializes the physical SPI registers and gates the clocks.
 * @param  hspi: Pointer to the driver handle configuration.
 * @retval w25q_spi_status_t: Configuration state outcome.
 */
w25q_spi_status_t w25q_spi_init(const w25q_spi_handle_t* hspi) {
    w25q_spi_cs_deassert(hspi);
    // order first set what to do in cr2 and then go do it by SPI_CR1_SPE
    hspi->spi->CR2 |= SPI_CR2_FRXTH;
    hspi->spi->CR1 |= SPI_CR1_SPE;
    /*
        If ignored the default values of the STM32G4 it should look like this for CR2:
        // First, unlock everything by disabling the SPI peripheral
        SPI1->CR1 &= ~SPI_CR1_SPE;
    
        // Clear the existing Data Size bits
        SPI1->CR2 &= ~SPI_CR2_DS;
        // Tell the hardware: "We want 8-bit transfers" (value 7)
        SPI1->CR2 |= (7 << SPI_CR2_DS_Pos);
    
        // Clear the FIFO Threshold bit
        SPI1->CR2 &= ~SPI_CR2_FRXTH;
        // Tell the hardware: "Raise a flag on 1 byte, not 2 bytes"
        SPI1->CR2 |= SPI_CR2_FRXTH;
    
        // Clear and set the Master Pin Output Enable
        SPI1->CR2 &= ~SPI_CR2_SSOE;
        SPI1->CR2 |= SPI_CR2_SSOE;
        */
}

/**
 * cs_pin state to reset = LOW
 * @brief  Drives the configured Chip Select (CS) pin low.
 * @param  hspi: Pointer to the hardware configuration structure.
 */
void w25q_spi_cs_assert(const w25q_spi_handle_t* hspi) {
    hspi->cs_port->BSRR = ((uint32_t)hspi->cs_pin << 16);
}

/**
 * cs_pin state to set = HIGH
 * @brief  Drives the configured Chip Select (CS) pin high.
 * @param  hspi: Pointer to the hardware configuration structure.
 */
void w25q_spi_cs_deassert(const w25q_spi_handle_t* hspi) {
    hspi->cs_port->BSRR = hspi->cs_pin;
}

/**
 * @brief  Transmits a single byte and clocks back a byte simultaneously (Blocking).
 * @param  hspi: Pointer to the active SPI handle.
 * @param  tx_data: Byte to shift out over MOSI.
 * @param  rx_data: Pointer to capture shifted-in byte over MISO.
 * @retval w25q_spi_status_t: Outcome of transmission timing loops.
 */
w25q_spi_status_t w25q_spi_transfer_byte(const w25q_spi_handle_t* hspi, uint8_t tx_data, uint8_t* rx_data) {
#if !defined(UNIT_TEST)
    volatile uint8_t* dr8 = (volatile uint8_t*)&hspi->spi->DR;
    uint32_t limitout = SPI_IO_CYCLES_LIMIT;

    while (!(hspi->spi->SR & SPI_SR_TXE)) {
        if (--limitout == 0) return SPI_ERROR_TIMEOUT;
    }
    *dr8 = tx_data;

    limitout = SPI_IO_CYCLES_LIMIT;
    while (!(hspi->spi->SR & SPI_SR_RXNE)) {
        if (--limitout == 0) return SPI_ERROR_TIMEOUT;
    }
    uint8_t received = *dr8;

    if (hspi->spi->SR & SPI_SR_OVR) {
        return SPI_ERROR_OVERRUN;
    }

    if (rx_data != NULL) {
        *rx_data = received;
    }
    return SPI_OK;
#else
    if (rx_data != NULL) {
        *rx_data = tx_data;
    }
    return SPI_OK;
    #endif
}

/**
 * @brief  Sends a block of data across MOSI. Discards resulting MISO inputs.
 * @param  hspi: Pointer to the active SPI handle.
 * @param  buffer: Memory address holding target array.
 * @param  length: Total bytes to stream.
 * @retval w25q_spi_status_t: Hardware transfer verification status.
 */
w25q_spi_status_t w25q_spi_transmit(const w25q_spi_handle_t* hspi, const uint8_t* buffer, uint32_t length) {
    /**
     * about SPI_ERROR_PARAM,
     * - if the (hspi != NULL && buffer != NULL) condition faile, we dits return error
     * - if it pass we lock assert
     * - if len == 0,
     *      - we wait for not BSY,
     *      - deassert
     *
     * I prefere a safer runtime then assert() statment that crush only in debug.
     */
    w25q_spi_status_t rc = SPI_ERROR_PARAM;
    const uint8_t* buff = buffer;
    if (hspi != NULL && buffer != NULL) {
        w25q_spi_cs_assert();
        while (length > 0) {
            rc = w25q_spi_transfer_byte(hspi, *buff++, NULL);
            if (rc != SPI_OK) {
                break;
            }
            length--;
        }
        while (hspi->spi->SR & SPI_SR_BSY) {}
        w25q_spi_cs_deassert();
    }
    return rc;
}

if
(hspi
==
NULL
||
buffer
==
NULL
||
length
==
0
)
 {
    return SPI_ERROR_PARAM;
}


/**
 * @brief  Receives a block of data across MISO by writing standard 0xFF dummy bytes.
 * @param  hspi: Pointer to the active SPI handle.
 * @param  buffer: Target destination memory allocation.
 * @param  length: Total bytes expected to read.
 * @retval w25q_spi_status_t: Hardware shift success code.
 */
w25q_spi_status_t w25q_spi_receive(const w25q_spi_handle_t* hspi, uint8_t* buffer, uint32_t length);
