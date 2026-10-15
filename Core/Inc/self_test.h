#ifndef SELF_TEST_H
#define SELF_TEST_H
#include <stdint.h>
#include "w25q_driver.h"
#include "bme688_driver.h"
#include "scd30_driver.h"
#include "mq7_driver.h"

/** Sector erase -> page program -> read-back. */
w25q_status_t w25q_self_test(w25q_device_t* device);
/** One forced-mode measurement, checked against plausible physical ranges. */
bme688_status_t bme688_self_test(bme688_device_t* device);
/** Length of the last SCD30 reply, set by the read callback (interrupt context), cleared by the main loop after the check. */
extern volatile uint16_t g_scd30_rx_len;
/** Checks one DMA measurement reply (length, CO2/temp/RH ranges). The driver layer already checked the CRC. */
scd30_status_t scd30_self_test(const uint8_t* rx, uint16_t len);
/** ADC sanity + heater/timer control. Starts and stops the cycle itself: run before the real mq7_run. */
mq7_status_t mq7_self_test(mq7_device_t* device);

#endif
