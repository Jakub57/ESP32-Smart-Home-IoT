# ESP32 Smart Home IoT System

A distributed home automation system consisting of two autonomous nodes: a battery-powered wireless environment sensor and a mains-powered multi-channel relay actuator. Both nodes integrate directly with Home Assistant via HTTP REST API and Webhooks, eliminating the need for intermediary MQTT brokers.

## Architecture Overview
The system is built around the ESP32 platform, utilizing its Wi-Fi capabilities and deep sleep efficiency:
1. **Sensor Node:** Periodically wakes up from Deep Sleep, reads environmental data, sends a JSON payload to Home Assistant via a Webhook, and goes back to sleep.
2. **Actuator Node:** Operates in continuous listening mode, handling HTTP GET requests to control isolated relay channels while asynchronously reporting state changes back to Home Assistant to ensure UI synchronization.

## Key Features
* **Deep Sleep Optimization:** The sensor node utilizes RTC memory to store BSSID and Wi-Fi channel data, bypassing standard network scanning to connect and transmit data in fractions of a second, drastically extending Li-Po battery life.
* **Local Web UI (Captive Portal):** Both modules feature a physical BOOT button interrupt that triggers an Access Point mode. A custom HTML/CSS graphical interface allows for dynamic configuration of SSID, Home Assistant IP, and static IP settings, which are permanently stored in NVS memory.
* **Direct Integration:** Seamless two-way communication using Webhooks (Node -> HA) and REST API (HA -> Node) for immediate state synchronization.

## Tech Stack & Hardware Components
### Sensor Node
* **Microcontroller:** ESP32-WROOM-32
* **Sensor:** DHT22 (AM2302) Temperature & Humidity Sensor
* **Power:** 800mAh Li-Po battery with TP4056 charging module
* **Enclosure:** Custom 3D-printed PETG case with functional ventilation slots

### Actuator Node
* **Microcontroller:** ESP32-WROOM-32
* **Relays:** 4-channel (scalable to 8) 5V relay module with optocoupler isolation
* **Power:** 12V AC/DC adapter with an integrated step-down DC-DC converter to 5V

## Hardware Showcase
![IoT System](TUTAJ_WPISZ_NAZWE_ZDJECIA.jpg)
