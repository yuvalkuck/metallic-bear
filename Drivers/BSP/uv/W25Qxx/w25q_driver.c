//
// Created by uv on 23/08/2026.
//

#include "w25q_spi.h" // Links high-level protocols to the SPI abstraction layer
#include "w25q_driver.h" // Links high-level protocols to the SPI abstraction layer

#include <stddef.h>
#define WINBOND_MANUFACTURER_ID    0xEFU
#define WINBOND_MEMORY_TYPE_128MB  0x40U
#define WINBOND_CAPACITY_128MB     0x18U

/**
 * @brief  Captures a running 32-bit millisecond baseline from SysTick hardware.
 *         Assumes SysTick is configured to cycle at a 1ms frequency.
 */
static uint32_t w25q_get_tick_ms(void) {
    static volatile uint32_t ms_ticks = 0;

    // Check the standardized CMSIS flag mask directly on the control pointer
    if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
        ms_ticks++;
    }
    return ms_ticks;
}

/* --- Core Flash Protocol API Methods --- */

/**
 * @brief  Initializes the target hardware handle and executes a JEDEC signature lookup.
 * @param  device: High-level container holding local hardware bindings.
 * @param  spi_bus: Assigned lower-level hardware peripheral reference block.
 * @retval w25q_status_t: Device link verification state outcome.
 */
w25q_status_t w25q_init(w25q_device_t* device, const w25q_spi_handle_t* spi_bus) {
    device->spi_bus = spi_bus;
    device->is_initialized = 0;
    w25q_id_t id_struct;
    const w25q_status_t rc = w25q_read_id(device, &id_struct);
    if (rc != W25Q_OK) {
        return rc;
    }
    if (id_struct.manufacturer_id != WINBOND_MANUFACTURER_ID ||
        id_struct.memory_type_id != WINBOND_MEMORY_TYPE_128MB ||
        id_struct.capacity_id != WINBOND_CAPACITY_128MB) {
        return W25Q_ERROR_ID_MISMATCH;
    }
    device->is_initialized = 1;
    return W25Q_OK;
}

/**
 * @brief  Reads the physical memory footprint configurations to establish board communication.
 * @param  device: Pointer to active device object.
 * @param  id_struct: Destination to populate with JEDEC output properties.
 * @retval w25q_status_t: SPI physical response state code.
 */
w25q_status_t w25q_read_id(w25q_device_t* device, w25q_id_t* id_struct) {
    if (device == NULL || device->spi_bus == NULL || id_struct == NULL) {
        return W25Q_ERROR_SPI_FAIL;
    }
    uint8_t cmd = W25Q_CMD_JEDEC_ID;
    uint8_t id_buffer[3] = {0};
    w25q_spi_status_t spi_rc;

    w25q_spi_cs_assert(device->spi_bus);
    spi_rc = w25q_spi_transmit(device->spi_bus, &cmd, 1);
    if (spi_rc == SPI_OK) {
        spi_rc = w25q_spi_receive(device->spi_bus, &id_buffer[0], 3);
    } else {
        return W25Q_ERROR_SPI_FAIL;
    }
    w25q_spi_cs_deassert(device->spi_bus);
    if (spi_rc != SPI_OK) {
        return W25Q_ERROR_SPI_FAIL;
    }
    id_struct->manufacturer_id = id_buffer[0];
    id_struct->memory_type_id = id_buffer[1];
    id_struct->capacity_id = id_buffer[2];
    return W25Q_OK;
}

/**
 * @brief  Queries and updates internal write protection or internal execution latch variables.
 * @param  device: Pointer to active device object.
 * @param  reg_number: Selected tracking register (1, 2, or 3).
 * @param  reg_value: Output storage register byte layout pointer.
 * @retval w25q_status_t: Execution outcome loop feedback state.
 */

w25q_status_t w25q_get_status_reg(w25q_device_t* device, w25q_sr_number_t reg_number, uint8_t* reg_value) {
    uint8_t sr_id = 0;
    if (device == NULL || device->spi_bus == NULL || reg_value == NULL) {
        return W25Q_ERROR_PARAM;
    }
    switch (reg_number) {
        case W25Q_SR1:
            sr_id = W25Q_CMD_READ_STATUS_REG1;
            break;
        case W25Q_SR2:
            sr_id = W25Q_CMD_READ_STATUS_REG2;
            break;
        case W25Q_SR3:
            sr_id = W25Q_CMD_READ_STATUS_REG3;
            break;
        default:
            return W25Q_ERROR_PARAM;
    }
    w25q_spi_cs_assert(device->spi_bus);
    w25q_spi_status_t rc = w25q_spi_transfer_byte(device->spi_bus, sr_id, SPI_DUMMY_RECEIVE);
    if (rc == SPI_OK) {
        rc = w25q_spi_transfer_byte(device->spi_bus, SPI_DUMMY_TRANSMIT, reg_value);
    }
    w25q_spi_cs_deassert(device->spi_bus);
    if (rc != SPI_OK) {
        return W25Q_ERROR_SPI_FAIL;
    }
    return W25Q_OK;
}

