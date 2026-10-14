#ifndef MQ7_DRIVER_H
#define MQ7_DRIVER_H
#include "mq7_adc.h"

typedef enum {
    MQ7_ADC_READY = 0, /* no transfer in flight, write() allowed */
    MQ7_ADC_READING,   /* request sent, DMA collecting the reply */
    MQ7_ADC_DONE,      /* reply captured, callback pending       */
} mq7_adc_state_t;

typedef struct {
    TIM_TypeDef* timer;           /* cycle timer, counts at 10 kHz */
    const mq7_adc_handle_t* hbus; /* Reference injection to the low-level bus */
    volatile mq7_adc_state_t state;
    mq7_adc_read_cb_t read_cb;
    uint8_t is_initialized; /* Internal tracker boolean flag */
} mq7_device_t;

typedef enum {
    MQ7_OK                    = 0x00, /**< Success / No Exception */
    MQ7_ERROR_DEVICE_FAIL     = 0x04, /**< Sensor internal error or EEPROM write failure */
    MQ7_ERROR_PARAMETER       = 0x05,
    MQ7_ERROR_NOT_IMPLEMENTED = 0x06
} mq7_status_t;


typedef void (*mq7_read_cb_t)(uint16_t*);
mq7_status_t mq7_init(mq7_device_t* device, const mq7_adc_handle_t* hbus, mq7_read_cb_t);
mq7_status_t mq7_run(mq7_device_t* device);
void mq7_stop(mq7_device_t* device);
void mq7_timer_irq(void); /* from TIM7_DAC_IRQHandler */

#endif
