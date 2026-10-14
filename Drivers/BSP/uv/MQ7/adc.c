#include <stddef.h>

#include "mq7_adc.h"
#include "stm32g4xx_ll_adc.h"

#define MQ7_ADC_TIMEOUT_LOOPS 100000U

mq7_adc_error_t mq7_adc_init(const mq7_adc_handle_t* handle) {
    return ADC_ERROR_NOT_IMPLEMENTED;
}

// TIMer start
mq7_adc_error_t mq7_adc_start(const mq7_adc_handle_t* handle) {
    return ADC_ERROR_NOT_IMPLEMENTED;
}

mq7_adc_error_t mq7_adc_stop(const mq7_adc_handle_t* handle) {
    return ADC_ERROR_NOT_IMPLEMENTED;
}

mq7_adc_error_t mq7_adc_sample(const mq7_adc_handle_t* handle, uint16_t* sample) {
    if (handle == NULL || sample == NULL) {
        return ADC_ERROR_INVALID_PARAMS;
    }

    LL_ADC_ClearFlag_EOC(handle->bus);
    LL_ADC_REG_StartConversion(handle->bus);

    uint32_t timeout = MQ7_ADC_TIMEOUT_LOOPS;
    while (!LL_ADC_IsActiveFlag_EOC(handle->bus)) {
        if (--timeout == 0) {
            return ADC_ERROR_TIMEOUT;
        }
    }

    // Returns 0 - 4095, casted and stored into a 16-bit unsigned integer
    *sample = (uint16_t)LL_ADC_REG_ReadConversionData12(handle->bus);
    return ADC_OK;
}

void mq7_adc_idle_irq(const mq7_adc_handle_t* handle) {}
