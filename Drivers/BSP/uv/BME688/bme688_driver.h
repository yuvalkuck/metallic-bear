#ifndef BEARMETAL_BME688_DRIVER_H
#define BEARMETAL_BME688_DRIVER_H
#include "bme688_i2c.h"

typedef struct {
    const bme688_i2c_handle_t* i2c_bus; /* Reference injection to the low-level bus */
    uint8_t device_id;                  /* should be BME68X_CHIP_ID */
    uint8_t is_initialized;             /* Internal tracker boolean flag */
} bme688_device_t;

#endif
