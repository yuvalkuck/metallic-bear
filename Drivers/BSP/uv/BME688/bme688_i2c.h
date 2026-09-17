#ifndef BEARMETAL_BME688_I2C_H
#define BEARMETAL_BME688_I2C_H
#include "stm32g474xx.h"
typedef struct {
    I2C_TypeDef   *i2c;
    // GPIO_TypeDef  *cs_port;      /* GPIO Port for Chip Select (e.g., GPIOA) */
    // uint16_t       cs_pin;       /* GPIO Pin for Chip Select (e.g., GPIO_BSRR_BS_4) */
} bme688_i2c_handle_t;

#endif
