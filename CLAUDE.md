# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

MetallicBear (formerly "EnviLogger") is a bare-metal (no RTOS) firmware project for the NUCLEO-G474RE board (STM32G474RET6, Cortex-M4 @ 170 MHz). It's a learning project: an automotive cabin air-quality monitor that reads CO (MQ7), CO2/RH/temp (SCD30), IAQ gas (BME688), logs to external SPI NOR flash (W25Q128), and streams debug output over a Virtual COM Port. Design doc: the project `README.md` (system architecture, hardware module interface mapping, driver learning roadmap). There is no separate `doc/` directory in this repo.

Drivers are built in a deliberate sequence, from simplest bus to most complex:
1. **W25Q128 flash (SPI3)** — in progress (see `Drivers/BSP/uv/W25Qxx/`)
2. **BME688 (I2C2)** — in progress (see `Drivers/BSP/uv/BME688/`)
3. **SCD30 (USART3 + DMA, Modbus RTU/CRC16)** — in progress (see `Drivers/BSP/uv/SCD30/`)
4. **MQ7 (ADC1 + TIM2 phase timer + GPIO heater switch)** — in progress (headers declared and functions stubbed in `Drivers/BSP/uv/MQ7/`; the `.ioc` has PA0 as `ADC1_IN1`, PC0 as GPIO output `MQ7_HEATER`, and `TIM2_IRQn` and `ADC1_2_IRQn` enabled in NVIC — see the MQ7 notes below)

Hardware pin map:
| Peripheral | Pins | Purpose |
|---|---|---|
| SPI3 | PC10 CLK, PC11 MISO, PC12 MOSI, PB0 CS (soft NSS, GPIO output labeled `W25Q_CS`) | W25Q128 flash |
| I2C2 | PC4 SCL, PA8 SDA | BME688 |
| USART3 | PB10 TX, PB11 RX (+ TX DMA on DMA1_Channel1, RX DMA on DMA1_Channel2, both normal mode, USART IDLE interrupt only, no DMA channel interrupts) | SCD30 Modbus, 19200 baud |
| TIM7 | none (internal) | SCD30 cycle timer: one-pulse, 10 kHz count (prescaler 16999), 16-bit ARR so max 6553 ms; shares `TIM7_DAC_IRQn` with DAC, so the handler checks `UIF` |
| USART2 | PA2 TX, PA3 RX | VCP debug console via ST-LINK, 115200 baud, `printf` redirected here |
| ADC1 | PA0 (IN1) | MQ7 analog read |
| GPIO | PC0 (output, label `MQ7_HEATER`) | MQ7 heater switch: high = 5.0 V phase, low = 1.4 V phase |
| TIM2 | none (internal) | MQ7 heat-cycle phase timer: one-pulse, 10 kHz count (prescaler 16999), 32-bit ARR so a 60 s or 90 s phase fits in one period |
| EXTI13 | PC13 | User button B1, falling edge |

Note on `W25Q_CS`: it lives on PB0 (Morpho connector) rather than on SPI3's hardware NSS pin, since the driver drives chip-select manually as a plain GPIO output. The flash link was originally wired to SPI1 (PA5 CLK, PA6 MISO, PA7 MOSI) but was moved to SPI3 because PA5 conflicts with the NUCLEO-G474RE's onboard LED (LD2), which is hardwired to that pin. This board has no HSE crystal fitted (NUCLEO-64 boards ship with the X3 footprint unpopulated by default), so `SystemClock_Config()` derives the 170 MHz system clock from HSI (16 MHz) through the PLL, not HSE.

Note on the MQ7 heater pin: `PC0` is a plain GPIO output (label `MQ7_HEATER`), not a PWM pin. The heater switch on the MQ7 board only needs two levels: high selects the 5.0 V heating phase and low selects the 1.4 V cooling phase, so the driver sets and clears the pin with `LL_GPIO_SetOutputPin`/`LL_GPIO_ResetOutputPin` (through `ht_port`/`ht_pin` in `mq7_adc_handle_t`) and no PWM duty setpoints are used. `TIM1` is not used for the MQ7 and is not enabled in the `.ioc`. CubeMX generates PC0 low at boot, which is the 1.4 V state if the switch is not inverting; check this against `hardware/mq7_heater_switch`.