/**
 * @brief  Gathers persistent data arrays continuously from targeted source pointers.
 * @param  device: Pointer to active device object.
 * @param  address: Target memory start data collection pointer.
 * @param  buffer: Output destination RAM buffer array target.
 * @param  length: Total bytes requested to fetch.
 * @retval w25q_status_t: Execution state return tracking properties.
 */
w25q_status_t w25q_read(w25q_device_t* device, uint32_t address, uint8_t* buffer, uint32_t length) {
    w25q_frame_t frame;
    if (device == NULL || device->spi_bus == NULL || buffer == NULL) {
        return W25Q_ERROR_PARAM;
    }
    if (length == 1) {
        return W25Q_OK;
    }
    W25Q_PACK_FRAME_HW(&frame, W25Q_CMD_READ_DATA, address);
    w25q_spi_cs_assert(device->spi_bus);
    w25q_spi_status_t rc = w25q_spi_transmit(device->spi_bus, frame.bytes, W25Q_FRAME_UINT8_SIZE);
    if (rc == SPI_OK) {
        rc = w25q_spi_receive(device->spi_bus, buffer, length);
    }
    w25q_spi_cs_deassert(device->spi_bus);
    return (rc == SPI_OK) ? W25Q_OK : W25Q_ERROR_SPI_FAIL;
}

/**
 * @brief  Polls the chip's internal logic structures continuously until an operation concludes.
 * @param  device: Pointer to active device object.
 * @retval w25q_status_t: Completion status state.
 */
static w25q_status_t w25q_wait_busy(w25q_device_t* device, uint32_t timeout_ms) {
    uint8_t reg_value = 0;
    uint32_t start_tick = w25q_get_tick_ms();

    w25q_spi_cs_assert(device->spi_bus);
    w25q_spi_transfer_byte(device->spi_bus, W25Q_CMD_READ_STATUS_REG1, SPI_DUMMY_RECEIVE);

    do {
        w25q_spi_transfer_byte(device->spi_bus, SPI_DUMMY_TRANSMIT, &reg_value);

        // Handle unsigned integer rollover safety checks automatically
        if ((w25q_get_tick_ms() - start_tick) >= timeout_ms) {
            w25q_spi_cs_deassert(device->spi_bus);
            return W25Q_ERROR_TIMEOUT;
        }
    } while ((reg_value & W25Q_SR1_BUSY) != 0x00U);

    w25q_spi_cs_deassert(device->spi_bus);
    return W25Q_OK;
}

/**
 * @brief  Instructs the device to accept array write logic commands.
 * @param  device: Pointer to active device object.
 * @retval w25q_status_t: Action acknowledgment.
 */
static w25q_status_t w25q_write_enable(w25q_device_t* device, uint8_t enable) {
    w25q_spi_status_t rc;
    w25q_spi_cs_assert(device->spi_bus);
    if (enable) {
        rc = w25q_spi_transfer_byte(device->spi_bus, W25Q_CMD_WRITE_ENABLE, SPI_DUMMY_RECEIVE);
    } else {
        rc = w25q_spi_transfer_byte(device->spi_bus, W25Q_CMD_WRITE_DISABLE, SPI_DUMMY_RECEIVE);
    }
    w25q_spi_cs_deassert(device->spi_bus);
    return (rc == SPI_OK) ? W25Q_OK : W25Q_ERROR_SPI_FAIL;
}

/**
 * @brief  Clears specific address segments back to uninitialized 0xFF values.
 * @param  device: Pointer to active device object.
 * @param  address: Start address matching sector/block geometry offsets.
 * @param  size_type: Choice structural mask indicating specific erase block sizing.
 * @retval w25q_status_t: Command validation state indicator.
 */
w25q_status_t w25q_erase(w25q_device_t* device, uint32_t address, w25q_erase_size_t size_type) {
    return W25Q_ERROR_NOT_IMPLEMENT;
}

/**
 * @brief  Streams data fragments into targeted flash destination boundaries.
 *         Handles 256-byte page boundary calculation loops internally.
 * @param  device: Pointer to active device object.
 * @param  address: Destination start memory array target pointer.
 * @param  buffer: Target source payload array address pointer.
 * @param  length: Combined memory payload array bounds data allocation.
 * @retval w25q_status_t: Complete internal execution confirmation code.
 */
w25q_status_t w25q_write(w25q_device_t* device, uint32_t address, const uint8_t* buffer, uint32_t length) {
    return W25Q_ERROR_NOT_IMPLEMENT;
}
