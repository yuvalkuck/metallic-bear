#include "bme688_i2c.h"

bme688_i2c_status_t bme688_i2c_init(const bme688_i2c_handle_t* hi2c) {
    if (hi2c == NULL || hi2c->i2c == NULL ||
        (hi2c->device != BME68X_I2C_ADDR_LOW && hi2c->device != BME68X_I2C_ADDR_HIGH)) {
        return I2C_ERROR_PARAM;
    }
    if (!(hi2c->i2c->CR1 & I2C_CR1_PE)) {
        return I2C_ERROR_SELF_TEST;
    }

    return I2C_OK;
}
