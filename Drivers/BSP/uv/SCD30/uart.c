#include <stddef.h>

#include "main.h" /* g_ms_ticks */
#include "scd30_uart.h"

typedef enum {
    SCD30_REPLY_OK           = 0x00, /* valid reply, not an exception */
    SCD30_REPLY_EXC_FUNCTION = 0x01, /* exception: function code not supported */
    SCD30_REPLY_EXC_ADDRESS  = 0x02, /* exception: register address does not exist */
    SCD30_REPLY_EXC_VALUE    = 0x03, /* exception: value out of range */
    SCD30_REPLY_EXC_DEVICE   = 0x04, /* exception: sensor internal error */
    SCD30_REPLY_BAD_CRC      = 0xFE, /* complete frame, but the CRC does not match */
    SCD30_REPLY_TIMEOUT      = 0xFF, /* nothing, or not enough bytes, arrived in time */
} scd30_reply_status_t;

#define DMA_CHANNEL_MAX 8U /* STM32G474: 8 channels per DMA controller */
#define MODBUS_EXCEPTION_FLAG 0x80U

// ch is the 1-based channel number: channel n owns IFCR bits [4(n-1) .. 4(n-1)+3] (GIF/TCIF/HTIF/TEIF).
static inline uint32_t dma_flag_mask(uint8_t ch) {
    return 0xFU << ((ch - 1U) * 4U);
}

// Modbus RTU CRC16: init 0xFFFF, polynomial 0xA001. The result goes to the frame's crc field, low byte first.
static void modbus_crc16_(const uint8_t* buf, uint16_t len, low_high_t* crc) {
    uint16_t value = 0xFFFFU;
    for (uint16_t ii = 0; ii < len; ii++) {
        value ^= buf[ii];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            value = (value & 1U) ? (uint16_t)((value >> 1) ^ 0xA001U) : (uint16_t)(value >> 1);
        }
    }
    crc->lo = (uint8_t)(value & 0xFFU);
    crc->hi = (uint8_t)(value >> 8);
}

static inline uint8_t is_crc_ok_(const uint8_t* frame, uint16_t len) {
    if (len < 3U) {
        return 0;
    }
    low_high_t crc;
    modbus_crc16_(frame, (uint16_t)(len - 2U), &crc);
    return (crc.lo == frame[len - 2U]) && (crc.hi == frame[len - 1U]);
}

// Fills a request frame: address, function code, register, data, then the CRC over the first 6 bytes.
static void fill_frame_(modbus_tx_frame_t* frame, modbus_function_code_t fc, scd30_register_t command, uint16_t data) {
    frame->slave_addr = SCD30_MODBUS_ID;
    frame->function_code = fc;
    frame->reg_command.hi = (uint8_t)(command >> 8);
    frame->reg_command.lo = (uint8_t)(command & 0xFFU);
    frame->data.hi = (uint8_t)(data >> 8);
    frame->data.lo = (uint8_t)(data & 0xFFU);
    modbus_crc16_((const uint8_t*)frame, 6U, &frame->crc);
}

// Sends len bytes, polled, and waits until the last one is fully out. Blocking, with timeout.
// Returns UART_OK, or UART_ERROR_TIMEOUT if the USART did not accept the bytes in time.
static inline scd30_uart_error_t send_frame_(const scd30_uart_handle_t* handle, const modbus_tx_frame_t* frame) {
    uint8_t* tx = (uint8_t*)frame;
    uint8_t len = sizeof(modbus_tx_frame_t); // will be remove when needed
    const uint32_t t0 = g_ms_ticks;
    for (uint8_t ii = 0; ii < len; ii++) {
        while (!(handle->bus->ISR & USART_ISR_TXE)) {
            if ((uint32_t)(g_ms_ticks - t0) > SCD30_UART_TIMEOUT_MS) {
                return UART_ERROR_TIMEOUT;
            }
        }
        handle->bus->TDR = tx[ii];
    }
    while (!(handle->bus->ISR & USART_ISR_TC)) { // last byte fully out
        if ((uint32_t)(g_ms_ticks - t0) > SCD30_UART_TIMEOUT_MS) {
            return UART_ERROR_TIMEOUT;
        }
    }
    return UART_OK;
}

