//
// Created by uv on 23/08/2026.
//

#ifndef BEARMETAL_W25Q_DRIVER_H
#define BEARMETAL_W25Q_DRIVER_H

#include <stdint.h>
#include "w25q_spi_driver.h" // Links high-level protocols to the SPI abstraction layer

/* --- Flash Operation Status Codes --- */
typedef enum {
    W25Q_OK                  = 0x00,
    W25Q_ERROR_SPI_FAIL      = 0x01,
    W25Q_ERROR_ID_MISMATCH   = 0x02,
    W25Q_ERROR_BOUNDARY      = 0x03,
    W25Q_ERROR_TIMEOUT       = 0x04,
    W25Q_ERROR_WRITE_PROTECT = 0x05
} w25q_status_t;

/* --- Memory Block Erase Sizing Options --- */
typedef enum {
    W25Q_ERASE_SECTOR_4KB  = 0x20,
    W25Q_ERASE_BLOCK_32KB  = 0x52,
    W25Q_ERASE_BLOCK_64KB  = 0xD8,
    W25Q_ERASE_CHIP        = 0xC7
} w25q_erase_size_t;

/* --- JEDEC Identification Storage Struct --- */
typedef struct {
    uint8_t  manufacturer_id; /* Expected output: 0xEF (Winbond) */
    uint8_t  memory_type_id;   /* Expected output: 0x40 */
    uint8_t  capacity_id;     /* Expected output: 0x18 (128M-bit) */
} w25q_id_t;

/* --- W25Q128 Protocol Device Object --- */
typedef struct {
    const w25q_spi_handle_t *spi_bus;       /* Reference injection to the low-level bus */
    w25q_id_t           id;           /* Parsed chip hardware confirmation data */
    uint8_t             is_initialized;/* Internal tracker boolean flag */
} w25q_device_t;

/* --- Core Flash Protocol API Methods --- */

/**
 * @brief  Initializes the target hardware handle and executes a JEDEC signature lookup.
 * @param  device: High-level container holding local hardware bindings.
 * @param  spi_bus: Assigned lower-level hardware peripheral reference block.
 * @retval w25q_status_t: Device link verification state outcome.
 */
w25q_status_t w25q_init(w25q_device_t *device, const w25q_spi_handle_t *spi_bus);

/**
 * @brief  Reads the physical memory footprint configurations to establish board communication.
 * @param  device: Pointer to active device object.
 * @param  id_struct: Destination to populate with JEDEC output properties.
 * @retval w25q_status_t: SPI physical response state code.
 */
w25q_status_t w25q_read_id(w25q_device_t *device, w25q_id_t *id_struct);

/**
 * @brief  Queries and updates internal write protection or internal execution latch variables.
 * @param  device: Pointer to active device object.
 * @param  reg_number: Selected tracking register (1, 2, or 3).
 * @param  reg_value: Output storage register byte layout pointer.
 * @retval w25q_status_t: Execution outcome loop feedback state.
 */
w25q_status_t w25q_get_status_reg(w25q_device_t *device, uint8_t reg_number, uint8_t *reg_value);

/**
 * @brief  Instructs the device to accept array write logic commands.
 * @param  device: Pointer to active device object.
 * @retval w25q_status_t: Action acknowledgment.
 */
w25q_status_t w25q_write_enable(w25q_device_t *device);

/**
 * @brief  Polls the chip's internal logic structures continuously until an operation concludes.
 * @param  device: Pointer to active device object.
 * @retval w25q_status_t: Completion status state.
 */
w25q_status_t w25q_wait_busy(w25q_device_t *device);

/**
 * @brief  Clears specific address segments back to uninitialized 0xFF values.
 * @param  device: Pointer to active device object.
 * @param  address: Start address matching sector/block geometry offsets.
 * @param  size_type: Choice structural mask indicating specific erase block sizing.
 * @retval w25q_status_t: Command validation state indicator.
 */
w25q_status_t w25q_erase(w25q_device_t *device, uint32_t address, w25q_erase_size_t size_type);

/**
 * @brief  Streams data fragments into targeted flash destination boundaries.
 *         Handles 256-byte page boundary calculation loops internally.
 * @param  device: Pointer to active device object.
 * @param  address: Destination start memory array target pointer.
 * @param  buffer: Target source payload array address pointer.
 * @param  length: Combined memory payload array bounds data allocation.
 * @retval w25q_status_t: Complete internal execution confirmation code.
 */
w25q_status_t w25q_write(w25q_device_t *device, uint32_t address, const uint8_t *buffer, uint32_t length);

/**
 * @brief  Gathers persistent data arrays continuously from targeted source pointers.
 * @param  device: Pointer to active device object.
 * @param  address: Target memory start data collection pointer.
 * @param  buffer: Output destination RAM buffer array target.
 * @param  length: Total bytes requested to fetch.
 * @retval w25q_status_t: Execution state return tracking properties.
 */
w25q_status_t w25q_read(w25q_device_t *device, uint32_t address, uint8_t *buffer, uint32_t length);

#endif //BEARMETAL_W25Q_DRIVER_H