Note on the MQ7 heat-cycle/sample design: `TIM2` is a 32-bit one-pulse timer (`TIM2.Prescaler=16999`, 10 kHz count) and `NVIC.TIM2_IRQn` is enabled. The cycle is heat (about 60 s, pin high) → cool (about 90 s, pin low) → sample at the end of the cool phase → heat again, with the phase lengths taken from the MQ7 datasheet. `mq7_adc_start` sets the pin high, arms TIM2 for the heating phase and returns at once. In the TIM2 interrupt, the end of the heating phase sets the pin low and re-arms TIM2 for the cooling phase; the end of the cooling phase starts one ADC1 conversion (`LL_ADC_REG_StartConversion`), sets the pin high and re-arms TIM2 for the heating phase. Because TIM2 stops itself after each period (one-pulse), every interrupt re-arms it by writing ARR and setting CEN, so the cycle repeats until `mq7_adc_stop` disables the update interrupt and the counter, aborts any conversion in flight and sets the pin low. `NVIC.ADC1_2_IRQn` is enabled so `ADC1_2_IRQHandler` fires on end-of-conversion to deliver the sample. Two things CubeMX's `.ioc` does *not* configure, since they are register writes rather than `.ioc`-level parameters: ADC1 calibration and enable (`LL_ADC_StartCalibration`, `LL_ADC_Enable`) plus the EOC interrupt enable bit (`LL_ADC_EnableIT_EOC(ADC1)`), all needed once at init, and the `LL_ADC_REG_StartConversion` call itself. Given this is a bare-metal, no-RTOS superloop, keep the ADC interrupt path minimal (read the raw 12-bit value and hand it up through the `read_cb` callback, which runs in interrupt context, so no `printf`) and do any heavier delivery (printf, flash logging) from `main()`'s loop.

On breadboard wiring, SPI3's baud rate prescaler is set to `/64` (~2.66 MHz) rather than the CubeMX-computed max of `/4` (42.5 MHz) — full speed causes signal-integrity failures (garbled/no communication) over jumper wires; verified with a logic analyzer. Revisit this once the driver moves to a proper PCB.

Planned safety features (not yet implemented): VREFINT brownout check before flash erase/write, IWDG watchdog fed within a 2000 ms loop budget.

## Build system

CMake + Ninja, cross-compiled for `arm-none-eabi` (Cortex-M4, hard float, fpv4-sp-d16). This is STM32CubeMX-generated CMake structure — do not hand-edit the generated blocks in `cmake/stm32cubemx/CMakeLists.txt` beyond what CubeMX would produce; add project code via the top-level `CMakeLists.txt`'s "Add user..." sections or a dedicated subdirectory (see `cmake/w25qxx/` for the pattern a new driver library should follow).

Configure and build with presets (from `CMakePresets.json`):
```sh
cmake --preset Debug        # or Release
cmake --build --preset Debug
```
Output ELF/map land in `build/Debug/` (e.g. `build/Debug/metallic-bear.elf`, `metallic-bear.map`).

There are two toolchain files under `cmake/`:
- `gcc-arm-none-eabi.cmake` — GNU arm-none-eabi-gcc/g++ (used by the `CMakePresets.json` presets; requires `arm-none-eabi-*` on `PATH`)
- `starm-clang.cmake` — alternate Clang-based toolchain (`starm-clang`/`starm-clang++`), not wired into the presets by default

No flashing/debugging scripts are checked in; use STM32CubeIDE, OpenOCD, or ST-LINK utilities externally against the built `.elf`.

There is no test suite in this repo (bare-metal firmware, HIL/manual test protocol only — manual bring-up test steps per peripheral, e.g. JEDEC ID read, sector erase/program/read loop, I2C WhoAmI, Modbus frame check, PWM/ADC timing).

## Code architecture

