# Smart Letterbox MQTT Setup Guide

This guide explains how to set up the Smart Letterbox system using MQTT communication between the ESP32 and Raspberry Pi.

## Overview of Changes

The system has been recoded to use MQTT instead of HTTP:

1. **ESP32 (letterbox_ultrasound_mqtt.ino)**:
   - Now uses PubSubClient library for MQTT communication
   - Publishes data to MQTT topics instead of making HTTP requests
   - Maintains the same core functionality for mail detection

2. **Raspberry Pi (raspberry_pi_mqtt_server.py)**:
   - Subscribes to MQTT topics to receive data from ESP32
   - Maintains the same web interface for monitoring
   - Still sends notifications using the Techulus Push API

## Prerequisites

### For ESP32:
- Arduino IDE
- ESP32 board support
- PubSubClient library (can be installed via Arduino Library Manager)

### For Raspberry Pi:
- Python 3
- Flask (`pip install flask`) (`sudo apt install python3-flask`)
- Paho MQTT client (`pip install paho-mqtt`)(`sudo apt install python3-paho-mqtt`)
- Mosquitto MQTT broker

## Setting Up the MQTT Broker on Raspberry Pi

1. Install the Mosquitto MQTT broker:
   ```
   sudo apt update
   sudo apt install -y mosquitto mosquitto-clients
   ```

2. Enable Mosquitto to start on boot:
   ```
   sudo systemctl enable mosquitto.service
   ```

3. Configure Mosquitto for your needs (optional):
   Create or edit the configuration file:
   ```
   sudo nano /etc/mosquitto/mosquitto.conf
   ```
   
   Add these lines for a basic setup:
   ```
   listener 1883
   allow_anonymous true
   ```
   
   For a more secure setup, you can configure username/password authentication.

4. Restart Mosquitto:
   ```
   sudo systemctl restart mosquitto
   ```

## Configuration

### ESP32 Configuration (letterbox_ultrasound_mqtt.ino):

1. Update the WiFi settings:
   ```cpp
   const char* ssid = "YourWiFiSSID";         // Replace with your WiFi network name
   const char* password = "YourWiFiPassword";  // Replace with your WiFi password
   ```

2. Update the MQTT settings:
   ```cpp
   const char* mqtt_server = "0.0.0.0";  // Replace with your Raspberry Pi's IP address
   const int mqtt_port = 1883;                 // Default MQTT port
   ```

### Raspberry Pi Configuration (raspberry_pi_mqtt_server.py):

1. Update the Techulus API key if you're using push notifications:
   ```python
   TECHULUS_API_KEY = "YOUR_API_KEY"  # Replace with your actual Techulus Push API key
   ```

2. Update the MQTT broker settings if needed:
   ```python
   MQTT_BROKER = "localhost"  # Use localhost if MQTT broker is on the same Raspberry Pi
   MQTT_PORT = 1883
   ```

## Running the System

1. **On the Raspberry Pi**:
   - Make sure the Mosquitto MQTT broker is running
   - Start the Flask server:
     ```
     sudo python3 raspberry_pi_mqtt_server.py
     ```
   - The web interface will be available at the Raspberry Pi's IP address
   - Alternatively, set up the Flask app as a service (see "Setting Up the Flask App as a Service" section below)

2. **On the ESP32**:
   - Upload the `letterbox_ultrasound_mqtt.ino` sketch using Arduino IDE
   - Monitor the serial output to verify connection to WiFi and MQTT broker

## Setting Up the Flask App as a Service

To ensure the Flask app runs automatically on boot and restarts if it crashes, you can set it up as a systemd service:

1. Create a systemd service file:
   ```
   sudo nano /etc/systemd/system/letterbox.service
   ```

2. Add the following content to the file (adjust paths and username as needed):
   ```
   [Unit]
   Description=Smart Letterbox Flask Server
   After=network.target mosquitto.service
   Wants=mosquitto.service

   [Service]
   User=wasabi
   WorkingDirectory=/path/to/your/letterbox_ultrasound_mqtt
   ExecStart=/usr/bin/python3 /path/to/your/letterbox_ultrasound_mqtt/app.py
   Restart=always
   RestartSec=10
   StandardOutput=journal
   StandardError=journal
   SyslogIdentifier=letterbox

   [Install]
   WantedBy=multi-user.target
   ```
   

3. Reload systemd to recognize the new service:
   ```
   sudo systemctl daemon-reload
   ```

4. Enable the service to start on boot:
   ```
   sudo systemctl enable letterbox.service
   ```

5. Start the service:
   ```
   sudo systemctl start letterbox.service
   ```

6. Check the service status:
   ```
   sudo systemctl status letterbox.service
   ```

7. View logs if needed:
   ```
   sudo journalctl -u letterbox.service
   ```

8. To stop or restart the service:
   ```
   sudo systemctl stop letterbox.service
   sudo systemctl restart letterbox.service
   ```

## Testing the System

1. **Verify MQTT Communication**:
   - On the Raspberry Pi, you can monitor MQTT messages:
     ```
     mosquitto_sub -t "letterbox/#" -v
     ```
   - This will show all messages published to the letterbox topics

2. **Test the Web Interface**:
   - Open a web browser and navigate to `http://<raspberry-pi-ip>/`
   - The dashboard should display the current letterbox status

## Troubleshooting

1. **ESP32 not connecting to MQTT**:
   - Check that the Raspberry Pi IP address is correct
   - Verify the MQTT broker is running: `sudo systemctl status mosquitto`
   - Check firewall settings: `sudo ufw status` (ensure port 1883 is open)

2. **Data not updating on the web interface**:
   - Check the Flask server logs for errors
   - Verify MQTT messages are being received using `mosquitto_sub`
   - Check the browser console for JavaScript errors

3. **Notifications not working**:
   - Verify your Techulus API key is correct
   - Check the Flask server logs for API response errors
