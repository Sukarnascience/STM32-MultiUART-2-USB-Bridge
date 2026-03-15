# BeeSerial3x

**Three independent USB serial ports from one STM32F103C8T6 BluePill — raw registers, no HAL, no libopencm3.**

Plug a single USB cable into a $2 BluePill and your PC enumerates three fully independent serial ports simultaneously. All three run in parallel with no multiplexing — each port has its own UART, its own ring buffers, and its own USB bulk endpoint pair.

---

## What This Project Is

BeeSerial3x turns a BluePill into a **3-channel USB-to-UART bridge**. Unlike cheap CH340 or CP2102 adapters that give you one port per chip, this gives you three independent ports from one chip over one USB cable.

```
                         USB cable
                            |
              ┌─────────────┴──────────────┐
              │      BeeSerial3x           │
              │   STM32F103C8T6 BluePill   │
              └──┬──────────┬─────────────┘
                 |          |          |
            USART1      USART2     USART3
           PA9/PA10    PA2/PA3   PB10/PB11

PC sees:
  COM3  <-->  USART1  (115200 8N1, independently controllable)
  COM4  <-->  USART2  (115200 8N1, independently controllable)
  COM5  <-->  USART3  (115200 8N1, independently controllable)
```

---

## Why This Was Built

- **Embedded systems development** often requires monitoring multiple UART devices at the same time — a main MCU, a modem, a GPS module — without buying three separate USB adapters
- **Protocol bridging** between devices with different baud rates on each channel
- **Learning raw USB firmware** on STM32 — this project implements the full USB CDC ACM stack from scratch against raw peripheral registers with no middleware
- **BeeBotix research platform** for autonomous systems that generate multi-channel serial telemetry

---

## Applications

- Debug three embedded boards simultaneously from one laptop
- Bridge RS-232 / TTL devices with independent baud rates per port
- Multi-channel data logger (each UART feeds a different sensor)
- USB CDC ACM reference implementation for STM32F1 (fully documented)
- Test fixture that generates/receives serial traffic on three channels at once

---

## Pin Map

```
BluePill (STM32F103C8T6)
                    ┌───────────────────┐
             VBAT   │  1            40  │  VCC (3.3V)
              PC13  │  2  LED       39  │  GND
              PC14  │  3            38  │  GND
              PC15  │  4            37  │  3.3V out
              PD0   │  5            36  │  BOOT1 (keep LOW=0)
              PD1   │  6            35  │  BOOT0 (keep LOW=0)
              RST   │  7            34  │  PB11  <-- USART3 RX (Port 2)
              PA0   │  8            33  │  PB10  <-- USART3 TX (Port 2)
              PA1   │  9            32  │  PB1
              PA2   │  10 USART2 TX 31  │  PB0
              PA3   │  11 USART2 RX 30  │  PA7
              PA4   │  12           29  │  PA6
              PA5   │  13           28  │  PA5
              PA6   │  14           27  │  PA4
              PA7   │  15           26  │  PA3   USART2 RX
              PB0   │  16           25  │  PA2   USART2 TX
              PB1   │  17           24  │  PA1
              PB10  │  18 USART3 TX 23  │  PA0
              PB11  │  19 USART3 RX 22  │  PC15
              3.3V  │  20           21  │  GND
                    └───────────────────┘

          PA9  = USART1 TX  (Port 0)   ← near USB connector, top row
          PA10 = USART1 RX  (Port 0)
          PA2  = USART2 TX  (Port 1)
          PA3  = USART2 RX  (Port 1)
          PB10 = USART3 TX  (Port 2)
          PB11 = USART3 RX  (Port 2)
          PC13 = LED (active low, blinks slow/fast as status)
          PA12 = USB D+  (driven by USB peripheral, do not connect externally)
          PA11 = USB D-  (driven by USB peripheral, do not connect externally)
```

### Connection Table

| PC COM Port | USB CDC Port | BluePill TX | BluePill RX | Default Baud |
|-------------|-------------|-------------|-------------|--------------|
| COM3        | Port 0      | PA9         | PA10        | 115200       |
| COM4        | Port 1      | PA2         | PA3         | 115200       |
| COM5        | Port 2      | PB10        | PB11        | 115200       |

> COM port numbers vary by machine. Check Device Manager after plugging in.

**Wiring to a target device:**

```
BluePill TX  ──────────────►  Target RX
BluePill RX  ◄──────────────  Target TX
BluePill GND ───────────────  Target GND   (always share ground)
```

---

## How It Works

### USB Stack

The firmware implements **USB Full-Speed CDC ACM** (USB Communications Device Class, Abstract Control Model) from scratch using raw STM32F1 peripheral registers. No HAL, no libopencm3, no USB middleware.

```
PC
│
│  USB FS 12 Mbps
│
STM32F103 USB peripheral
│
├── EP0  Control  (enumeration, SETUP requests)
├── EP1  Interrupt IN  (Port 0 CDC notification — NAK only)
├── EP2  Bulk IN/OUT   (Port 0 serial data)
├── EP3  Interrupt IN  (Port 1 CDC notification)
├── EP4  Bulk IN/OUT   (Port 1 serial data)
├── EP5  Interrupt IN  (Port 2 CDC notification)
└── EP6  Bulk IN/OUT   (Port 2 serial data)
```

The composite device uses **Interface Association Descriptors (IAD)** so Windows loads `usbser.sys` automatically for each of the three CDC pairs — no driver installation needed.

### Data Flow

