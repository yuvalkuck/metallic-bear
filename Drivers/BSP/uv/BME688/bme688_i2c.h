#ifndef BEARMETAL_BME688_I2C_H
#define BEARMETAL_BME688_I2C_H
#include "stm32g474xx.h"
#include "bme68x_defs.h"

typedef struct {
    I2C_TypeDef   *i2c;
    TIM_TypeDef  *tim;
    uint8_t       device;
} bme688_i2c_handle_t;

typedef enum {
    I2C_ERROR_SELF_TEST= BME68X_E_SELF_TEST,
    I2C_OK             = BME68X_OK,
    I2C_ERROR_TIMEOUT  = 0x01,
    I2C_ERROR_OVERRUN  = 0x02,
    I2C_ERROR_BUSY     = 0x03,
    I2C_ERROR_PARAM    = 0x04
} bme688_i2c_status_t;

bme688_i2c_status_t bme688_i2c_init(const bme688_i2c_handle_t *hi2c);
int8_t bme688_i2c_bus_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t bme688_i2c_bus_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
void bme688_delay_us(uint32_t period, void *intf_ptr);


#endif
