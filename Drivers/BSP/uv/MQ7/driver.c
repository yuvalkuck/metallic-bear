#include <stddef.h>

#include "mq7_driver.h"
static mq7_read_cb_t s_mq7_process;

static inline void timer_arm_(TIM_TypeDef* tim, uint32_t ms) { // one-pulse: fires once, then stops by itself
    tim->ARR = ms * 10U - 1U;                                  // 10 kHz: 10 counts per ms
    tim->CNT = 0;
    tim->CR1 |= TIM_CR1_CEN;
}

mq7_status_t mq7_init(mq7_device_t* device, const mq7_adc_handle_t* hbus, mq7_read_cb_t read_cb) {
    if (device == NULL || hbus == NULL || read_cb == NULL) {
        return MQ7_ERROR_PARAMETER;
    }

    device->is_initialized = 0;
    device->hbus = hbus;
    s_mq7_process = read_cb;
    uint16_t test_value;
    if (mq7_adc_sample(hbus, &test_value) != ADC_OK) {
        return MQ7_ERROR_DEVICE_FAIL;
    }
    device->is_initialized = 1;
    return MQ7_OK;
}

mq7_status_t mq7_run(mq7_device_t* device) {
    return MQ7_ERROR_NOT_IMPLEMENTED;
}

void mq7_stop(mq7_device_t* device) {}
void mq7_timer_irq(void) {}
