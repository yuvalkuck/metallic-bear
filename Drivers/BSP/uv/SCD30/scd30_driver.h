#ifndef SCD30_DRIVER_H
#define SCD30_DRIVER_H
#include "scd30_uart.h"

typedef struct {
    const scd30_uart_handle_t* hbus; /* Reference injection to the low-level bus */
    scd30_read_properties_t read_props;
    uint8_t is_initialized; /* Internal tracker boolean flag */
} scd30_device_t;

/**
 * @brief SCD30 Modbus Protocol Exception Codes
 * Returned by the sensor when a packet is successfully received but cannot be processed.
 */
typedef enum {
    MODBUS_OK                    = 0x00, /**< Success / No Exception */
    MODBUS_ERROR_ILLEGAL_FUNC    = 0x01, /**< Function code not supported (Only 0x03, 0x04, 0x06 allowed) */
    MODBUS_ERROR_ILLEGAL_ADDR    = 0x02, /**< Register address out of bounds or doesn't exist */
    MODBUS_ERROR_ILLEGAL_VAL     = 0x03, /**< Payload value out of range (e.g., measurement interval < 2s) */
    MODBUS_ERROR_DEVICE_FAIL     = 0x04, /**< Sensor internal error or EEPROM write failure */
    MODBUS_ERROR_PARAMETER       = 0x05,
    MODBUS_ERROR_NOT_IMPLEMENTED = 0x06
} scd30_status_t;

typedef void (*scd30_read_cb_t)(uint8_t *rx, uint16_t len);
scd30_status_t scd30_init(scd30_device_t* device, const scd30_uart_handle_t* hbus, scd30_read_cb_t);


#endif
