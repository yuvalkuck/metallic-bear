#include "bme688_i2c.h"

bme688_i2c_status_t bme688_i2c_init(const bme688_i2c_handle_t* hi2c) {
    if (hi2c == NULL || hi2c->i2c == NULL || hi2c->tim == NULL ||
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

/**
 * Just copy from AI and adjust to my way of code
 * @param period
 * @param intf_ptr
 */
void bme688_delay_us(uint32_t period, void* intf_ptr) {
    if (period == 0 || intf_ptr == NULL) {
        return;
    }
    bme688_i2c_handle_t* handle = intf_ptr;
    TIM_TypeDef* TIMx = handle->tim;

    // Ensure counter is disabled while configuring timing settings
    TIMx->CR1 &= ~TIM_CR1_CEN;

    // 1. Calculate Prescaler for 1 microsecond ticking resolution.
    // This scales the counter down to step exactly once every 1 us.
    TIMx->PSC = (uint16_t)((SystemCoreClock / 1000000U) - 1U);

    // 2. Assign the target microsecond wait value as the Auto-Reload ceiling limit
    TIMx->ARR = (uint16_t)(period & 0xFFFFU);

    // 3. Clear dynamic flag registers to purge transient states
    TIMx->SR = 0;

    // 4. Force Update Generation (UG) to latch the programmed PSC mapping instantly
    TIMx->EGR |= TIM_EGR_UG;
    TIMx->SR = 0; // Immediately clear the update flag raised by the hardware UG trigger

    // 5. Enable the timer engine
    TIMx->CR1 |= TIM_CR1_CEN;

    // 6. Block until hardware asserts the Update Interrupt Flag (UIF) signaling ARR match
    while (!(TIMx->SR & TIM_SR_UIF)) {
        // Execute safe context spin block
    }

    // 7. Deactivate the hardware block and reset flags for next routine pass
    TIMx->CR1 &= ~TIM_CR1_CEN;
    TIMx->SR = 0;
}

/**
 * Read start with write because it a transaction, open transaction, write to the device to send something, read it & close transaction
 * because it actually 2 action, the auto-end is Off between first and second.
 * @param  reg_addr
 * @param  reg_data
 * @param  datalen
 * @param  intf_ptr
 * @return uint8_t
 */
int8_t bme688_i2c_bus_read(uint8_t reg_addr, uint8_t* reg_data, uint32_t datalen, void* intf_ptr) {
    bme688_i2c_handle_t* handle = intf_ptr;
    I2C_TypeDef* I2Cx = handle->i2c;
    return BME68X_E_NULL_PTR;
}

int8_t bme688_i2c_bus_write(uint8_t reg_addr, const uint8_t* reg_data, uint32_t len, void* intf_ptr) {
    bme688_i2c_handle_t* handle = intf_ptr;
    I2C_TypeDef* I2Cx = handle->i2c;
    return BME68X_E_NULL_PTR;
}
