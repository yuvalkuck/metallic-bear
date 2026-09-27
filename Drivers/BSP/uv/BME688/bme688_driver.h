#ifndef BEARMETAL_BME688_DRIVER_H
#define BEARMETAL_BME688_DRIVER_H
#include "bme688_i2c.h"

typedef struct {
    const bme688_i2c_handle_t* i2c_bus; /* Reference injection to the low-level bus */
    uint8_t chip_id;                  /* should be BME68X_CHIP_ID */
    uint8_t is_initialized;             /* Internal tracker boolean flag */
    struct bme68x_dev api;
} bme688_device_t;

typedef enum {
    BME688_ERROR_SELF_TEST = BME68X_E_SELF_TEST,
    BME688_OK                  = 0x00,
    BME688_ERROR_I2C_FAIL      = 0x01,
    BME688_ERROR_ID_MISMATCH   = 0x02,
    BME688_ERROR_DEVICE_INIT   = 0x03,
    BME688_ERROR_PARAM         = 0x06,
    BME688_ERROR_NOT_IMPLEMENT = 0x0A,
} bme688_status_t;

bme688_status_t bme688_init(bme688_device_t* device, const bme688_i2c_handle_t* bus);
bme688_status_t bme688_verify_chip_id(bme688_device_t* hw_handle);
#endif
