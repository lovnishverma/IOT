# 🛰️ IoT MQTT ESP8266 Projects (HiveMQ, Firebase, Sensors)

## 📄 About this Repository
This repository contains various IoT projects I have created using ESP8266, HiveMQ Cloud MQTT, Firebase, DHT11 sensors, motion sensors, and relays. These projects demonstrate controlling devices remotely through MQTT, reading sensor data, and publishing it to cloud dashboards.

## 🚀 Features
- ESP8266 + HiveMQ MQTT for Remote Device Control
- Firebase Realtime Database for Sensor Logging
- Relay Control via MQTT
- Servo Motor Control via MQTT
- Motion Detection with MQTT Alerts
- DHT11 Temperature & Humidity Monitoring

## 📂 Directory Structure
```

/src/
├── arduino/
│   ├── 212roomHiveMQ.ino
│   ├── dht11webserver.ino
│   ├── servo\_switch.ino
│   └── motion\_2.0.ino
├── micropython/
│   ├── micropython.md
│   └── micropython.txt
├── mqtt\_examples/
│   ├── control relay from mqtt using hivemqtt.txt
│   ├── 2 relay controls hive mqtt.txt
│   ├── TEMP, HUM.txt
│   └── motion relay.txt
/drivers/
├── CH341SER.zip
├── CP210x\_Universal\_Windows\_Driver.zip
/libraries/
└── pubsubclient-master.zip
/docs/
├── LICENSE
├── README.md
└── ...

```

## 🛠️ Hardware Requirements
- ESP8266 NodeMCU
- DHT11 Temperature & Humidity Sensor
- Relay Module
- Servo Motor
- Motion (PIR) Sensor
- USB to Serial Drivers (CH341SER / CP210x)

## 🔧 Software Requirements
- Arduino IDE
- PubSubClient MQTT Library
- HiveMQ Account
- Firebase Realtime Database

## 📡 Example MQTT Topics
- `room/212/relay1`
- `room/212/relay2`
- `room/212/temperature`
- `room/212/humidity`

## 🔌 HiveMQ Setup
1. Register at [HiveMQ Cloud](https://www.hivemq.com/mqtt-cloud-broker/)
2. Create credentials and endpoint.
3. Update the Arduino `.ino` sketches with your broker details.

## 📷 Circuit Diagrams
_TBD: Please upload Fritzing diagrams for clarity._

## 📚 Included Examples
| File                         | Description                           |
|-------------------------------|---------------------------------------|
| `Firebase-dht11-live-sensor.ino` | DHT11 Sensor to Firebase             |
| `mqttnewbag.ino`               | MQTT Relay Control                    |
| `motion_2.0.ino`               | Motion Sensor via MQTT                |
| `servo_switch_MQTT.ino`        | Servo Control via MQTT                |

## 👨‍💻 How to Run
1. Open `.ino` files with Arduino IDE.
2. Set your Wi-Fi and MQTT credentials.
3. Upload to ESP8266.
4. Monitor via HiveMQ Cloud or MQTT client.

## 📜 License
[MIT](./LICENSE)

## ⚠️ Disclaimer
For educational purposes only. Do not use for production or critical infrastructure without proper security audits.


---


 