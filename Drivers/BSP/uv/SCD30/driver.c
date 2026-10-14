#include <stddef.h>

#include "scd30_driver.h"

void* scd30_irq_ctx[2]; /* [0] = scd30_uart_handle_t*, [1] = scd30_read_properties_t*; read by stm32g4xx_it.c */
static scd30_device_t* s_scd30_device;
static scd30_read_cb_t s_scd30_process;

static inline void timer_arm_(TIM_TypeDef* tim, uint32_t ms) { // one-pulse: fires once, then stops by itself
    tim->ARR = ms * 10U - 1U; // 10 kHz: 10 counts per ms
    tim->CNT = 0;
    tim->CR1 |= TIM_CR1_CEN;
}

static inline void scd30_driver_process(scd30_uart_error_t err, uint16_t len) {
    timer_arm_(s_scd30_device->timer, s_scd30_device->cycle_ms); // reply came (good or bad): next cycle
    if (err == UART_OK) {
        s_scd30_process(s_scd30_device->read_props.rx_buffer, len);
    }
}

scd30_status_t scd30_init(scd30_device_t* device, const scd30_uart_handle_t* hbus, scd30_read_cb_t read_cb) {
    if (device == NULL || hbus == NULL || read_cb == NULL) {
        return SCD30_ERROR_PARAMETER;
    }

    device->is_initialized = 0;
    device->hbus = hbus;
    device->read_props.read_cb = scd30_driver_process;
    s_scd30_device = device;
    s_scd30_process = read_cb;
    scd30_irq_ctx[0] = (void*)hbus;
    scd30_irq_ctx[1] = &device->read_props;
    uint8_t version[2] = {0};
    if ( scd30_get_firmware_version(device, &version[0], &version[1]) != MODBUS_OK ) {
        return SCD30_ERROR_DEVICE_FAIL;
    }
    device->is_initialized = 1;
    return MODBUS_OK;
}

scd30_status_t scd30_get_firmware_version(scd30_device_t* device, uint8_t* major, uint8_t* minor) {
    if (device == NULL || device->hbus == NULL || major == NULL || minor == NULL) {
        return SCD30_ERROR_PARAMETER;
    }

    scd30_uart_response_t response;
    scd30_uart_error_t err = scd30_uart_read(device->hbus, SCD30_REG_FIRMWARE_VERSION, &response);
    if (err == UART_ERROR_EXCEPTION_RETURNED) {
        return (scd30_status_t)response.exception_t.code; // 1..4 matches the SCD30_ERROR_* values
    }
    if (err != UART_OK) {
        return SCD30_ERROR_DEVICE_FAIL; // timeout, CRC, malformed
    }

    *major = response.data_t.data[0];
    *minor = response.data_t.data[1];
    return MODBUS_OK;
}

scd30_status_t scd30_run(scd30_device_t* device) {
    if (device == NULL || !device->is_initialized || device->timer == NULL ||
        device->cycle_ms == 0 || device->cycle_ms > SCD30_CYCLE_MAX_MS) {
        return SCD30_ERROR_PARAMETER;
    }
    scd30_uart_response_t response;
    if (scd30_uart_write(device->hbus, SCD30_REG_START_MEASUREMENT, 0U, &response) != UART_OK) { // 0 = no pressure compensation
        return SCD30_ERROR_DEVICE_FAIL;
    }
    device->timer->SR = ~TIM_SR_UIF; // write 0 to clear
    device->timer->DIER |= TIM_DIER_UIE;
    timer_arm_(device->timer, device->cycle_ms);
    return MODBUS_OK;
}

void scd30_stop(scd30_device_t* device) {
    device->timer->DIER &= ~TIM_DIER_UIE; // first: no more cycles from the timer
    device->timer->CR1 &= ~TIM_CR1_CEN;
    device->timer->SR = ~TIM_SR_UIF;      // a pending update must not start one more cycle
    scd30_uart_stop(device->hbus, &device->read_props);
    scd30_uart_response_t response;
    scd30_uart_write(device->hbus, SCD30_REG_STOP_MEASUREMENT, 1U, &response); // after the DMA request is aborted
}

// TIM7 update: one cycle
void scd30_timer_irq(void) {
    scd30_device_t* device = s_scd30_device;
    if (device->read_props.state != SCD30_UART_READY) { // last reply never came
        scd30_uart_stop(device->hbus, &device->read_props);
    }
    scd30_uart_start(device->hbus, &device->read_props);
    timer_arm_(device->timer, device->cycle_ms); // no reply: this arm runs the next cycle
}
