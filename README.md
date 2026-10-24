# ESP32 Web Relay

A web-controlled traffic light built around an ESP32, a PN2222A NPN transistor, and a 5 V relay module. The ESP32 serves a small control page on your local network; pressing the button switches the relay, which powers the traffic light.

## Features

- On/off button in a responsive browser UI
- Live state polling, so multiple tabs or phones stay in sync
- Joins your Wi-Fi network and answers at `http://esp32-relay.local` (mDNS)
- Optional static IP
- Falls back to its own access point when Wi-Fi is unavailable
- Fully self-contained page: no external fonts, scripts, or network access needed

## Hardware

| Part | Notes |
| --- | --- |
| ESP32 dev board | Any board with GPIO 18 broken out |
| PN2222A NPN transistor | Low-side switch for the relay input |
| 5 V relay module | One channel |
| 1 kΩ resistor | Base resistor |
| 5 V supply | Micro-USB breakout feeding the ESP32, relay, and light |

### Wiring

| From | Connects to |
| --- | --- |
| VBUS (5 V) | ESP32 VIN, PN2222A collector, relay VCC |
| GND | ESP32 GND, relay GND |
| GPIO 18 | 1 kΩ resistor → PN2222A base |
| PN2222A emitter | GND (common-emitter) **or** relay IN (emitter-follower, current wiring) |

The relay input logic is set by `RELAY_ACTIVE_LOW` in the sketch. Most relay modules are active-low; if the button switches the light in reverse, flip that constant.

## Setup

1. Install the Arduino IDE and the ESP32 board package.
2. Copy `arduino_secrets.example.h` to `arduino_secrets.h` and fill in your Wi-Fi details.
3. Optionally set a static IP in the same file with `WIFI_STATIC_IP`, `WIFI_GATEWAY`, `WIFI_SUBNET`, and `WIFI_DNS`. Comment out the block to use DHCP (`WIFI_DNS` defaults to the gateway).
4. Open `ESP32-web-relay.ino`, select your ESP32 board, and upload.
5. Open the serial monitor at 115200 baud to see the assigned address.

`arduino_secrets.h` is git-ignored, so credentials and network settings stay out of the repository.

## Usage

- **Wi-Fi mode:** open `http://<board-ip>` or `http://esp32-relay.local`.
- **Static IP:** use the address configured in `arduino_secrets.h` directly.
- **Access-point fallback:** if no credentials are present or the connection fails, join `ESP32-Relay` (password `relay1234`) and open `http://192.168.4.1`. Change `AP_SSID` and `AP_PASSWORD` in the sketch to customize this.

The page shows red when the light is off, green when it is on, and amber while a command is in flight. If the board cannot be reached, it reports "Not connected" and retries every few seconds.

## HTTP API

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/` | Control page |
| `GET` | `/state` | Current state as JSON: `{"on":true}` or `{"on":false}` |
| `POST` | `/toggle` | Toggle the relay and return the new state |

## Project layout

```
ESP32-web-relay.ino          Firmware and embedded web page
arduino_secrets.example.h    Wi-Fi and network settings template
Schematic/                   KiCad project, symbols, and footprints
```

## Notes

- The control page has no authentication. Anyone on the same network can toggle the relay.
- The access point password is a default; change it before using AP mode outside a bench setup.
