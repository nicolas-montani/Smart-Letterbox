# Smart Letterbox MQTT System v2 - Setup Instructions

This document provides instructions for setting up the Smart Letterbox MQTT system version 2, which consists of an ESP32 with an ultrasonic sensor and a Raspberry Pi server.

## Components

1. **ESP32 Microcontroller** with ultrasonic sensor
2. **Raspberry Pi** (any model with WiFi capability)
3. **Ultrasonic Sensor** (HC-SR04)
4. **10000mAh Battery Pack** for powering the ESP32

## ESP32 Setup

### Hardware Connections

1. Connect the ultrasonic sensor to the ESP32:
   - VCC to 5V
   - GND to GND
   - TRIG to GPIO 2
   - ECHO to GPIO 4

2. Connect a voltage divider to monitor battery voltage (optional):
   - Connect the battery voltage through a voltage divider to GPIO 34
   - Adjust the voltage divider values in the code if needed

3. Power the ESP32 using the 10000mAh battery pack via micro USB

### Software Setup

1. Install the Arduino IDE and ESP32 board support
2. Install the required libraries:
   - WiFi
   - PubSubClient (MQTT client)
3. Open the `letterbox_ultrasound_mqtt_v2.ino` file
4. Update the WiFi credentials and MQTT broker IP address:
   ```cpp
   const char* ssid = "Your_WiFi_SSID";
   const char* password = "Your_WiFi_Password";
   const char* mqtt_server = "Your_Raspberry_Pi_IP";
   ```
5. Upload the sketch to your ESP32

## Raspberry Pi Setup

### Software Requirements

1. Update your Raspberry Pi:
   ```bash
   sudo apt update
   sudo apt upgrade -y
   ```

2. Install the required packages:
   ```bash
   sudo apt install -y python3-pip mosquitto mosquitto-clients
   ```

3. Enable and start the Mosquitto MQTT broker:
   ```bash
   sudo systemctl enable mosquitto.service
   sudo systemctl start mosquitto.service
   ```

4. Install the required Python packages:
   ```bash
   pip3 install flask paho-mqtt
   ```

### Running the Server

1. Navigate to the project directory:
   ```bash
   cd letterbox_ultrasound_mqtt_v2
   ```

2. Run the server:
   ```bash
   sudo python3 raspberry_pi_mqtt_server_v2.py
   ```

   Note: `sudo` is required to run the server on port 80. If you want to run without sudo, change the port in the code to a value above 1024 (e.g., 8080).

3. Access the dashboard by opening a web browser and navigating to:
   ```
   http://[Raspberry_Pi_IP]
   ```

## System Features

### ESP32 Features
- Measures distance using ultrasonic sensor every second
- Monitors battery voltage and estimates consumption
- Sends all data to the MQTT broker

### Raspberry Pi Server Features
- Receives and logs all data from the ESP32
- Displays real-time data on a web dashboard
- Shows distance over time in a graph
- Highlights distances greater than 5mm in red
- Tracks battery consumption and estimates remaining time
- Stores historical data for analysis

## Troubleshooting

### ESP32 Issues
- If the ESP32 is not connecting to WiFi, check your credentials
- If the ultrasonic sensor readings are inconsistent, check the wiring
- If battery monitoring is inaccurate, adjust the voltage divider values in the code

### Raspberry Pi Issues
- If the MQTT broker is not running, restart it with:
  ```bash
  sudo systemctl restart mosquitto.service
  ```
- If the web server is not accessible, check your firewall settings:
  ```bash
  sudo ufw allow 80
  ```
- If data is not being received, check that the ESP32 and Raspberry Pi are on the same network

## Making the Server Start on Boot

To make the server start automatically when the Raspberry Pi boots:

1. Create a systemd service file:
   ```bash
   sudo nano /etc/systemd/system/letterbox-server.service
   ```

2. Add the following content:
   ```
   [Unit]
   Description=Letterbox MQTT Server
   After=network.target mosquitto.service

   [Service]
   ExecStart=/usr/bin/python3 /home/pi/letterbox_ultrasound_mqtt_v2/raspberry_pi_mqtt_server_v2.py
   WorkingDirectory=/home/pi/letterbox_ultrasound_mqtt_v2
   StandardOutput=inherit
   StandardError=inherit
   Restart=always
   User=root

   [Install]
   WantedBy=multi-user.target
   ```

3. Enable and start the service:
   ```bash
   sudo systemctl enable letterbox-server.service
   sudo systemctl start letterbox-server.service
   ```

4. Check the status:
   ```bash
   sudo systemctl status letterbox-server.service
