#include "scd30_driver.h"
scd30_status_t scd30_init(scd30_device_t *device, const scd30_uart_handle_t *hbus) {
    device->hbus = hbus;
    device->is_initialized = 0;
    return MODBUS_ERROR_NOT_IMPLEMENTED;

}