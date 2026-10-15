#include <stddef.h>

#include "mq7_driver.h"
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_tim.h"
mq7_device_t *mq7_dev_irq_ctx;

#define MQ7_HEAT_MS 60000U
#define MQ7_COOL_MS 90000U
// TIM2 counts 10 kHz and stops itself (one-pulse): write ARR, then set CEN
static void timer_arm_(TIM_TypeDef* timer, uint32_t ms) {
    LL_TIM_SetAutoReload(timer, ms * 10U - 1U);
    LL_TIM_EnableCounter(timer);
}

static void mq7_read_cb(uint16_t value);

mq7_status_t mq7_init(mq7_device_t* device, mq7_adc_handle_t* hbus, mq7_read_cb_t read_cb) {
    if (device == NULL || hbus == NULL || read_cb == NULL) {
        return MQ7_ERROR_PARAMETER;
    }

    device->is_initialized = 0;
    device->hbus = hbus;

    device->read_cb = read_cb;
    hbus->read_cb = mq7_read_cb;  // the ADC layer reports up through this pointer
    uint16_t test_value;
    if (mq7_adc_sample(hbus, &test_value) != ADC_OK) {
        return MQ7_ERROR_DEVICE_FAIL;
    }
    mq7_dev_irq_ctx = device;
    device->is_initialized = 1;
    return MQ7_OK;
}

mq7_status_t mq7_run(mq7_device_t* device) {
    if (device == NULL || !device->is_initialized) {
        return MQ7_ERROR_PARAMETER;
}
    LL_GPIO_SetOutputPin(device->ht_port, device->ht_pin);   // 5.0 V heat phase
    LL_TIM_ClearFlag_UPDATE(device->timer);
    LL_TIM_EnableIT_UPDATE(device->timer);
    timer_arm_(device->timer, MQ7_HEAT_MS);
    return MQ7_OK;
}

mq7_status_t mq7_stop(mq7_device_t* device) {
    if (device == NULL || !device->is_initialized) {
        return MQ7_ERROR_PARAMETER;
    }
    LL_TIM_DisableIT_UPDATE(device->timer);   // first, so no phase handler runs from here on
    LL_TIM_DisableCounter(device->timer);
    LL_TIM_ClearFlag_UPDATE(device->timer);
    mq7_adc_error_t err = mq7_adc_stop(device->hbus);
    LL_GPIO_ResetOutputPin(device->ht_port, device->ht_pin);  // 1.4 V
    return (err == ADC_OK) ? MQ7_OK : MQ7_ERROR_TIMEOUT;
}
void mq7_timer_irq(const mq7_device_t* handle) {
    if (!LL_TIM_IsActiveFlag_UPDATE(handle->timer)) {
        return;
    }
    LL_TIM_ClearFlag_UPDATE(handle->timer);
    if (LL_GPIO_IsOutputPinSet(handle->ht_port, handle->ht_pin)) {
        // heat phase ended: go to 1.4 V cool phase
        LL_GPIO_ResetOutputPin(handle->ht_port, handle->ht_pin);
        timer_arm_(handle->timer, MQ7_COOL_MS);
    } else {
        // cool phase ended: sample first, heat resumes in mq7_read_cb (EOC interrupt)
        mq7_adc_start(handle->hbus);
    }
}

// called from the ADC EOC interrupt once the sample is ready, then the heat phase starts
static void mq7_read_cb(uint16_t value) {
    if (!LL_TIM_IsEnabledIT_UPDATE(mq7_dev_irq_ctx->timer)) {
        return;   // stopped while the sample was in flight: do not restart the heat
    }
    mq7_dev_irq_ctx->read_cb(value);
    LL_GPIO_SetOutputPin(mq7_dev_irq_ctx->ht_port, mq7_dev_irq_ctx->ht_pin);
    timer_arm_(mq7_dev_irq_ctx->timer, MQ7_HEAT_MS);
}
