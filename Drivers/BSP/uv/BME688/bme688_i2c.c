#include "bme688_i2c.h"

bme688_i2c_status_t bme688_i2c_init(const bme688_i2c_handle_t* hi2c) {
    if (hi2c == NULL || hi2c->i2c == NULL ||
        (hi2c->device != BME68X_I2C_ADDR_LOW && hi2c->device != BME68X_I2C_ADDR_HIGH)) {
        return I2C_ERROR_PARAM;
    }
    I2C_TypeDef* I2Cx = hi2c->i2c;
    if (!(I2Cx->CR1 & I2C_CR1_PE)) {
        return I2C_ERROR_SELF_TEST;
    }

    uint32_t timeout = 50000;
    while (!(I2Cx->ISR & I2C_ISR_STOPF)) {
        // If the BME688 responds with a NACK, clear flags and catch it immediately
        if (I2Cx->ISR & I2C_ISR_NACKF) { // on NACK flag we need to exit now.
            I2Cx->ICR |= I2C_ICR_NACKCF; // Clear NACK flag
            I2Cx->ICR |= I2C_ICR_STOPCF; // Clear STOP flag
            return I2C_ERROR_SELF_TEST;
        }

        // Decrement our safety counter
        if (--timeout == 0) {
            // Bus is completely locked up or shorted out! Clear the line and exit.
            I2Cx->CR1 &= ~I2C_CR1_PE; // Disable the I2C block to reset its state
            I2Cx->CR1 |= I2C_CR1_PE; // Re-enable it
            return I2C_ERROR_TIMEOUT; // Return timeout error code instead of freezing
        }
    }

    // Clean up the STOP flag from our successful ping transmission
    I2Cx->ICR |= I2C_ICR_STOPCF;
    return I2C_OK;
}
