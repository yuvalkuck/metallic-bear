#ifndef MQ7_ADC_H
#define MQ7_ADC_H
#include <stdint.h>
#include "stm32g474xx.h"

typedef enum {
    ADC_OK                        = 0,  /**< Packet transaction completed successfully */
    ADC_ERROR_TIMEOUT             = -1, /**< No response from sensor (Check SEL pin pulled HIGH to VDD) */
    ADC_ERROR_CALIBRATION_TIMEOUT = -2, /**< No response from sensor (Check SEL pin pulled HIGH to VDD) */
    // yes there is a gap here --- reserved
    ADC_ERROR_INVALID_PARAMS      = -4,
    ADC_ERROR_NOT_IMPLEMENTED     = -5,
} mq7_adc_error_t;

typedef struct {
    ADC_TypeDef* bus;                // bus type
    void (*read_cb)(uint16_t value); // set by the driver layer, runs in interrupt context
} mq7_adc_handle_t;

mq7_adc_error_t mq7_adc_init(const mq7_adc_handle_t* handle);
/**
 * Blocking single conversion, the EOC interrupt is masked while it polls
 * @param handle
 * @param sample return value
 * @return
 */
mq7_adc_error_t mq7_adc_sample(const mq7_adc_handle_t* handle, uint16_t* sample);
/** Starts one conversion, the result arrives through the EOC interrupt (read_cb). */
mq7_adc_error_t mq7_adc_start(const mq7_adc_handle_t* handle);
/** Aborts a conversion in flight, waits for it to stop and clears EOC. */
mq7_adc_error_t mq7_adc_stop(const mq7_adc_handle_t* handle);
void mq7_adc_eoc_irq(const mq7_adc_handle_t* handle);

#endif
