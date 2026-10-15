#ifndef MQ7_DRIVER_H
#define MQ7_DRIVER_H
#include "mq7_adc.h"

typedef void (*mq7_read_cb_t)(uint16_t value); /* runs in interrupt context */

typedef struct {
    TIM_TypeDef* timer;           /* cycle timer, counts at 10 kHz */
    mq7_adc_handle_t* hbus;       /* Reference injection to the low-level bus */
    GPIO_TypeDef* ht_port;        /* GPIO Port for Heater switch */
    mq7_read_cb_t read_cb;        /* user callback, runs in interrupt context */
    uint16_t ht_pin;              /* GPIO Pin for Heater switch */
    uint8_t is_initialized;       /* Internal tracker boolean flag */
} mq7_device_t;

typedef enum {
    MQ7_OK                    = 0x00, /**< Success / No Exception */
    MQ7_ERROR_DEVICE_FAIL     = 0x04, /**< Sensor internal error or EEPROM write failure */
    MQ7_ERROR_PARAMETER       = 0x05,
    MQ7_ERROR_NOT_IMPLEMENTED = 0x06,
    MQ7_ERROR_TIMEOUT         = 0x07  /**< ADC did not stop in time */
} mq7_status_t;

mq7_status_t mq7_init(mq7_device_t* device, mq7_adc_handle_t* hbus, mq7_read_cb_t);
mq7_status_t mq7_run(mq7_device_t* device);
mq7_status_t mq7_stop(mq7_device_t* device);
void mq7_timer_irq(const mq7_device_t* device);

#endif
