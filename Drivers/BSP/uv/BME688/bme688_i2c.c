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
            I2Cx->CR1 |= I2C_CR1_PE;  // Re-enable it
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
 * @brief Zero-overhead inline helper to poll I2C status register flags with a timeout.
 *        Optimized by the compiler to eliminate code duplication without a function call penalty.
 */
static inline int8_t _i2c_poll_flag(I2C_TypeDef* I2Cx, uint32_t flag, uint32_t expected_mask) {
    uint32_t timeout = 150000U;

    // Pure bitwise match: loops as long as the filtered bit does not match the target mask
    while ((I2Cx->ISR & flag) != expected_mask) {
        // Immediately intercept and clear NACK errors to avoid a dead-lock hang
        if (I2Cx->ISR & I2C_ISR_NACKF) {
            I2Cx->ICR |= I2C_ICR_NACKCF;
            return I2C_ERROR_COMM; // BME68X_E_COM_FAIL
        }

        if (--timeout == 0) {
            return I2C_ERROR_COMM; // BME68X_E_COM_FAIL
        }
    }
    return I2C_OK; // Success
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
    if ((reg_data == NULL) || (intf_ptr == NULL) || (datalen == 0)) {
        return I2C_ERROR_COMM; // BME68X_E_COM_FAIL
    }
    bme688_i2c_handle_t* handle = intf_ptr;
    I2C_TypeDef* I2Cx = handle->i2c;
    if (I2Cx == NULL) {
        return I2C_ERROR_COMM; // BME68X_E_COM_FAIL
    }
    // just in case
    if (_i2c_poll_flag(I2Cx, I2C_ISR_BUSY, 0U) != 0) { return I2C_ERROR_COMM; }
    // cycle is write what you what to read & then wait to read it
    /**
     * PHASE I: write to the device and live the line open
     */
    //The CR2 register contains bits that you configure once at startup and must never erase or overwrite during a read or write transaction.
    // must modify it content and not just override
    uint32_t cr2_reg = I2Cx->CR2;
    // first remove what we do not want
    cr2_reg &= ~(I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND | I2C_CR2_START | I2C_CR2_STOP);
    // add what need to happen next
    cr2_reg |= (((uint32_t)(handle->device << 1) & I2C_CR2_SADD) |
        (1U << I2C_CR2_NBYTES_Pos) |
        I2C_CR2_START);
    // make it happen.
    I2Cx->CR2 = cr2_reg;

    if (_i2c_poll_flag(I2Cx, I2C_ISR_TXIS, I2C_ISR_TXIS) != 0) { return I2C_ERROR_COMM; }
    /**
     * Q: If the CPU doesn't write to TXDR fast enough after TXIS clears, won't the hardware time out, drop the ball, or cause a race condition?
     * A: the STM32 streach the clock until it get data in the register, it will not send conetetr of TXRD until it was updated.
     */
    I2Cx->TXDR = reg_addr; // set what to send
    // wait until the action ends or timeout, Because AUTOEND is disabled, we just need to know it Complete Transmitted
    if (_i2c_poll_flag(I2Cx, I2C_ISR_TC, I2C_ISR_TC) != 0) { return I2C_ERROR_COMM; }

    /**
    * PHASE II: read from the device
    */
    // same as before, read, clear instructions, add intructions and go
    // Read CR2, subtract old Phase 1 bits via clear mask, and assign the Phase 2 blueprint
    cr2_reg = I2Cx->CR2 & ~(I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND | I2C_CR2_START |
        I2C_CR2_STOP);
    cr2_reg |= (((uint32_t)(handle->device << 1) & I2C_CR2_SADD) |
        ((datalen << I2C_CR2_NBYTES_Pos) & I2C_CR2_NBYTES) |
        I2C_CR2_RD_WRN |  // AI: Invert transaction flow to input read mode
        I2C_CR2_AUTOEND | // AI: Instruct engine to send a Stop sequence on the final byte
        I2C_CR2_START);   // AI: Generate a Repeated START on the wide open bus line
    I2Cx->CR2 = cr2_reg;

    // Read block loop
    for (uint32_t ii = 0; ii < datalen; ii++) {
        // Wait for dynamic byte capture notification via Receive Not Empty (RXNE) flag
        if (_i2c_poll_flag(I2Cx, I2C_ISR_RXNE, I2C_ISR_RXNE) != 0) { return I2C_ERROR_COMM; }

        // Harvest the captured data byte straight into destination
        reg_data[ii] = (uint8_t)(I2Cx->RXDR);
    }
    // Wait until the hardware-driven AUTOEND scheduler achieves complete physical bus line closure
    if (_i2c_poll_flag(I2Cx, I2C_ISR_STOPF, I2C_ISR_STOPF) != 0) { return I2C_ERROR_COMM; }
    // clear the stop after a transaction completely finishes.
    I2Cx->ICR |= I2C_ICR_STOPCF;

    return I2C_OK;
}

int8_t bme688_i2c_bus_write(uint8_t reg_addr, const uint8_t* reg_data, uint32_t datalen, void* intf_ptr) {
    if ((reg_data == NULL) || (intf_ptr == NULL)) {
        return I2C_ERROR_COMM;
    }
    bme688_i2c_handle_t* handle = intf_ptr;
    I2C_TypeDef* I2Cx = handle->i2c;
    if (I2Cx == NULL) {
        return I2C_ERROR_COMM;
    }
    // just in case
    if (_i2c_poll_flag(I2Cx, I2C_ISR_BUSY, 0U) != 0) { return I2C_ERROR_COMM; }
    // same as in read, remove, add and set
    uint32_t cr2_reg = I2Cx->CR2 & ~(I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND | I2C_CR2_START |
        I2C_CR2_STOP);
    cr2_reg |= (((uint32_t)(handle->device << 1) & I2C_CR2_SADD) |
        (((1U + datalen) << I2C_CR2_NBYTES_Pos) & I2C_CR2_NBYTES) |
        I2C_CR2_AUTOEND |
        I2C_CR2_START);
    I2Cx->CR2 = cr2_reg;

    if (_i2c_poll_flag(I2Cx, I2C_ISR_TXIS, I2C_ISR_TXIS) != 0) { return I2C_ERROR_COMM; }
    I2Cx->TXDR = reg_addr; // set what to send

    for (uint32_t ii = 0; ii < datalen; ii++) {
        if (_i2c_poll_flag(I2Cx, I2C_ISR_TXIS, I2C_ISR_TXIS) != 0) { return I2C_ERROR_COMM; }
        I2Cx->TXDR = reg_data[ii];
    }
    // Wait until the hardware-driven AUTOEND scheduler achieves complete physical bus line closure
    if (_i2c_poll_flag(I2Cx, I2C_ISR_STOPF, I2C_ISR_STOPF) != 0) { return I2C_ERROR_COMM; }
    // clear the stop after a transaction completely finishes.
    I2Cx->ICR |= I2C_ICR_STOPCF;

    return I2C_OK;
}
