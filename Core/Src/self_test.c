#include <string.h>

#include "self_test.h"

#include "bme688_driver.h"
#include "bme68x.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_tim.h"

#define W25Q_TEST_ADDRESS 0x00000000U
#define W25Q_TEST_LENGTH  256U


/**
 * @brief Sector erase -> page program -> read-back self-test for the W25Q128 driver.
 *        Step into this with the debugger and watch the return value / g_w25q_last_error.
 */
w25q_status_t w25q_self_test(w25q_device_t* device) {
    uint8_t write_buf[W25Q_TEST_LENGTH];
    uint8_t read_buf[W25Q_TEST_LENGTH];
    w25q_status_t rc;

    for (uint32_t i = 0; i < W25Q_TEST_LENGTH; i++) {
        write_buf[i] = (uint8_t)i;
    }

    rc = w25q_erase(device, W25Q_TEST_ADDRESS, W25Q_ERASE_SECTOR);
    if (rc != W25Q_OK) {
        return rc;
    }

    rc = w25q_write(device, W25Q_TEST_ADDRESS, write_buf, W25Q_TEST_LENGTH);
    if (rc != W25Q_OK) {
        return rc;
    }

    rc = w25q_read(device, W25Q_TEST_ADDRESS, read_buf, W25Q_TEST_LENGTH);
    if (rc != W25Q_OK) {
        return rc;
    }

    if (memcmp(write_buf, read_buf, W25Q_TEST_LENGTH) != 0) {
        return W25Q_ERROR_ID_MISMATCH;
    }

    return W25Q_OK;
}

/**
 * @brief bme688_delay_us() drives a 16-bit ARR, so a single call tops out at
 *        ~65535 us; chunk longer waits into slices it can actually perform.
 */
static void bme688_test_delay_us(uint32_t total_us, void* intf_ptr) {
    const uint32_t chunk_us = 50000U;
    while (total_us > chunk_us) {
        bme688_delay_us(chunk_us, intf_ptr);
        total_us -= chunk_us;
    }
    if (total_us > 0U) {
        bme688_delay_us(total_us, intf_ptr);
    }
}

/**
 * @brief Forced-mode measurement self-test for the BME688 driver: configures
 *        oversampling + the gas heater, triggers one forced-mode conversion,
 *        and sanity-checks the returned reading is within a plausible
 *        physical range. Step into this with the debugger and watch the
 *        return value / g_bme688_last_error.
 */
bme688_status_t bme688_self_test(bme688_device_t* device) {
    struct bme68x_conf conf = {0};
    struct bme68x_heatr_conf heatr_conf = {0};
    struct bme68x_data data = {0};
    uint8_t n_data = 0;

    conf.os_hum = BME68X_OS_2X;
    conf.os_temp = BME68X_OS_8X;
    conf.os_pres = BME68X_OS_4X;
    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE;
    if (bme68x_set_conf(&conf, &device->api) != BME68X_OK) {
        return BME688_ERROR_DEVICE_INIT;
    }

    heatr_conf.enable = BME68X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    if (bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &device->api) != BME68X_OK) {
        return BME688_ERROR_DEVICE_INIT;
    }

    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &device->api) != BME68X_OK) {
        return BME688_ERROR_DEVICE_INIT;
    }

    uint32_t meas_dur_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &device->api);
    meas_dur_us += (uint32_t)heatr_conf.heatr_dur * 1000U;
    bme688_test_delay_us(meas_dur_us, device->api.intf_ptr);

    if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_data, &device->api) != BME68X_OK || n_data == 0U) {
        return BME688_ERROR_SELF_TEST;
    }

    // Fixed-point bme68x_data layout: temperature is degC x100, humidity is %RH x1000.
    if (data.temperature < -4000 || data.temperature > 8500 ||
        data.pressure < 30000U || data.pressure > 110000U ||
        data.humidity > 100000U) {
        return BME688_ERROR_SELF_TEST;
    }

    return BME688_OK;
}

mq7_status_t mq7_self_test(mq7_device_t* device) {
    uint16_t v;
    for (int i = 0; i < 2; i++) {
        if (mq7_adc_sample(device->hbus, &v) != ADC_OK || v == 0U || v >= 4095U) {
            return MQ7_ERROR_DEVICE_FAIL; // stuck at a rail
        }
    }
    mq7_status_t rc = mq7_run(device);
    if (rc != MQ7_OK) {
        return rc;
    }
    uint8_t run_ok = LL_GPIO_IsOutputPinSet(device->ht_port, device->ht_pin) &&
        LL_TIM_IsEnabledCounter(device->timer) && LL_TIM_IsEnabledIT_UPDATE(device->timer);
    rc = mq7_stop(device);
    if (rc != MQ7_OK) {
        return rc;
    }
    uint8_t stop_ok = !LL_GPIO_IsOutputPinSet(device->ht_port, device->ht_pin) &&
        !LL_TIM_IsEnabledCounter(device->timer) && !LL_TIM_IsEnabledIT_UPDATE(device->timer);
    return (run_ok && stop_ok) ? MQ7_OK : MQ7_ERROR_DEVICE_FAIL;
}
