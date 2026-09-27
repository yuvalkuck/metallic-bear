#include "bme688_driver.h"

bme688_status_t bme688_init(bme688_device_t *device, const bme688_i2c_handle_t *bus) {
    device->i2c_bus = bus;
    device->is_initialized = 0;
    bme688_status_t rc = bme688_verify_chip_id(device);
    if (rc != BME688_OK) {
        return rc;
    }
    device->is_initialized = 1;
    return BME688_OK;
}

bme688_status_t bme688_verify_chip_id(bme688_device_t *hw_handle) {
    uint8_t rx_buffer = 0;
    int8_t status = bme688_i2c_bus_read(BME68X_REG_CHIP_ID, &rx_buffer, 1U, (void *)hw_handle->i2c_bus);
    if ( status != I2C_OK ) {
        return BME688_ERROR_I2C_FAIL;
    }
    if ( rx_buffer != BME68X_CHIP_ID ) {
        return BME688_ERROR_ID_MISMATCH;
    }
    return BME688_OK;
}

