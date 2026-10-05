#include <stddef.h>

#include "scd30_driver.h"

void* scd30_irq_ctx[2]; /* [0] = scd30_uart_handle_t*, [1] = scd30_read_properties_t*; read by stm32g4xx_it.c */
static scd30_device_t *s_scd30_device;
scd30_read_cb_t s_scd30_process;
static inline void scd30_driver_process(scd30_uart_error_t err, uint16_t len) {
    if ( err == UART_OK ) {
        s_scd30_process(s_scd30_device->read_props.rx_buffer,len);
    }
}

scd30_status_t scd30_init(scd30_device_t *device, const scd30_uart_handle_t *hbus, scd30_read_cb_t read_cb) {
    if ( device == NULL || hbus == NULL || read_cb == NULL) {
        return MODBUS_ERROR_PARAMETER;
    }
    s_scd30_process = read_cb;
    device->is_initialized = 0;
    device->hbus = hbus;
    device->read_props.read_cb = scd30_driver_process;
    s_scd30_device = device;
    scd30_irq_ctx[0] = (void*)hbus;
    scd30_irq_ctx[1] = &device->read_props;
    device->is_initialized = 1;
    return MODBUS_OK;

}

