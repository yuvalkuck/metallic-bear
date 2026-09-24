# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

MetallicBear (formerly "EnviLogger") is a bare-metal (no RTOS) firmware project for the NUCLEO-G474RE board (STM32G474RET6, Cortex-M4 @ 170 MHz). It's a learning project: an automotive cabin air-quality monitor that reads CO (MQ7), CO2/RH/temp (SCD30), IAQ gas (BME688), logs to external SPI NOR flash (W25Q128), and streams debug output over a Virtual COM Port. Design doc: the project `README.md` (system architecture, hardware module interface mapping, driver learning roadmap). There is no separate `doc/` directory in this repo.

Drivers are built in a deliberate sequence, from simplest bus to most complex:
1. **W25Q128 flash (SPI3)** — in progress (see `Drivers/BSP/uv/W25Qxx/`)
2. **BME688 (I2C2)** — not started
3. **SCD30 (USART3 + DMA, Modbus RTU/CRC16)** — not started
4. **MQ7 (ADC1 + TIM1 PWM)** — not started

Hardware pin map:
| Peripheral | Pins | Purpose |
|---|---|---|
| SPI3 | PC10 CLK, PC11 MISO, PC12 MOSI, PB0 CS (soft NSS, GPIO output labeled `W25Q_CS`) | W25Q128 flash |
| I2C2 | PC4 SCL, PA8 SDA | BME688 |
| USART3 | PB10 TX, PB11 RX (+ RX DMA, circular) | SCD30 Modbus, 19200 baud |
| USART2 | PA2 TX, PA3 RX | VCP debug console via ST-LINK, 115200 baud, `printf` redirected here |
| ADC1 | PA0 (IN1) | MQ7 analog read |
| TIM1 | PA8 (CH1 PWM) | MQ7 heater drive |
| EXTI13 | PC13 | User button B1, falling edge |

Note on `W25Q_CS`: it lives on PB0 (Morpho connector) rather than on SPI3's hardware NSS pin, since the driver drives chip-select manually as a plain GPIO output. The flash link was originally wired to SPI1 (PA5 CLK, PA6 MISO, PA7 MOSI) but was moved to SPI3 because PA5 conflicts with the NUCLEO-G474RE's onboard LED (LD2), which is hardwired to that pin. This board has no HSE crystal fitted (NUCLEO-64 boards ship with the X3 footprint unpopulated by default), so `SystemClock_Config()` derives the 170 MHz system clock from HSI (16 MHz) through the PLL, not HSE.

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
- `Drivers/BSP/uv/W25Qxx` (`w25q_driver.c`/`.h`, `w25q_spi.c`/`.h`) — the custom, hand-written W25Q128 flash driver, split into a protocol layer (`w25q_driver`) and a low-level SPI transport layer (`w25q_spi`); this is the template for how future custom sensor drivers (BME688, SCD30, MQ7) should be structured. It has its own `CMakeLists.txt` (an `OBJECT` library named `w25qxx_Driver`, linked against `stm32cubemx`) and is pulled into the build via `ADD_SUBDIRECTORY(Drivers/BSP/uv/W25Qxx)` and `target_link_libraries(... w25qxx_Driver)` in the top-level `CMakeLists.txt` — there is no `cmake/w25qxx/` directory or `scope-share.cmake`/`STM_SET_LIBNAME()` macro in this repo; follow the simpler pattern actually used in `Drivers/BSP/uv/W25Qxx/CMakeLists.txt` for new drivers instead.
- `metallicbear.ioc` — the STM32CubeMX project file; peripheral/pin changes should generally be made here and regenerated, not by hand-editing generated `Core/` files outside `USER CODE` blocks.
- `STM32G474xx_FLASH.ld`, `startup_stm32g474xx.s` — linker script and startup assembly, CubeMX/CMSIS-generated.