static scd30_reply_status_t receive_and_validate_(const scd30_uart_handle_t* handle,
                                                 scd30_uart_response_t* response) {
    const uint32_t t0 = g_ms_ticks; // timeout counts from the start of the receive
    uint8_t expected = 3U;          // start with the header: address, function, third byte
    for (uint8_t ii = 0; ii < expected; ii++) {
        while (!(handle->bus->ISR & USART_ISR_RXNE)) {
            if ((uint32_t)(g_ms_ticks - t0) > SCD30_UART_TIMEOUT_MS) {
                return SCD30_REPLY_TIMEOUT;
            }
        }
        response->raw[ii] = (uint8_t)handle->bus->RDR;
        if (ii == 2U) {                                     // header complete: now the length is known
            if (response->raw[1] & MODBUS_EXCEPTION_FLAG) { // bit 7 of the function code: exception reply
                expected = 5U;                              // address, function, code, crc lo, crc hi
            } else if (response->raw[1] == FC_WRITE_SINGLE_HOLDING) {
                expected = 8U; // echo of the request
            } else {
                expected = (uint8_t)(5U + response->raw[2]); // header + data (byte count) + crc
            }
            if (expected > SCD30_RX_MAX) {
                return SCD30_REPLY_BAD_CRC; // length field is nonsense: damaged frame
            }
        }
    }
    if (!is_crc_ok_(response->raw, expected)) {
        return SCD30_REPLY_BAD_CRC;
    }
    if (response->raw[1] & MODBUS_EXCEPTION_FLAG) {
        return (scd30_reply_status_t)response->exception_t.code; // 1..4 = the sensor's exception code
    }
    return SCD30_REPLY_OK;
}

///////////////////////////////////////////////////////////////////////////////////////

