#ifndef SCD30_UART_H
#define SCD30_UART_H
#include <stdint.h>
#include "stm32g474xx.h"

typedef struct {
    USART_TypeDef* bus; // bus type
    DMA_Channel_TypeDef* dma_tx;
    DMA_Channel_TypeDef* dma_rx;
} scd30_uart_handle_t;

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
} scd30_uart_error_t;

scd30_uart_error_t scd30_uart_init(const scd30_uart_handle_t* handle);
#endif
