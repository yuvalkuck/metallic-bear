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

typedef enum {
    SCD30_REG_START_MEASUREMENT          = 0x0036, // FC06: Start continuous measurement with pressure (mBar) offset
    SCD30_REG_STOP_MEASUREMENT           = 0x0037, // FC06: Stop measurement (always write 0x0001)
    SCD30_REG_MEASUREMENT_INTERVAL       = 0x0025, // FC03/06: Interval time in seconds (2 to 1800)
    SCD30_REG_AUTOMATIC_SELF_CALIBRATION = 0x003A, // FC03/06: ASC toggle (0x0000 = Disable, 0x0001 = Enable)
    SCD30_REG_FORCED_RECALIBRATION       = 0x0039, // FC03/06: FRC target value (400 to 2000 ppm)
    SCD30_REG_TEMPERATURE_OFFSET         = 0x003B, // FC03/06: Temperature offset in 0.01°C steps
    SCD30_REG_ALTITUDE_COMPENSATION      = 0x0038, // FC03/06: Height above sea level in meters
    SCD30_REG_FIRMWARE_VERSION           = 0x0020, // FC03: Firmware version major/minor byte
    SCD30_REG_SOFT_RESET                 = 0x0034, // FC06: Soft reset sensor (always write 0x0001)
    SCD30_REG_DATA_READY_STATUS          = 0x0027, // FC04: Data status (0x0001 = Data ready to read)
    SCD30_REG_READ_MEASUREMENT_BLOCK     = 0x0028  // FC04: Read 6 consecutive registers for CO2, Temp, and RH floats
} scd30_register_t;


typedef void (*scd30_read_cb_t)(uint8_t* rx, uint16_t len);
scd30_status_t scd30_init(scd30_device_t* device, const scd30_uart_handle_t* hbus, scd30_read_cb_t);
scd30_status_t scd30_get_fiemware_version(scd30_device_t* device, uint8_t* major, uint8_t* minor);

#endif
