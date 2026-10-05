#ifndef SCD30_DRIVER_H
#define SCD30_DRIVER_H
#include "scd30_uart.h"

typedef struct {
    uint8_t *rx_buffer;
    uint16_t rx_cap;
    uint32_t start_ms;
    volatile scd30_uart_state_t state;
} scd30_read_properties_t;

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
    MODBUS_ERROR_NOT_IMPLEMENTED = 0x05
} scd30_status_t;

scd30_status_t scd30_init(scd30_device_t* device, const scd30_uart_handle_t* hbus);

#endif
