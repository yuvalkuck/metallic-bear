#ifndef SCD30_UART_H
#define SCD30_UART_H
#include <stdint.h>
#include "stm32g474xx.h"

/**
 * @brief SCD30 USART Driver & Link-Layer Error Codes
 * Generated locally by your microcontroller's UART driver during parsing.
 */
typedef enum {
    UART_OK                       = 0,  /**< Packet transaction completed successfully */
    UART_ERROR_TIMEOUT            = -1, /**< No response from sensor (Check SEL pin pulled HIGH to VDD) */
    UART_ERROR_CRC_MISMATCH       = -2, /**< Frame check sequence failed (Bad wiring, wrong baud rate, or noise) */
    UART_ERROR_FRAME_MALFORMED    = -3, /**< Response data length or structuring is invalid */
    UART_ERROR_EXCEPTION_RETURNED = -4, /**< Sensor returned an active Modbus exception code */
    UART_ERROR_INVALID_PARAMS     = -5,
    UART_ERROR_NOT_IMPLEMENTED    = -6,
    UART_ERROR_BUSY               = -7, /**< A request is still in flight */
    UART_ERROR_RX_HW              = -8, /**< USART overrun / framing / noise during the reply */
} scd30_uart_error_t;

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

#define SCD30_UART_TIMEOUT_MS 100U
#define SCD30_MODBUS_ID 0X61

typedef enum {
    SCD30_UART_READY = 0, /* no transfer in flight, write() allowed */
    SCD30_UART_READING,   /* request sent, DMA collecting the reply */
    SCD30_UART_DONE,      /* reply captured, callback pending       */
} scd30_uart_state_t;

typedef void (*scd30_uart_read_cb_t)(scd30_uart_error_t err, uint16_t len);

typedef struct {
    USART_TypeDef* bus;    // bus type
    DMA_TypeDef* dma_ctrl; // dma controller
    DMA_Channel_TypeDef* dma_tx;
    DMA_Channel_TypeDef* dma_rx;
    uint8_t tx_ch; // channel index
    uint8_t rx_ch; // channel index
} scd30_uart_handle_t;

typedef struct {
    uint8_t* rx_buffer;
    uint16_t rx_cap;
    uint32_t start_ms;
    volatile scd30_uart_state_t state;
    scd30_uart_read_cb_t read_cb;
} scd30_read_properties_t;

typedef enum {
    FC_READ_HOLDING         = 0x03,
    FC_READ_INPUT           = 0x04,
    FC_WRITE_SINGLE_HOLDING = 0x06,
} modbus_function_code_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t hi; // MSB
    uint8_t lo; // LSB
} high_low_t;

typedef struct {
    uint8_t lo; // LSB
    uint8_t hi; // MSB
} low_high_t;

typedef struct {
    uint8_t slave_addr;    // Target device address (SCD30 is default 0x61) - SCD30_MODBUS_ID
    uint8_t function_code; // Modbus FC (0x03, 0x04, or 0x06) - modbus_function_code_t
    high_low_t reg_command;// 0x00nn
    high_low_t data;       // Value to write or number of regs to read
    low_high_t crc;        // Checksum validation
} modbus_tx_frame_t;

#define SCD30_MEASUREMENT_REGS 6U             /* READ_MEASUREMENT_BLOCK: CO2, temperature, RH as 3 floats of 2 registers */
#define SCD30_SINGLE_REG       1U             /* every other readable register */
#define SCD30_RX_MAX (5U + 2U * SCD30_MEASUREMENT_REGS) /* largest reply: measurement read */
typedef union {
    uint8_t raw[SCD30_RX_MAX];                /* the buffer DMA fills */
    struct __attribute__((packed)) {          /* exception reply, 5 bytes */
        uint8_t     addr;
        uint8_t     function;                 /* bit 7 set */
        uint8_t     code;
        low_high_t  crc;
    } exception_t;
    struct __attribute__((packed)) {          /* write echo, 8 bytes */
        uint8_t     addr;
        uint8_t     function;                 /* 0x06 */
        high_low_t  reg;
        high_low_t  value;
        low_high_t  crc;
    } accepted_t;
    struct __attribute__((packed)) {          /* read reply: header, then data, then CRC */
        uint8_t     addr;
        uint8_t     function;                 /* 0x03 */
        uint8_t     byte_count;               /* N */
        uint8_t     data[SCD30_RX_MAX - 3U];  /* N data bytes; the CRC follows them */
    } data_t;
} scd30_uart_response_t;

#pragma pack(pop)

scd30_uart_error_t scd30_uart_init(const scd30_uart_handle_t* handle);
scd30_uart_error_t scd30_uart_write(const scd30_uart_handle_t* handle, scd30_register_t command, uint16_t data, scd30_uart_response_t *response);
scd30_uart_error_t scd30_uart_read(const scd30_uart_handle_t* handle, scd30_register_t command, scd30_uart_response_t *response);
// DMA start
scd30_uart_error_t scd30_uart_start(const scd30_uart_handle_t* handle, scd30_read_properties_t* props);
scd30_uart_error_t scd30_uart_stop(const scd30_uart_handle_t* handle, scd30_read_properties_t* props);

void scd30_uart_idle_irq(const scd30_uart_handle_t* handle, scd30_read_properties_t* props);
#endif
