#include <stddef.h>

#include "scd30_uart.h"

#define DMA_CHANNEL_MAX 8U /* STM32G474: 8 channels per DMA controller */

// ch is the 1-based channel number: channel n owns IFCR bits [4(n-1) .. 4(n-1)+3] (GIF/TCIF/HTIF/TEIF).
static inline uint32_t dma_flag_mask(uint8_t ch) {
    return 0xFU << ((ch - 1U) * 4U);
}

scd30_uart_error_t scd30_uart_init(const scd30_uart_handle_t* handle) {
    if (handle == NULL || handle->bus == NULL || handle->dma_ctrl == NULL ||
        handle->dma_tx == NULL || handle->dma_rx == NULL) {
        return UART_ERROR_FRAME_MALFORMED;
    }
    if (handle->tx_ch < 1U || handle->tx_ch > DMA_CHANNEL_MAX ||
        handle->rx_ch < 1U || handle->rx_ch > DMA_CHANNEL_MAX) {
        return UART_ERROR_FRAME_MALFORMED;
    }

    // 1. start modify by diable first - Channels must be disabled before CPAR/CMAR/CNDTR can be written.
    handle->dma_tx->CCR &= ~DMA_CCR_EN;
    handle->dma_rx->CCR &= ~DMA_CCR_EN;

    // 2. Peripheral side is fixed; the buffer side (CMAR/CNDTR) is set per request in send().
    // set peripheral address register
    handle->dma_tx->CPAR = (uint32_t)&(handle->bus->TDR);
    handle->dma_rx->CPAR = (uint32_t)&(handle->bus->RDR);

    // 3. Let the USART raise DMA requests (CubeMX LL init does not set these).
    handle->bus->CR3 |= USART_CR3_DMAT | USART_CR3_DMAR;
    /*
    * Why CubeMX leaves this to you
    * 1. Zero Hardware Overhead: LL expects you to control exactly when a hardware peripheral requests data. If CubeMX set DMAT/DMAR during initialization, the UART would start generating DMA requests immediately before you've even configured or enabled your DMA streams.
    * 2. Explicit Transfer Control: In an LL project, you are responsible for starting and stopping the transfers. Enabling these bits is your explicit signal to start the hardware handshake.
     */

    // 4. cleanup before continue - Clear stale flags: USART (IDLE, overrun, framing, noise) and both DMA channels.
    handle->bus->ICR = USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    // write 1 to clear
    handle->dma_ctrl->IFCR = dma_flag_mask(handle->tx_ch) | dma_flag_mask(handle->rx_ch);

    // 5. IDLE interrupt = end of reply. The only interrupt source; no DMA channel interrupts.
    handle->bus->CR1 |= USART_CR1_IDLEIE;

    return UART_OK;
}
