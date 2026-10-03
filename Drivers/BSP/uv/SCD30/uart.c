#include <stddef.h>

#include "scd30_uart.h"

scd30_uart_error_t scd30_uart_init(const scd30_uart_handle_t* handle) {
    if (handle == NULL || handle->bus == NULL ||
        handle->dma_tx == NULL || handle->dma_rx == NULL) {
        return UART_ERROR_FRAME_MALFORMED;
    }
    return UART_ERROR_NOT_IMPLEMENTED;
}
