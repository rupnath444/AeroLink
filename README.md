# Aerolink: two separate LoRa devices

The firmware is intended for **two separate ESP32 devices**. Each ESP32 is
connected to its own RYLR998 module. The two RYLR998 modules communicate
wirelessly; they must not be connected to each other by UART wires.

## Device roles

| Device | PlatformIO environment | RYLR998 address | Destination |
| --- | --- | ---: | ---: |
| A | `esp32_node_a` | 1 | 2 |
| B | `esp32_node_b` | 2 | 1 |

The addresses are selected by `build_flags` in `platformio.ini`. Flash
`esp32_node_a` to the ESP32 connected to module A and `esp32_node_b` to the
other ESP32.

## Wiring for each ESP32

Use the same wiring on both devices:

| ESP32 | RYLR998 |
| --- | --- |
| GPIO17 (UART TX) | RXD |
| GPIO16 (UART RX) | TXD |
| GND | GND |
| Compatible module power | VDD |

The firmware uses UART2 at 115200 baud. Connect TX to RX and RX to TX; do not
connect TX to TX. Power the module from a supply appropriate for the specific
RYLR998 breakout/module, and ensure the ESP32 and module share ground. Do not
apply 5 V to an ESP32 GPIO or to a bare 3.3 V-only module.

The optional local controls are connected to each ESP32:

| ESP32 pin | Function |
| --- | --- |
| GPIO32 | Red LED |
| GPIO33 | Green LED |
| GPIO25 | Yellow LED |
| GPIO18 | Button 1 to GND |
| GPIO19 | Button 2 to GND |
| GPIO21 | Button 3 to GND |
| GPIO22 | Button 4 to GND |

Buttons use `INPUT_PULLUP`, so each button is wired between its GPIO and GND.
Use suitable current-limiting resistors for LEDs.

## Configure the RYLR998 modules

Configure both modules with the same LoRa radio settings and network ID, but
different addresses:

```text
Module A: AT+ADDRESS=1
Module B: AT+ADDRESS=2
Both:     same AT+NETWORKID value
Both:     UART baud rate 115200
```

Terminate each command as required by the module. Verify the exact supported
command syntax and power requirements in the RYLR998 datasheet for the
breakout being used. The firmware also sends `AT+ADDRESS` and
`AT+NETWORKID=10` to the attached module at startup, using the address and
network ID defined by the selected PlatformIO environment.

## Build and upload

```text
pio run -e esp32_node_a -t upload --upload-port COMx
pio run -e esp32_node_b -t upload --upload-port COMx
```

Run only the command for the node assigned to that computer. Replace `COMx`
with the port shown by that computer (for example, `COM3`). The serial port is
intentionally not stored in `platformio.ini`, because Windows can assign a
different port on each machine.

Open the monitor for the same node and port:

```text
pio device monitor -p COMx -b 115200
```

For example, on the computer connected to node B:

```text
pio run -e esp32_node_b -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```

A button press or a line entered in the serial monitor on either device is
sent wirelessly to the other device and acknowledged. The two computers do
not need to be connected to each other; only the RYLR998 modules communicate
with each other over LoRa.
