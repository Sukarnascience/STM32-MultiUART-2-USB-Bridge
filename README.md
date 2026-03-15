# BeeSerial3x

Three independent UART-to-USB CDC ACM bridges on a single STM32F103C8T6 (BluePill).

Plug in one USB cable and the host enumerates **three separate serial ports simultaneously**.
All three ports are fully independent — no multiplexing, no time-sharing.

---

## Hardware

**MCU:** STM32F103C8T6 (BluePill, 64 KB flash / 20 KB SRAM)

### Pin Map

```
USART1  (USB Port 0)   TX = PA9    RX = PA10
USART2  (USB Port 1)   TX = PA2    RX = PA3
USART3  (USB Port 2)   TX = PB10   RX = PB11
```

> **Note:** USART1 TX (PA9) is the same GPIO bank as the USB D+ re-enumeration
> pull-down (PA12). They do not conflict but keep SWD debug attached only
> when not using USART1 simultaneously, or re-map USART1 to PB6/PB7 via AFIO
> if you need both at the same time.

### Connections

Wire your external UART targets as follows:

```
BeeSerial3x BluePill          External device
----------------               ---------------
PA9  (USART1 TX)  ---------->  RX
PA10 (USART1 RX)  <----------  TX
GND               -----------  GND

PA2  (USART2 TX)  ---------->  RX
PA3  (USART2 RX)  <----------  TX
GND               -----------  GND

PB10 (USART3 TX)  ---------->  RX
PB11 (USART3 RX)  <----------  TX
GND               -----------  GND
```

---

## USB Device Identity

| Field        | Value          |
|--------------|----------------|
| Manufacturer | BeeBotix       |
| Product      | BeeSerial3x    |
| Serial       | BEESRL001      |
| VID          | 0x1D50         |
| PID          | 0x6018         |
| Class        | CDC ACM x 3    |

---

## Architecture

```
                     STM32F103C8T6
  +-----------+      +-----------------------------------------+
  |  USB Host |      |                                         |
  |           |      |  USB FS Peripheral (libopencm3)         |
  |  /dev/    |<---->|                                         |
  |  ttyACM0  |      |  CDC ACM Port 0  <-->  USART1 ISR      |
  |  ttyACM1  |      |  CDC ACM Port 1  <-->  USART2 ISR      |
  |  ttyACM2  |      |  CDC ACM Port 2  <-->  USART3 ISR      |
  |           |      |                                         |
  +-----------+      |  Ring buffers (SPSC, 256 B each)        |
                     |    uart_ch[n].rx  UART->USB             |
                     |    uart_ch[n].tx  USB->UART             |
                     |                                         |
                     |  Main loop: usb_cdc3_poll()             |
                     |    drains rx rings to USB IN EPs        |
                     +-----------------------------------------+
```

### Endpoint Map

```
EP  Address  Direction  Type       Port
--  -------  ---------  ---------  ----
1   0x81     IN         Interrupt  Port 0 CDC notification
2   0x02     OUT        Bulk       Port 0 data (PC -> UART1 TX)
2   0x82     IN         Bulk       Port 0 data (UART1 RX -> PC)
3   0x83     IN         Interrupt  Port 1 CDC notification
4   0x04     OUT        Bulk       Port 1 data (PC -> UART2 TX)
4   0x84     IN         Bulk       Port 1 data (UART2 RX -> PC)
5   0x85     IN         Interrupt  Port 2 CDC notification
6   0x06     OUT        Bulk       Port 2 data (PC -> UART3 TX)
6   0x86     IN         Bulk       Port 2 data (UART3 RX -> PC)
```

---

## Building

### Requirements

- PlatformIO Core or PlatformIO IDE (VS Code extension)
- ST-Link V2 programmer
- libopencm3 (fetched automatically by PlatformIO)

### Build and flash

```bash
pio run -t upload
```

Or in VS Code: **PlatformIO: Upload**

### Monitor (any one port)

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

---

## Testing

Install pyserial, then run the parallel test script:

```bash
pip install pyserial

# Auto-discover (Linux/macOS)
python test_beeserial.py

# Explicit ports (Linux)
python test_beeserial.py /dev/ttyACM0 /dev/ttyACM1 /dev/ttyACM2

# Explicit ports (Windows)
python test_beeserial.py COM3 COM4 COM5
```

The script opens all three ports simultaneously, sends a numbered ping every
second on each, and prints all received data with a port label.

To loopback-test without external devices: wire PA9->PA10, PA2->PA3, PB10->PB11.

---

## Runtime Behavior

- **LED (PC13, active low):** slow blink (500 ms) when no port open, fast blink (100 ms) when any port open.
- **Baud reconfiguration:** changing the baud rate from the host side (e.g. in minicom/PuTTY) automatically reconfigures the corresponding USART via CDC SetLineCoding.
- **DTR gating:** data is only forwarded to USB when the host application has the port open (DTR asserted). This prevents buffer overflows during device enumeration.

---

## Source Layout

```
BeeSerial3x/
  platformio.ini          PlatformIO project config
  include/
    ring_buffer.h         Lock-free SPSC ring buffer (power-of-2 size)
    uart_driver.h         UART channel API
    usb_cdc3.h            USB CDC 3-port API
  src/
    main.c                Clock setup, init, main loop
    uart_driver.c         USART1/2/3 interrupt-driven driver
    usb_cdc3.c            Composite CDC ACM descriptors and USB stack
  test_beeserial.py       Python parallel port tester
```

---

## Known Limitations

- Flash: approximately 30-35 KB used out of 64 KB.
- No hardware flow control (RTS/CTS). If the host sends faster than the UART
  can drain, the 256-byte TX ring will drop bytes. Increase RING_SIZE in
  ring_buffer.h if needed (keep it a power of 2, max ~1 KB to stay in SRAM).
- USART1 and USART2 share GPIOA, and USART1 TX (PA9) is near the USB
  re-enumeration pin (PA12). Both work independently in firmware.

---

*BeeBotix Autonomous Systems — Sukarna Jana*