scd30_uart_error_t scd30_uart_init(const scd30_uart_handle_t* handle) {
    if (handle == NULL || handle->bus == NULL || handle->dma_ctrl == NULL ||
        handle->dma_tx == NULL || handle->dma_rx == NULL) {
        return UART_ERROR_INVALID_PARAMS;
    }
    if (handle->tx_ch < 1U || handle->tx_ch > DMA_CHANNEL_MAX ||
        handle->rx_ch < 1U || handle->rx_ch > DMA_CHANNEL_MAX) {
        return UART_ERROR_INVALID_PARAMS;
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

scd30_uart_error_t scd30_uart_write(const scd30_uart_handle_t* handle, scd30_register_t command, uint16_t data,
                                    scd30_uart_response_t* response) {
    if (handle == NULL || response == NULL) {
        return UART_ERROR_INVALID_PARAMS;
    }
    modbus_tx_frame_t frame;
    fill_frame_(&frame, FC_WRITE_SINGLE_HOLDING, command, data);

    if (send_frame_(handle, &frame) != UART_OK) {
        return UART_ERROR_TIMEOUT;
    }
    scd30_reply_status_t status = receive_and_validate_(handle, response);
    if (status > SCD30_REPLY_OK) {
        if (status == SCD30_REPLY_TIMEOUT) {
            return UART_ERROR_TIMEOUT;
        }
        if (status == SCD30_REPLY_BAD_CRC) {
            return UART_ERROR_CRC_MISMATCH;
        }
        return UART_ERROR_EXCEPTION_RETURNED;
    }

    // a good write is echoed back unchanged
    // if (response->accepted_t.addr != frame.slave_addr ||
    //     response->accepted_t.function != FC_WRITE_SINGLE_HOLDING ||
    //     response->accepted_t.reg.hi != frame.reg_command.hi ||
    //     response->accepted_t.reg.lo != frame.reg_command.lo ||
    //     response->accepted_t.value.hi != frame.data.hi ||
    //     response->accepted_t.value.lo != frame.data.lo) {
    //     return UART_ERROR_FRAME_MALFORMED;
    // }
    return UART_OK;
}

scd30_uart_error_t scd30_uart_read(const scd30_uart_handle_t* handle, scd30_register_t command,
                                   scd30_uart_response_t* response) {
    if (handle == NULL || response == NULL) {
        return UART_ERROR_INVALID_PARAMS;
    }
    modbus_tx_frame_t frame;

    const uint16_t count = (command == SCD30_REG_READ_MEASUREMENT_BLOCK) ? SCD30_MEASUREMENT_REGS : SCD30_SINGLE_REG;
    frame.slave_addr = SCD30_MODBUS_ID;
    switch (command) {
        case SCD30_REG_DATA_READY_STATUS:
        case SCD30_REG_READ_MEASUREMENT_BLOCK:
            frame.function_code = FC_READ_INPUT;
            break;
        case SCD30_REG_MEASUREMENT_INTERVAL:
        case SCD30_REG_AUTOMATIC_SELF_CALIBRATION:
        case SCD30_REG_FORCED_RECALIBRATION:
        case SCD30_REG_TEMPERATURE_OFFSET:
        case SCD30_REG_ALTITUDE_COMPENSATION:
        case SCD30_REG_FIRMWARE_VERSION:
            frame.function_code = FC_READ_HOLDING;
            break;
        default: // write-only registers (start, stop, soft reset) or unknown
            return UART_ERROR_INVALID_PARAMS;
    }
    frame.reg_command.hi = (uint8_t)(command >> 8);
    frame.reg_command.lo = (uint8_t)(command & 0xFFU);
    frame.data.hi = (uint8_t)(count >> 8);
    frame.data.lo = (uint8_t)(count & 0xFFU);
    modbus_crc16_((const uint8_t*)&frame, 6U, &frame.crc);

    if (send_frame_(handle, &frame) != UART_OK) {
        return UART_ERROR_TIMEOUT;
    }
    scd30_reply_status_t status = receive_and_validate_(handle, response);
    if (status > SCD30_REPLY_OK) {
        if (status == SCD30_REPLY_TIMEOUT) {
            return UART_ERROR_TIMEOUT;
        }
        if (status == SCD30_REPLY_BAD_CRC) {
            return UART_ERROR_CRC_MISMATCH;
        }
        return UART_ERROR_EXCEPTION_RETURNED;
    }

    // a good read answers with the same function code and 2 bytes per register
    // if (response->data_t.addr != frame.slave_addr ||
    //     response->data_t.function != frame.function_code ||
    //     response->data_t.byte_count != 2U * count) {
    //     return UART_ERROR_FRAME_MALFORMED;
    // }
    return UART_OK;
}

static modbus_tx_frame_t s_dma_frame; // static: DMA reads it after start returns

// One measurement read by DMA, returns at once; the reply comes in read_cb.
scd30_uart_error_t scd30_uart_start(const scd30_uart_handle_t* handle, scd30_read_properties_t* rp) {
    if (handle == NULL || rp == NULL || rp->rx_buffer == NULL || rp->rx_cap == 0) {
        return UART_ERROR_INVALID_PARAMS;
    }
    if (rp->state != SCD30_UART_READY) {
        return UART_ERROR_BUSY;
    }
    fill_frame_(&s_dma_frame, FC_READ_INPUT, SCD30_REG_READ_MEASUREMENT_BLOCK, SCD30_MEASUREMENT_REGS); // data = register count

    rp->state = SCD30_UART_READING;
    rp->start_ms = g_ms_ticks;

    handle->dma_tx->CCR &= ~DMA_CCR_EN;
    handle->dma_rx->CCR &= ~DMA_CCR_EN;
    handle->bus->ICR = USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    handle->bus->RQR = USART_RQR_RXFRQ; // flush RDR: DMA would take a stale byte as reply byte 0
    handle->dma_ctrl->IFCR = dma_flag_mask(handle->tx_ch) | dma_flag_mask(handle->rx_ch);

    // RX first: the reply can come fast
    handle->dma_rx->CMAR = (uint32_t)rp->rx_buffer;
    handle->dma_rx->CNDTR = rp->rx_cap;
    handle->dma_rx->CCR |= DMA_CCR_EN;

    handle->dma_tx->CMAR = (uint32_t)&s_dma_frame;
    handle->dma_tx->CNDTR = sizeof(s_dma_frame);
    handle->dma_tx->CCR |= DMA_CCR_EN;
    return UART_OK;
}

// Aborts a request in flight. No callback. Safe to call when idle.
scd30_uart_error_t scd30_uart_stop(const scd30_uart_handle_t* handle, scd30_read_properties_t* rp) {
    if (handle == NULL || rp == NULL) {
        return UART_ERROR_INVALID_PARAMS;
    }
    rp->state = SCD30_UART_READY; // first: the IDLE ISR returns early once the state is not READING

    handle->dma_tx->CCR &= ~DMA_CCR_EN;
    handle->dma_rx->CCR &= ~DMA_CCR_EN;
    handle->bus->ICR = USART_ICR_IDLECF | USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    handle->bus->RQR = USART_RQR_RXFRQ; // drop a byte of the aborted reply
    handle->dma_ctrl->IFCR = dma_flag_mask(handle->tx_ch) | dma_flag_mask(handle->rx_ch);
    return UART_OK;
}

void scd30_uart_idle_irq(const scd30_uart_handle_t* h, scd30_read_properties_t* rp) {
    if (!(h->bus->ISR & USART_ISR_IDLE)) {
        return;
    }
    h->bus->ICR = USART_ICR_IDLECF;
    if (rp->state != SCD30_UART_READING) {
        return;
    }
    h->dma_rx->CCR &= ~DMA_CCR_EN; // freezes CNDTR
    uint16_t rx_len = rp->rx_cap - h->dma_rx->CNDTR;

    scd30_uart_error_t err =
        (h->bus->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) ? UART_ERROR_RX_HW : UART_OK;
    h->bus->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;

    rp->read_cb(err, rx_len);
    rp->state = SCD30_UART_READY; // after the callback: a write() inside it gets BUSY
}
