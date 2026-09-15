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

#define SPI_DUMMY_RECEIVE 0x00
#define SPI_DUMMY_TRANSMIT 0xFF

typedef enum {
    /* Write Control Commands */
    W25Q_CMD_WRITE_ENABLE              = 0x06, // Sets WEL bit
    W25Q_CMD_VOLATILE_SR_WRITE_ENABLE  = 0x50, // Write Enable for Volatile Status Register
    W25Q_CMD_WRITE_DISABLE             = 0x04, // Resets WEL bit

    /* Register Access Commands */
    W25Q_CMD_READ_STATUS_REG1          = 0x05, // Read Status Register 1
    W25Q_CMD_READ_STATUS_REG2          = 0x35, // Read Status Register 2
    W25Q_CMD_READ_STATUS_REG3          = 0x15, // Read Status Register 3
    W25Q_CMD_WRITE_STATUS_REG1         = 0x01, // Write Status Register 1
    W25Q_CMD_WRITE_STATUS_REG2         = 0x31, // Write Status Register 2
    W25Q_CMD_WRITE_STATUS_REG3         = 0x11, // Write Status Register 3
    W25Q_CMD_READ_SFDP_REG             = 0x5A, // Read SFDP Register

    /* Read Data Commands */
    W25Q_CMD_READ_DATA                 = 0x03, // Read Data (Normal, 3-Byte Address)
    W25Q_CMD_FAST_READ                 = 0x0B, // Fast Read Data
    W25Q_CMD_FAST_READ_DUAL_OUTPUT     = 0x3B, // Fast Read Dual Output
    W25Q_CMD_FAST_READ_QUAD_OUTPUT     = 0x6B, // Fast Read Quad Output
    W25Q_CMD_FAST_READ_DUAL_IO         = 0xBB, // Fast Read Dual I/O
    W25Q_CMD_FAST_READ_QUAD_IO         = 0xEB, // Fast Read Quad I/O

    /* Program and Erase Commands */
    W25Q_CMD_PAGE_PROGRAM              = 0x02, // Page Program (Up to 256 bytes)
    W25Q_CMD_QUAD_PAGE_PROGRAM         = 0x32, // Quad Input Page Program
    W25Q_CMD_SECTOR_ERASE_4KB          = 0x20, // Sector Erase (4KB)
    W25Q_CMD_BLOCK_ERASE_32KB          = 0x52, // Block Erase (32KB)
    W25Q_CMD_BLOCK_ERASE_64KB          = 0xD8, // Block Erase (64KB)
    W25Q_CMD_CHIP_ERASE                = 0xC7, // Chip Erase Alternate 1
    W25Q_CMD_CHIP_ERASE_ALT            = 0x60, // Chip Erase Alternate 2

    /* Power Management / Suspend */
    W25Q_CMD_ERASE_PROG_SUSPEND        = 0x75, // Suspend Erase or Program
    W25Q_CMD_ERASE_PROG_RESUME         = 0x7A, // Resume Erase or Program
    W25Q_CMD_POWER_DOWN                = 0xB9, // Deep Power Down
    W25Q_CMD_RELEASE_POWER_DOWN_ID     = 0xAB, // Release Power Down / Read Device ID

    /* ID and Security Commands */
    W25Q_CMD_MANUFACTURER_DEVICE_ID    = 0x90, // Read Manufacturer / Device ID
    W25Q_CMD_JEDEC_ID                  = 0x9F, // Read JEDEC ID
    W25Q_CMD_READ_UNIQUE_ID            = 0x4B, // Read Unique ID
    W25Q_CMD_ERASE_SECURITY_REG        = 0x44, // Erase Security Registers (3x256 bytes)
    W25Q_CMD_PROGRAM_SECURITY_REG      = 0x42, // Program Security Registers
    W25Q_CMD_READ_SECURITY_REG         = 0x48, // Read Security Registers

    /* 4-Byte Address Mode Commands (W25Q256 and higher) */
    W25Q_CMD_ENTER_4BYTE_ADDR_MODE     = 0xB7, // Enter 4-Byte Address Mode
    W25Q_CMD_EXIT_4BYTE_ADDR_MODE      = 0xE9, // Exit 4-Byte Address Mode

    /* Reset Commands */
    W25Q_CMD_ENABLE_RESET              = 0x66, // Enable Software Reset
    W25Q_CMD_RESET_DEVICE              = 0x99  // Execute Software Reset
} w25q_command_t;

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
