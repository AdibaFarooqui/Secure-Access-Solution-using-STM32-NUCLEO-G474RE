# Secure Access Management System on STM32 NUCLEO-G474RE

A FreeRTOS-based secure access management prototype built on the STM32 NUCLEO-G474RE.  
The system uses a keypad for PIN entry, an I2C LCD for user feedback, LEDs for status indication, and a passive buzzer for audible alerts. It includes PIN verification, failed-attempt tracking, and exponential lockout behaviour.

## Overview

This project was developed for an embedded computing practical assignment focused on designing and implementing a secure access management solution.

The design began as a simpler HAL-based prototype and was later restructured using FreeRTOS to improve software modularity, timing structure, and overall architectural complexity.

## Features

- 4-digit PIN entry using a 4x4 matrix keypad
- 16x2 I2C LCD user interface
- Masked PIN display on LCD
- Correct / incorrect PIN handling
- Failed-attempt counter
- Exponential lockout after repeated failures
- Red / green LED status indication
- Passive buzzer feedback patterns
- UART debug output over USART2
- FreeRTOS task-based software architecture

## Hardware Used

- STM32 NUCLEO-G474RE
- 4x4 membrane keypad
- 16x2 I2C LCD (address `0x27`)
- 1x green LED
- 1x red LED
- Passive buzzer
- Breadboard and jumper wires
- Resistors for LEDs

## Software Stack

- STM32CubeMX
- STM32CubeIDE
- STM32 HAL
- FreeRTOS (CMSIS-RTOS v2 API)

## Architecture

The final software uses a task-centric FreeRTOS structure:

- **InputTask**  
  Periodically scans the keypad and sends key events

- **AuthTask**  
  Owns the authentication logic and system state

- **UiTask**  
  Owns the LCD and all display updates

- **OutputTask**  
  Owns LEDs and buzzer

- **LoggerTask**  
  Owns UART debug logging

Communication between tasks is handled through queues, and software timers are used for lockout timing and temporary UI timing.

## Scheduling Approach

The system uses **fixed-priority pre-emptive scheduling**.

This was chosen because the software contains multiple concurrent activities with different urgency levels. Authentication and event handling are more time-critical than UI or logging, so the design gives higher priority to the tasks responsible for core access-control behaviour.

The system is also event-driven, meaning tasks mostly wait on queues or timers instead of relying on one large blocking control loop.

## Authentication Logic

1. System boots and displays the idle PIN-entry screen
2. User enters digits on the keypad
3. PIN is masked on the LCD
4. `*` clears the current PIN buffer
5. `#` submits the PIN
6. If the PIN is correct:
   - access granted message is displayed
   - green LED and success buzzer pattern activate
7. If the PIN is incorrect:
   - failed-attempt counter increments
   - denial message is displayed
   - red LED and failure buzzer pattern activate
8. After repeated failed attempts, the system enters a timed lockout state
9. Lockout countdown is shown on the LCD
10. System returns to idle when the countdown expires

## Lockout Policy

The design uses exponential lockout after repeated failed attempts.

- 5th failed attempt → 60 seconds
- 6th failed attempt → 120 seconds
- 7th failed attempt → 240 seconds
- further lockouts continue increasing, with an upper cap in software

This was implemented to improve resistance to brute-force PIN guessing.

## Pin Mapping

### LCD
- `PB8` → I2C1_SCL
- `PB9` → I2C1_SDA

### UART
- `PA2` → USART2_TX
- `PA3` → USART2_RX

### Keypad
- Rows:
  - `PA0`
  - `PA1`
  - `PA4`
  - `PB0`
- Columns:
  - `PA8`
  - `PB10`
  - `PA9`
  - `PC7`

### Outputs
- `PB5` → Green LED
- `PB4` → Red LED
- `PA10` → Passive buzzer

### On-board
- `PA5` → LD2 user LED

## Build and Run

1. Open the project in STM32CubeIDE
2. Ensure the correct board/target is selected
3. Build the project
4. Flash it to the NUCLEO-G474RE
5. Open a serial terminal at **115200 baud** for UART logging
6. Interact with the keypad and observe:
   - LCD status messages
   - LED indications
   - buzzer output
   - UART debug logs