- `Core/Src`, `Core/Inc` — STM32CubeMX-generated startup code and **LL-only** peripheral init, all of it inlined into a single `main.c`/`main.h` (plus `stm32g4xx_it.c`, `system_stm32g4xx.c`, `syscalls.c`, `sysmem.c`) — there is no per-peripheral split (no `gpio.c`/`spi.c`/`adc.c`/`tim.c`/`usart.c`) and no HAL MSP/conf files, because CubeMX's Project Manager is configured to generate LL drivers with everything under `main.c` rather than "one pair of files per peripheral." If a regeneration ever produces `stm32g4xx_hal_conf.h`, `stm32g4xx_hal_msp.c`, or `HAL_*`/`*_HandleTypeDef` calls in `main.c`, that means CubeMX's per-peripheral driver selection reverted to HAL for the newly-touched peripheral (this has happened when adding a new peripheral, e.g. TIM6) — go back into the `.ioc` Project Manager settings and set that peripheral's generated driver back to LL, then regenerate; don't hand-patch HAL calls into an otherwise LL codebase. Treat CubeMX-managed regions (`/* USER CODE BEGIN ... END */` blocks) as the only place to hand-edit generated files, since regenerating from `metallicbear.ioc` will overwrite everything else. Note: `main.c` has CRLF line endings — when a plain-text edit tool fails to match a line that greps fine, it's almost always this; either match the `\r` explicitly or use `sed`. A USER CODE block edited with mismatched line endings (LF inserted into an otherwise-CRLF file) can make CubeMX's regeneration merge silently drop that block's contents instead of preserving them — keep any manual edits inside USER CODE blocks CRLF-consistent with the rest of the file.
- `Drivers/STM32G4xx_HAL_Driver` — vendored ST HAL/LL sources. The SPI HAL/LL files here are currently untracked (added manually to support SPI3 before a CubeMX regeneration picked them up) — check `git status` before assuming the HAL driver set matches what CubeMX would generate.
- `Drivers/CMSIS` — vendored ARM CMSIS + ST device headers.
- `Drivers/BME68x` — vendored Bosch official BME68x sensor API (third-party, not hand-written), built as its own `bme68x_Driver` object library and pulled in via `ADD_SUBDIRECTORY(Drivers/BME68x)` in the top-level `CMakeLists.txt`.
- `Drivers/BSP/uv/W25Qxx` (`w25q_driver.c`/`.h`, `w25q_spi.c`/`.h`) — the custom, hand-written W25Q128 flash driver, split into a protocol layer (`w25q_driver`) and a low-level SPI transport layer (`w25q_spi`); this is the template for how future custom sensor drivers (SCD30, MQ7) should be structured. It has its own `CMakeLists.txt` (an `OBJECT` library named `w25qxx_Driver`, linked against `stm32cubemx`) and is pulled into the build via `ADD_SUBDIRECTORY(Drivers/BSP/uv/W25Qxx)` and `target_link_libraries(... w25qxx_Driver)` in the top-level `CMakeLists.txt` — there is no `cmake/w25qxx/` directory or `scope-share.cmake`/`STM_SET_LIBNAME()` macro in this repo; follow the simpler pattern actually used in `Drivers/BSP/uv/W25Qxx/CMakeLists.txt` for new drivers instead.
- `Drivers/BSP/uv/BME688` (`bme688_driver.c`/`.h`, `bme688_i2c.c`/`.h`) — the custom BME688 driver, following the same protocol-layer/transport-layer split as W25Qxx: `bme688_i2c` is the low-level I2C transport (plus `bme688_delay_us()`, the TIM6-backed microsecond delay callback the Bosch API requires), and `bme688_driver` wraps the vendored `bme68x_Driver` (`Drivers/BME68x`) with this project's own init/self-test logic. Its `CMakeLists.txt` (`bme688_Driver` object library) links against both `bme68x_Driver` and `stm32cubemx`, and is pulled in via `ADD_SUBDIRECTORY(Drivers/BSP/uv/BME688)` in the top-level `CMakeLists.txt`.
- `Drivers/BSP/uv/SCD30` (`uart.c`, `scd30_uart.h`, `driver.c`, `scd30_driver.h`) — the custom SCD30 driver, split into a UART layer and a driver layer that only calls down (the UART layer reports up through the `read_cb` function pointer, never by including the driver header). `uart.c` builds Modbus RTU frames and CRC itself (`fill_frame_`), and offers blocking `scd30_uart_write`/`scd30_uart_read` for setup and diagnostics, plus the DMA path: `scd30_uart_start` (sends the measurement-read frame by DMA, returns at once), `scd30_uart_stop` (aborts a request in flight, no callback) and `scd30_uart_idle_irq` (USART3 IDLE interrupt, calls `read_cb` with the raw reply, CRC included, and is wired in `USART3_IRQHandler`). `driver.c` runs the cycle: `scd30_run` writes start-measurement then arms TIM7, `scd30_timer_irq` (called from `TIM7_DAC_IRQHandler`) starts one request per tick, the reply callback re-arms TIM7 for `cycle_ms`, and `scd30_stop` cuts the cycle, aborts the request and writes stop-measurement. The caller fills `device->timer` and `device->cycle_ms` before `scd30_init`, and writes the sensor measurement interval (`cycle_ms / 1000` seconds) with `scd30_uart_write` before `scd30_run`; `cycle_ms` must stay longer than that interval. The user callback runs in interrupt context, so keep it short (no `printf`). Its `CMakeLists.txt` builds the `scd30_Driver` object library (linked against `stm32cubemx`), pulled in via `ADD_SUBDIRECTORY(Drivers/BSP/uv/SCD30)`.
- `Drivers/BSP/uv/MQ7` (`adc.c`, `mq7_adc.h`, `driver.c`, `mq7_driver.h`) — the custom MQ7 driver, declared in the same two-layer split as SCD30 (an ADC layer that only calls down, and a driver layer on top), currently with stubbed functions. `mq7_adc.h` declares `mq7_adc_handle_t` (`bus` = ADC1, `timer` = TIM2, `ht_port`/`ht_pin` = the `MQ7_HEATER` pin), `mq7_adc_error_t`, and `mq7_adc_init`/`mq7_adc_start`/`mq7_adc_stop`/`mq7_adc_idle_irq`; `adc.c` also has a blocking `mq7_adc_sample` used by `mq7_init` as a start-up check. `mq7_driver.h` declares `mq7_device_t`, `mq7_status_t`, and `mq7_init`/`mq7_run`/`mq7_stop`/`mq7_timer_irq`, where `mq7_init` takes a `read_cb` the driver calls with each sample. `mq7_run`/`mq7_stop` are meant to start and stop the heat/cool/sample cycle described in the MQ7 note above. Its `CMakeLists.txt` builds the `mq7_Driver` object library (linked against `stm32cubemx`), pulled in via `ADD_SUBDIRECTORY(Drivers/BSP/uv/MQ7)`.
- `metallicbear.ioc` — the STM32CubeMX project file; peripheral/pin changes should generally be made here and regenerated, not by hand-editing generated `Core/` files outside `USER CODE` blocks.
- `STM32G474xx_FLASH.ld`, `startup_stm32g474xx.s` — linker script and startup assembly, CubeMX/CMSIS-generated.
