# LAN8651 Industrial Ethernet Stack

## Overview

This repository contains the development of an industrial Ethernet communication system using STM32 microcontrollers and the LAN8651 Ethernet PHY.

The project focuses on low-level embedded firmware development, industrial communication, USB networking, SPI-based Ethernet communication, PLCA configuration, and performance analysis.

The system supports:

- SPI-based communication with LAN8651
- USB RNDIS/NCM networking
- USB HID communication
- Multi-node Ethernet communication
- PLCA configuration and analysis
- Register-level debugging and performance tuning
- STM32 ↔ BeagleBone communication
- Python-based monitoring and control tools

This project was developed to explore industrial embedded communication systems and real-time networking concepts.

---

# System Architecture

```text
+------------------------------------------------+
|                    PC Host                     |
|        Python Monitoring / Analysis Tool       |
+------------------------------------------------+
                     ↑
              USB RNDIS / NCM
                     ↑
+------------------------------------------------+
|                    STM32                       |
|                                                |
|  +------------------------------------------+  |
|  |             Application Layer            |  |
|  |------------------------------------------|  |
|  | Communication | PLCA | USB | Diagnostics | |
|  +------------------------------------------+  |
|                                                |
|  +------------------------------------------+  |
|  |               Driver Layer               |  |
|  |------------------------------------------|  |
|  | SPI | GPIO | DMA | USB | UART | Timers  |  |
|  +------------------------------------------+  |
+------------------------------------------------+
                     ↓ SPI
+------------------------------------------------+
|                  LAN8651 PHY                   |
+------------------------------------------------+
                     ↓ Ethernet
+------------------------------------------------+
|                 BeagleBone Node                |
+------------------------------------------------+
```

---

# Key Features

## Communication Features

- SPI-based LAN8651 communication
- USB RNDIS networking support
- USB NCM support
- USB HID communication
- Multi-node communication support
- Ethernet packet transmission and reception
- BeagleBone communication testing

## Industrial Networking Features

- PLCA node configuration
- Register-level PHY control
- Multi-node network experimentation
- Communication performance analysis
- Throughput testing
- Packet debugging and monitoring

## Firmware Engineering Features

- STM32 HAL driver integration
- Modular firmware architecture
- Driver abstraction
- Middleware integration
- Low-level register manipulation
- Debugging support

## Analysis and Tooling

- Python backend utility
- Performance analysis scripts
- PLCA configuration interface
- Data monitoring and diagnostics
- Debug logging support

---

# Hardware Used

## Main Controller

- STM32H5 / STM32F4

## Ethernet PHY

- LAN8651

## Additional Platforms

- BeagleBone

## Interfaces Used

- SPI
- USB
- Ethernet
- GPIO
- UART

---

# Software Stack

## Embedded Firmware

- STM32 HAL
- CMSIS
- TinyUSB

## Communication Protocols

- USB RNDIS
- USB NCM
- USB HID
- Ethernet
- SPI

## Development Tools

- STM32CubeIDE
- Python
- Logic Analyzer
- Oscilloscope

---

# Folder Structure

```text
lan8651-industrial-ethernet-stack/
│
├── App/
│   ├── Communication/
│   ├── LAN8651/
│   ├── PLCA/
│   ├── USB/
│   └── Diagnostics/
│
├── Core/
│   ├── Inc/
│   ├── Src/
│   └── Startup/
│
├── Drivers/
│   ├── CMSIS/
│   ├── BSP/
│   └── STM32H5xx_HAL_Driver/
│
├── Middleware/
│   └── TinyUSB/
│
├── Docs/
│   ├── architecture.md
│   ├── spi_driver_design.md
│   ├── plca_notes.md
│   ├── usb_stack_notes.md
│   └── debugging_log.md
│
├── Images/
├── LogicAnalyzer/
├── Performance/
├── Tools/
│   └── PythonBackend/
│
├── README.md
├── .gitignore
└── project.ioc
```

---

# Firmware Architecture

## Layered Design

The firmware is organized into multiple abstraction layers.

### Application Layer

Responsible for:

- Communication handling
- PLCA configuration
- Diagnostics
- Performance monitoring
- Packet processing

### Driver Layer

Responsible for:

- SPI communication
- GPIO handling
- USB interfacing
- DMA transfers
- Peripheral initialization

### Middleware Layer

Responsible for:

- USB protocol stack
- USB class handling
- Networking support

---

# SPI Communication Flow

```text
STM32 Application
        ↓
SPI Driver Layer
        ↓
LAN8651 Register Access
        ↓
Ethernet PHY Communication
        ↓
Network Packet Transfer
```

---

# USB Communication Architecture

The project includes:

- USB RNDIS implementation
- USB NCM communication
- USB HID support

USB communication was used for:

- Device enumeration
- Networking support
- Host communication
- Diagnostics and monitoring

---

# PLCA Configuration

The project includes experimentation with:

- PLCA node configuration
- Multi-node communication
- Register-level PHY setup
- Timing analysis
- Throughput optimization

Configuration was performed through:

- Direct register access
- SPI communication interface
- Diagnostic monitoring tools

---

# Performance Analysis

Performance measurements performed:

- SPI transfer latency
- Ethernet throughput
- USB communication stability
- PLCA timing analysis
- Multi-node communication testing

The repository includes:

- Logic analyzer captures
- Throughput logs
- Timing measurements
- Debugging notes

---

# Engineering Challenges

## SPI Synchronization

Challenges encountered:

- SPI timing stability
- Register synchronization
- Packet integrity verification
- Communication reliability

## USB Enumeration

Challenges encountered:

- Enumeration handling
- Device descriptor debugging
- USB communication stability
- Host recognition issues

## PLCA Debugging

Challenges encountered:

- Node configuration
- Timing adjustments
- Multi-node synchronization
- Throughput optimization

## System Integration

Challenges encountered:

- STM32 ↔ LAN8651 integration
- USB stack integration
- BeagleBone communication testing
- Performance debugging

---

# Development Workflow

## Firmware Development

- Peripheral initialization
- Driver development
- Communication testing
- Register debugging
- Performance optimization

## Validation

- Logic analyzer debugging
- Packet verification
- Throughput testing
- Multi-node testing
- USB communication validation

---

# Python Backend Tool

The repository also contains a Python-based backend utility for:

- Monitoring communication
- Throughput analysis
- PLCA control
- Data visualization
- Diagnostic logging

---

# Repository Goals

This repository is intended to demonstrate:

- Embedded firmware engineering
- Industrial communication systems
- Ethernet communication concepts
- USB stack integration
- Embedded debugging workflows
- Protocol analysis
- Low-level driver development
- Firmware architecture practices

---

# Future Improvements

Planned future improvements:

- FreeRTOS integration
- Advanced diagnostics
- Packet analysis dashboard
- DMA optimization
- Web-based monitoring interface
- Extended multi-node support
- Improved throughput benchmarking

---

# Images and Debugging Captures

The repository includes:

- Hardware setup photos
- Logic analyzer captures
- Communication test screenshots
- USB enumeration logs
- Throughput measurement graphs

---

# Third-Party Components

This project uses:

- STM32 HAL
- CMSIS
- TinyUSB

All third-party components remain property of their respective maintainers.

---

# Author

Embedded firmware and industrial communication development project focused on STM32-based real-time networking systems.