```
PC writes to COM3
  └─► USB OUT EP2  ─► ring buffer (uart_ch[0].tx)
                       └─► USART1 TX interrupt ─► PA9 pin

PA10 pin (USART1 RX)
  └─► USART1 RX interrupt ─► ring buffer (uart_ch[0].rx)
                              └─► main loop poll ─► USB IN EP2 ─► PC reads COM3
```

Each channel has a **256-byte lock-free SPSC ring buffer** for both TX and RX directions. UART RX and TX are interrupt-driven. USB polling runs in the main loop.

### Clock

HSE 8 MHz crystal × PLL6 = **48 MHz SYSCLK**. USB clock = 48 MHz (no division). The external crystal is mandatory — the internal RC oscillator (HSI) is ±1% which is 20× outside the USB ±500ppm tolerance.

### USB Register Access

A critical implementation detail: STM32F1 USB registers are **16-bit wide**. Accessing them as `uint32_t` (the common mistake) silently corrupts the peripheral. All USB register access in this firmware uses `volatile uint16_t*` pointers.

### LED Status

| Pattern | Meaning |
|---------|---------|
| 3 fast blinks on boot | MCU started, clock OK |
| Slow blink 500 ms | USB connected, no port open |
| Fast blink 100 ms | At least one COM port open |

---

## Building and Flashing

**Requirements:** PlatformIO (VS Code extension or CLI), ST-Link V2

```bash
# Build
pio run

# Flash
pio run --target upload

# Clean build (recommended after any file change)
pio run --target clean && pio run --target upload
```

After flashing:
1. Disconnect and reconnect the USB cable
2. Windows Device Manager → Universal Serial Bus controllers → three "USB Serial Device (COMx)" entries appear
3. No driver installation required on Windows 10/11, Linux, or macOS

---

## Stress Testing with Arduino

The `test_stress/BeeSerial_Stress.ino` sketch tests one BluePill UART port using an ATmega328P-based Arduino (UNO, Nano, Pro Mini).

### Wiring

```
Arduino UNO/Nano          BluePill
─────────────────         ──────────────────────────────
TX  (D1)         ──────►  RX of target port
RX  (D0)         ◄──────  TX of target port
GND              ───────  GND

For Port 0:  BluePill PA9(TX) / PA10(RX)
For Port 1:  BluePill PA2(TX) / PA3(RX)
For Port 2:  BluePill PB10(TX)/ PB11(RX)
```

> The Arduino must share GND with the BluePill. Logic levels are both 3.3V/5V tolerant on UART — direct connection works.

### Test Modes

Set `TEST_MODE` at the top of the sketch before flashing to Arduino:

**Mode 0 — Echo test** (default)

Arduino sends `PING #N`, waits up to 500ms for the PC to echo it back through the COM port. Measures round-trip latency and counts failures.

On PC: open the matching COM port in a terminal with local echo enabled, or run:
```python
import serial
p = serial.Serial('COM3', 115200, timeout=1)
while True:
    d = p.read(p.in_waiting or 1)
    if d: p.write(d)
```

**Mode 1 — Blast test**

Arduino sends bytes at full UART speed continuously and counts how many come back. Reports TX rate, RX rate, and drop count every second. Tests buffer integrity under load.

**Mode 2 — Slow test**

One timestamped message per second. Tests that the USB bridge correctly handles idle periods and wakes up cleanly after long silences.

**Mode 3 — Baud rate stress**

Cycles through 9600 / 19200 / 38400 / 57600 / 115200 every ~800ms. Tests that `SET_LINE_CODING` USB requests correctly reconfigure the USART BRR register at runtime.

### Full Parallel Test

Wire three Arduinos to all three BluePill UART ports simultaneously. Flash each Arduino with a different `TEST_MODE` and `PORT_LABEL`. Open all three PC COM ports at once. All three channels should run independently with no interference.

---

## File Structure

```
BeeSerial3x/
├── platformio.ini
├── include/
│   ├── stm32f1_regs.h      Raw register definitions (RCC, GPIO, USART, USB 16-bit, NVIC, SysTick)
│   ├── clock.h             Clock API
│   ├── ring_buffer.h       Lock-free SPSC ring buffer (256 bytes, power-of-2)
│   ├── uart_driver.h       3-channel USART driver API
│   └── usb_cdc3.h          USB CDC 3-port API
├── src/
│   ├── main.c              Init sequence, main poll loop, LED status
│   ├── clock.c             HSE + PLL48MHz, SysTick 1ms, HAL tick stubs
│   ├── uart_driver.c       USART1/2/3 interrupt-driven RX+TX
│   └── usb_cdc3.c          Full USB CDC ACM stack: descriptors, EP0 state machine,
│                           SETUP handler, polling loop, UART bridge
└── test_stress/
    └── BeeSerial_Stress.ino   Arduino ATmega328P stress test (4 modes)
```

---

## Known Limitations

- **16-byte USB max packet size** on data endpoints (fits in 512-byte PMA with 3 ports). Throughput is limited to ~16 KB/s per port, sufficient for typical embedded serial at up to 115200 baud.
- **No hardware flow control** (RTS/CTS). If the host writes faster than the UART drains, the 256-byte TX ring drops bytes. Increase `RING_SIZE` in `ring_buffer.h` if needed (keep power of 2, max ~512 bytes to stay in SRAM).
- **Single USB cable** — the BluePill must be powered via USB. Do not also power it from an external 5V supply on VIN simultaneously.

---

*BeeBotix Autonomous Systems — Sukarna Jana*