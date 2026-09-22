#ifndef BEARMETAL_BME688_DRIVER_H
#define BEARMETAL_BME688_DRIVER_H
#include "bme688_i2c.h"

/* --- JEDEC Identification Storage Struct --- */
typedef struct {
    uint8_t manufacturer_id; /* Expected output: 0xEF (Winbond) */
    uint8_t memory_type_id; /* Expected output: 0x40 */
    uint8_t capacity_id; /* Expected output: 0x18 (128M-bit) */
} bme688_id_t;

typedef struct {
    const bme688_i2c_handle_t* i2c_bus; /* Reference injection to the low-level bus */
    bme688_id_t id; /* Parsed chip hardware confirmation data */
    uint8_t     is_initialized; /* Internal tracker boolean flag */
} bme688_device_t;

#endif
