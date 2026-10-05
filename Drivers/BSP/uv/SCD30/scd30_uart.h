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
    UART_ERROR_NOT_IMPLEMENTED    = -5,
    UART_ERROR_BUSY               = -6, /**< A request is still in flight */
    UART_ERROR_RX_HW              = -7, /**< USART overrun / framing / noise during the reply */
} scd30_uart_error_t;

#define SCD30_UART_TIMEOUT_MS 100U

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
    uint8_t *rx_buffer;
    uint16_t rx_cap;
    uint32_t start_ms;
    volatile scd30_uart_state_t state;
    scd30_uart_read_cb_t read_cb;
} scd30_read_properties_t;

scd30_uart_error_t scd30_uart_init(const scd30_uart_handle_t* handle);
scd30_uart_error_t scd30_uart_write(const scd30_uart_handle_t* handle, scd30_read_properties_t* props,
                                    const uint8_t* tx, uint16_t tx_len);
void               scd30_uart_idle_irq(const scd30_uart_handle_t* handle, scd30_read_properties_t* props);
#endif
