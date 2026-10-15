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

typedef void (*mq7_adc_read_cb_t)(mq7_adc_error_t err, uint16_t len);

typedef struct {
    ADC_TypeDef* bus;      // bus type
    GPIO_TypeDef* ht_port; /* GPIO Port for Heater switch*/
    uint16_t ht_pin;       /* GPIO Pin for Heater switch*/
    mq7_adc_read_cb_t read_cb;
} mq7_adc_handle_t;

mq7_adc_error_t mq7_adc_init(const mq7_adc_handle_t* handle);
mq7_adc_error_t mq7_adc_sample(const mq7_adc_handle_t* handle, uint16_t* sample);
// TIMer start
mq7_adc_error_t mq7_adc_start(const mq7_adc_handle_t* handle);
mq7_adc_error_t mq7_adc_stop(const mq7_adc_handle_t* handle);

void mq7_adc_idle_irq(const mq7_adc_handle_t* handle);

#endif
