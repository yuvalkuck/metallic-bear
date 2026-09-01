## System Architecture & Interconnect Topology

The MetalicBear system operates as an embedded automotive cabin air quality monitor and telemetry logging engine. It captures raw carbon monoxide spikes, absolute carbon dioxide values, and multi-variable atmospheric air parameters to evaluate driver fatigue states.

Telemetry data is packaged into structured, uniform binary arrays and committed locally to non-volatile flash memory via a serialized communication link. Debug tracking and system state inspection are achieved via an integrated Virtual COM Port (VCP) link back to a PC console.

```mermaid
graph TD
    %% Node Definitions
    MCU[STM32G474 Microcontroller<br>170 MHz Core]
    MQ7[MQ7 Gas Sensor<br>Carbon Monoxide]
    SCD30[SCD30 Sensor<br>CO2, RH, Temp]
    BME688[BME688 Sensor<br>IAQ Gas, Pressure]
    W25Q[W25Q128 Flash<br>Data Logger Memory]
    VCP[Virtual COM Port<br>PC Serial Terminal]
    B1[User Button B1<br>PC13 EXTI]

    %% Hardware Connections
    MCU -->|Analog ADC1_IN1 / TIM1_CH1 PWM| MQ7
    MCU -->|UART Asynchronous USART1| SCD30
    MCU -->|I2C Multi-Master I2C1| BME688
    MCU -->|SPI Master Bus SPI1| W25Q
    B1 -->|Hardware Falling Edge Interrupt| MCU

    %% Styling Elements
    style MCU fill:#1f618d,stroke:#114b70,stroke-width:2px,color:#fff
    style MQ7 fill:#d35400,stroke:#ba4a00,stroke-width:1px,color:#fff
    style SCD30 fill:#27ae60,stroke:#1e8449,stroke-width:1px,color:#fff
    style BME688 fill:#2e4053,stroke:#212f3d,stroke-width:1px,color:#fff
    style W25Q fill:#7d3c98,stroke:#6c3483,stroke-width:1px,color:#fff
    style VCP fill:#117a65,stroke:#0e6251,stroke-width:1px,color:#fff
    style B1 fill:#922b21,stroke:#7b241c,stroke-width:1px,color:#fff
```

---

## Hardware Module Interface Mapping

| Module | Core Purpose | Interface Type | Pin Allocation | Electrical Requirements |
| :--- | :--- | :--- | :--- | :--- |
| **MQ7** | Toxic Carbon Monoxide tracking | Analog (ADC) + PWM | PA0 (ADC1_IN1), PA8 (TIM1_CH1) | 5.0V / 1.4V Dual VCC Cycles |
| **SCD30** | Optical NDIR CO2 monitoring | Asynchronous UART | PA9 (TX), PA10 (RX) [USART1] | 3.3V - 5.5V DC VCC |
| **BME688** | 4-in-1 Volatile Gas/IAQ | I2C Multi-Master Bus | PB8 (SCL), PB9 (SDA) [I2C1] | 1.2V - 3.6V DC VCC (3.3V Typ) |
| **W25Q128**| 128M-bit Non-Volatile Flash | SPI Master Bus | PA5(CLK), PA6(MISO), PA7(MOSI), PA4(CS) | 2.7V - 3.6V DC VCC |
| **Button B1** | Hardware Event Interrupt | External EXTI Line | PC13 (Hardwired Blue Switch) | Active-Low External Pull-up |



## Low-Level Driver Learning Roadmap

To master bare-metal peripheral programming from scratch, EnviLogger drivers are partitioned and developed using a clear sequential protocol roadmap:

### Phase 1: W25Q128 Serial Flash (SPI Master)
* **Learning Intent:** Master master-slave hardware clocks, manual Chip Select pin control, data shifting alignment, instruction sets, and non-volatile flash page boundary processing.
* **Why first:** The clean, deterministic nature of synchronous SPI communication simplifies checking byte integrity during writes and reads.

### Phase 2: BME688 Air Quality Sensor (I2C Register-Mapped Bus)
* **Learning Intent:** Master I2C Start/Stop conditions, 7-bit slave address matching, register pointer selection writes, multi-byte burst reading, and executing factory calibration polynomials.
* **Why second:** Introduces standard register addressing architectures over a shared 2-wire bus layout.

### Phase 3: SCD30 Gas Array Module (UART Frame Parsing)
* **Learning Intent:** Master asynchronous streaming, Direct Memory Access (DMA) channel processing utilizing a circular ring buffer design, and verification of multi-byte Modbus RTU checksum frames (CRC16).
* **Why third:** Shifts focus from low-level register matching to heavy frame structure processing and error validation protocols.

### Phase 4: MQ7 Sensor Management (MCU Core Analog & Timers)
* **Learning Intent:** Master internal microcontroller core configurations. Drive external transistor paths using hardware PWM outputs, execute tracking time profiles, and isolate analog read windows.
* **Why last:** Teaches how to coordinate multiple internal chip systems (TIM and ADC blocks) to execute complex, time-dependent physical workloads.
