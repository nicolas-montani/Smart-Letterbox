# Smart Letterbox MQTT System v2

A complete IoT solution for monitoring your letterbox using an ESP32 with an ultrasonic sensor and a Raspberry Pi server with MQTT communication.

## Overview

This system allows you to monitor your letterbox remotely by measuring the distance inside the letterbox using an ultrasonic sensor. The ESP32 sends the raw data directly to an MQTT broker running on a Raspberry Pi, which then processes and displays the data on a web dashboard.

## Key Features

### ESP32 Sensor Node
- Measures distance using an ultrasonic sensor every second
- Monitors battery voltage and estimates consumption
- Sends raw data directly to the MQTT broker
- Optimized for low power consumption
- Powered by a 10000mAh battery pack via micro USB

### Raspberry Pi Server
- Receives and logs all data from the ESP32
- Displays real-time data on a modern web dashboard
- Shows distance over time in an interactive graph
- Highlights distances greater than 5mm in red
- Tracks battery consumption and estimates remaining time
- Stores historical data for analysis

## Directory Structure

```
letterbox_ultrasound_mqtt_v2/
├── letterbox_ultrasound_mqtt_v2.ino  # ESP32 Arduino code
├── raspberry_pi_mqtt_server_v2.py    # Raspberry Pi server code
├── setup_instructions.md             # Detailed setup instructions
├── README.md                         # This file
├── templates/                        # HTML templates for the web interface
└── static/                           # CSS and other static files
```

## Quick Start

1. Set up the ESP32 with the ultrasonic sensor according to the wiring instructions in `setup_instructions.md`
2. Update the WiFi and MQTT settings in the ESP32 code
3. Upload the code to the ESP32
4. Set up the Raspberry Pi with the MQTT broker and server
5. Run the server on the Raspberry Pi
6. Access the dashboard by navigating to the Raspberry Pi's IP address in a web browser

For detailed instructions, see [setup_instructions.md](setup_instructions.md).

## Dashboard

The web dashboard provides:
- Real-time distance measurements
- Battery status and consumption
- Interactive graphs showing distance and battery trends over time
- Visual alerts when distance exceeds 5mm

## Improvements from v1

- Direct transmission of raw data from ESP32 to MQTT broker
- Periodic updates (every 10 seconds instead of every seconds)
- Battery monitoring and consumption estimation
- Modern, responsive dashboard design
- Interactive graphs for data visualization
- Historical data logging and analysis
- Visual alerts for distance thresholds

## License

This project is open source and available under the MIT License.
