# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

EnviLogger is a bare-metal (no RTOS) firmware project for the NUCLEO-G474RE board (STM32G474RET6, Cortex-M4 @ 170 MHz). It's a learning project: an automotive cabin air-quality monitor that reads CO (MQ7), CO2/RH/temp (SCD30), IAQ gas (BME688), logs to external SPI NOR flash (W25Q128), and streams debug output over a Virtual COM Port. Full design doc: `doc/cubemx.md` (also duplicated as the project `README.md`).

Drivers are built in a deliberate sequence, from simplest bus to most complex:
1. **W25Q128 flash (SPI1)** — in progress (see `Drivers/W25Qxx/`)
2. **BME688 (I2C1)** — not started
3. **SCD30 (USART1 + DMA, Modbus RTU/CRC16)** — not started
4. **MQ7 (ADC1 + TIM1 PWM)** — not started

Hardware pin map (see `doc/cubemx.md` §3 for full electrical detail):
| Peripheral | Pins | Purpose |
|---|---|---|
| SPI1 | PA5 CLK, PA6 MISO, PA7 MOSI, PA4 CS (soft NSS, GPIO output labeled `W25Q_CS`) | W25Q128 flash |
| I2C1 | PB8 SCL, PB9 SDA | BME688 |
| USART1 | PA9 TX, PA10 RX (+ RX DMA, circular) | SCD30 Modbus, 19200 baud |
| USART2 | PA2 TX, PA3 RX | VCP debug console via ST-LINK, 115200 baud, `printf` redirected here |
| ADC1 | PA0 (IN1) | MQ7 analog read |
| TIM1 | PA8 (CH1 PWM) | MQ7 heater drive |
| EXTI13 | PC13 | User button B1, falling edge |

Planned safety features (see `doc/cubemx.md` §6, not yet implemented): VREFINT brownout check before flash erase/write, IWDG watchdog fed within a 2000 ms loop budget.

## Build system

CMake + Ninja, cross-compiled for `arm-none-eabi` (Cortex-M4, hard float, fpv4-sp-d16). This is STM32CubeMX-generated CMake structure — do not hand-edit the generated blocks in `cmake/stm32cubemx/CMakeLists.txt` beyond what CubeMX would produce; add project code via the top-level `CMakeLists.txt`'s "Add user..." sections or a dedicated subdirectory (see `cmake/w25qxx/` for the pattern a new driver library should follow).

Configure and build with presets (from `CMakePresets.json`):
```sh
cmake --preset Debug        # or Release
cmake --build --preset Debug
```
Output ELF/map land in `build/Debug/` (e.g. `build/Debug/envilogger.elf`, `envilogger.map`).

There are two toolchain files under `cmake/`:
- `gcc-arm-none-eabi.cmake` — GNU arm-none-eabi-gcc/g++ (used by the `CMakePresets.json` presets; requires `arm-none-eabi-*` on `PATH`)
- `starm-clang.cmake` — alternate Clang-based toolchain (`starm-clang`/`starm-clang++`), not wired into the presets by default

No flashing/debugging scripts are checked in; use STM32CubeIDE, OpenOCD, or ST-LINK utilities externally against the built `.elf`.

There is no test suite in this repo (bare-metal firmware, HIL/manual test protocol only — see `doc/cubemx.md`, "Hardware Driver Verification & Testing Protocol", for the manual bring-up test steps per peripheral, e.g. JEDEC ID read, sector erase/program/read loop, I2C WhoAmI, Modbus frame check, PWM/ADC timing).

## Code architecture

- `Core/Src`, `Core/Inc` — STM32CubeMX-generated peripheral init and startup code (`main.c`, `gpio.c`, `spi.c`, `adc.c`, `tim.c`, `usart.c`, `stm32g4xx_it.c`, HAL MSP/conf). Treat CubeMX-managed regions (`/* USER CODE BEGIN ... END */` blocks) as the only place to hand-edit generated files, since regenerating from `envilogger.ioc` will overwrite everything else.
- `Drivers/STM32G4xx_HAL_Driver` — vendored ST HAL/LL sources. The SPI HAL/LL files here are currently untracked (added manually to support SPI1 before a CubeMX regeneration picked them up) — check `git status` before assuming the HAL driver set matches what CubeMX would generate.
- `Drivers/CMSIS` — vendored ARM CMSIS + ST device headers.
- `Drivers/W25Qxx` (`w25q.c`/`w25q.h`) — the custom, hand-written W25Q128 flash driver; this is the template for how future custom sensor drivers (BME688, SCD30, MQ7) should be structured and wired into `cmake/<driver>/CMakeLists.txt`.
- `cmake/w25qxx/CMakeLists.txt` — pattern for adding a new driver as a small CMake object library: an INTERFACE target for includes, an OBJECT library for sources, linked into the main `${CMAKE_PROJECT_NAME}` target. Uses the `STM_SET_LIBNAME()` macro from `cmake/scope-share.cmake` to derive interface/library names from the directory name.
- `device/include/logger.h` — lightweight debug tracing (`METHODTRACE`, `LOGTRACE`) compiled to no-ops unless `DEBUG` is defined; note it's written as C++ (`cstdint`/`cstdio`, a `MethodTracer` class) even though the rest of the firmware is C — only include it from C++ translation units.
- `envilogger.ioc` — the STM32CubeMX project file; peripheral/pin changes should generally be made here and regenerated, not by hand-editing generated `Core/` files outside `USER CODE` blocks.
- `STM32G474xx_FLASH.ld`, `startup_stm32g474xx.s` — linker script and startup assembly, CubeMX/CMSIS-generated.
