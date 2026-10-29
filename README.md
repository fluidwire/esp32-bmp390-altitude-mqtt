# esp32-bmp390-altitude-mqtt — read pressure from a BMP390 and publish calculated altitude over MQTT

IoT example by [Fluidwire](https://fluidwire.com/).

Sample sketch for an ESP32 dev board that reads pressure and temperature from a **Bosch BMP390** barometric sensor over I2C, converts pressure to **altitude** using a sea-level reference you set in `config.h`, and publishes JSON to an MQTT broker. It also reports `rel_alt_m`, the height change relative to the first reading after boot, which is often what a drone or hill-trek logger really needs.

It's a good building block for drone, hill-trek and weather-station thesis prototypes where you care about height changes of well under a metre.

## What it does

Every 5 seconds it triggers a reading and computes altitude with the international barometric formula. The sensor is configured with **oversampling** (8x temperature, 4x pressure) and an **IIR filter** (coefficient 3) so short pressure spikes are smoothed in hardware. If the sensor isn't found, the node publishes an error message and keeps retrying initialisation.

## Required hardware

- 1x ESP32 dev board
- 1x BMP390 breakout (I2C)
- An MQTT broker (local Mosquitto, or a public test broker like `broker.hivemq.com`)
- A 2.4 GHz WiFi network

## Required libraries

Install via Library Manager:

- **PubSubClient** by Nick O'Leary
- **Adafruit BMP3XX Library** by Adafruit
- **Adafruit Unified Sensor** by Adafruit

## Wiring (I2C)

| BMP390 pin | ESP32 pin |
|------------|-----------|
| VIN        | 3V3       |
| GND        | GND       |
| SCL        | GPIO22    |
| SDA        | GPIO21    |

The default I2C address is **0x77**; if your breakout's SDO pin is tied to GND it is **0x76** (change `BMP_ADDR` in the sketch).

## Setup

1. Copy `config.h.example` → `config.h`. Fill in your WiFi + MQTT broker details.
2. Set `SEALEVEL_HPA` to the current sea-level pressure from a local weather report. This is the main calibration step.
3. Wire the BMP390 to the I2C pins above.
4. Flash. Open the Serial Monitor at **115200 baud**.

## Expected serial output

```
[bmp390] found
[wifi] connecting to my-network
....
[wifi] IP: 192.168.1.42
[mqtt] connecting... ok
[mqtt] {"temp_c":28.41,"pressure_hpa":1008.62,"altitude_m":57.3,"rel_alt_m":0.00}
[mqtt] {"temp_c":28.43,"pressure_hpa":1008.55,"altitude_m":57.9,"rel_alt_m":0.60}
```

Subscribe from your machine to watch the data arrive:

```
mosquitto_sub -h broker.hivemq.com -t "fluidwire/demo/bmp390"
```

## Troubleshooting

- **`[bmp390] NOT FOUND`** — check SDA/SCL wiring and 3V3 power, and try the other address (0x76 / 0x77). An I2C scanner helps.
- **Altitude is off by tens of metres** — `SEALEVEL_HPA` is not your local value. Absolute altitude depends on it; `rel_alt_m` does not.
- **Readings drift slowly** — weather pressure changes over hours. Use `rel_alt_m` for short flights or hikes.
- **Noisy readings** — raise the pressure oversampling or IIR coefficient in `initSensor()`.
- **`failed state=-2`** on MQTT — the broker host/port is wrong or unreachable.

## Security note

This sketch keeps your WiFi and MQTT credentials in `config.h`, which is `.gitignore`'d — it never gets committed. Only `config.h.example` ships in the repo. Never hardcode real SSID/password into the `.ino`. For a public broker, anyone subscribed to your topic can see the data; use a private broker with `MQTT_USER`/`MQTT_PASSWORD` (and TLS — see [esp32-mqtt-tls-secure](https://github.com/fluidwire/esp32-mqtt-tls-secure)) for anything sensitive.

## License

MIT — see [LICENSE](LICENSE).

## Got stuck?

Drop by [fluidwire.com](https://fluidwire.com/) — happy to help.
