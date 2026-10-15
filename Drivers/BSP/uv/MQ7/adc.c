#include <stddef.h>

#include "mq7_adc.h"
#include "stm32g4xx_ll_adc.h"

#define MQ7_ADC_TIMEOUT_LOOPS 100000U
const void *mq7_adc_irq_ctx;

mq7_adc_error_t mq7_adc_init(const mq7_adc_handle_t* handle) {
    if (handle == NULL) {
        return ADC_ERROR_INVALID_PARAMS;
    }
    // Calibration, there is no failed-calibration only Hang there for TIMEOUT
    LL_ADC_StartCalibration(handle->bus, LL_ADC_SINGLE_ENDED);
    uint32_t timeout = MQ7_ADC_TIMEOUT_LOOPS;
    while (LL_ADC_IsCalibrationOnGoing(handle->bus)) {
        if (--timeout == 0) {
            return ADC_ERROR_CALIBRATION_TIMEOUT;
        }
    }
    LL_ADC_ClearFlag_ADRDY(handle->bus);
    LL_ADC_Enable(handle->bus);
    timeout = MQ7_ADC_TIMEOUT_LOOPS;
    while (!LL_ADC_IsActiveFlag_ADRDY(handle->bus)) {
        if (--timeout == 0) {
            /**
             * can happen when:
            * - Regulator not on (ADVREGEN clear).
            * - ADC still in deep power-down.
            * - No ADC clock.
            */
            return ADC_ERROR_TIMEOUT;
        }
    }
    // enable the interrupt
    LL_ADC_EnableIT_EOC(handle->bus);
    mq7_adc_irq_ctx = handle;
    return ADC_OK;
}

/**
 * Blocking single conversion, the EOC interrupt is masked while it polls so the ISR does not take the result.
 * @param handle
 * @param sample return value
 * @return
 */
mq7_adc_error_t mq7_adc_sample(const mq7_adc_handle_t* handle, uint16_t* sample) {
    if (handle == NULL || sample == NULL) {
        return ADC_ERROR_INVALID_PARAMS;
    }

    LL_ADC_DisableIT_EOC(handle->bus);
    LL_ADC_ClearFlag_EOC(handle->bus);
    LL_ADC_REG_StartConversion(handle->bus);

    uint32_t timeout = MQ7_ADC_TIMEOUT_LOOPS;
    while (!LL_ADC_IsActiveFlag_EOC(handle->bus)) {
        if (--timeout == 0) {
            LL_ADC_EnableIT_EOC(handle->bus);
            return ADC_ERROR_TIMEOUT;
        }
    }

    // Returns 0 - 4095, casted and stored into a 16-bit unsigned integer
    *sample = (uint16_t)LL_ADC_REG_ReadConversionData12(handle->bus);
    LL_ADC_EnableIT_EOC(handle->bus);
    return ADC_OK;
}

mq7_adc_error_t mq7_adc_start(const mq7_adc_handle_t* handle) {
    if (handle == NULL) {
        return ADC_ERROR_INVALID_PARAMS;
    }
    LL_ADC_REG_StartConversion(handle->bus);
    return ADC_OK;
}

mq7_adc_error_t mq7_adc_stop(const mq7_adc_handle_t* handle) {
    if (handle == NULL) {
        return ADC_ERROR_INVALID_PARAMS;
    }
    mq7_adc_error_t err = ADC_OK;
    if (LL_ADC_REG_IsConversionOngoing(handle->bus)) {
        LL_ADC_REG_StopConversion(handle->bus);
        uint32_t timeout = MQ7_ADC_TIMEOUT_LOOPS;
        while (LL_ADC_REG_IsStopConversionOngoing(handle->bus)) {
            if (--timeout == 0) {
                err = ADC_ERROR_TIMEOUT;
                break;
            }
        }
    }
    LL_ADC_ClearFlag_EOC(handle->bus);
    return err;
}

void mq7_adc_eoc_irq(const mq7_adc_handle_t* handle) {
    // context validation this is the relevant ADC
    if (LL_ADC_IsActiveFlag_EOC(handle->bus) && handle->read_cb != NULL) {
        handle->read_cb(LL_ADC_REG_ReadConversionData12(handle->bus)); // read clears EOC
    }
}

