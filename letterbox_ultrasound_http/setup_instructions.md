# Letterbox Monitoring System Setup Guide

This guide explains how to set up a letterbox monitoring system using an ESP32 and a Raspberry Pi. The ESP32 uses an ultrasonic sensor to detect mail and sends data to a Raspberry Pi webserver, which can send push notifications through the Techulus Push service.

## System Overview

- **ESP32**: Monitors the letterbox using an ultrasonic sensor and sends data to the Raspberry Pi
- **Raspberry Pi**: Hosts a web server that displays letterbox status and sends notifications
- **Techulus Push**: Service used to send notifications to your devices

## Part 1: Raspberry Pi Setup

### 1. Install Required Packages

SSH into your Raspberry Pi and install the required packages:

```bash
sudo apt update
sudo apt upgrade
sudo apt install python3-pip python3-flask
pip3 install requests
```

### 2. Create Project Directory

```bash
mkdir -p ~/letterbox_monitor/templates
cd ~/letterbox_monitor
```

### 3. Set Up the Server Application

Create the server application file:

```bash
nano app.py
```

Copy and paste the code from the "Raspberry Pi Server - Flask Application" file into this file and save (Ctrl+O, Enter, Ctrl+X).

### 4. Make the Application Start on Boot

Create a systemd service file:

```bash
sudo nano /etc/systemd/system/letterbox.service
```

Paste the following content:

```
[Unit]
Description=Letterbox Monitoring System
After=network.target

[Service]
User=pi
WorkingDirectory=/home/pi/letterbox_monitor
ExecStart=/usr/bin/python3 app.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Enable and start the service:

```bash
sudo systemctl enable letterbox.service
sudo systemctl start letterbox.service
```

### 5. Configure Techulus Push API Key

Edit the app.py file to replace the placeholder API key with your actual Techulus Push API key:

```bash
nano app.py
```

Find the line:
```python
TECHULUS_API_KEY = "YOUR_API_KEY"
```

Replace "YOUR_API_KEY" with your actual Techulus Push API key, then save and exit.

Restart the service to apply the changes:

```bash
sudo systemctl restart letterbox.service
```

## Part 2: ESP32 Setup

### 1. Prepare the Arduino IDE

1. Install the Arduino IDE on your computer if you haven't already
2. Add ESP32 board support:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add `https://dl.espressif.com/dl/package_esp32_index.json` to the "Additional Boards Manager URLs" field
   - Go to Tools > Board > Boards Manager
   - Search for ESP32 and install the ESP32 board package

### 2. Connect Ultrasonic Sensor to ESP32

Connect the HC-SR04 ultrasonic sensor to your ESP32:
- VCC to 5V
- GND to GND
- TRIG to GPIO 2 (D2)
- ECHO to GPIO 4 (D4)

### 3. Upload Code to ESP32

1. Open the Arduino IDE
2. Create a new sketch
3. Copy and paste the code from the "Modified ESP32 Code - WiFi Client" file
4. Update the configuration settings:
   - Replace "YourWiFiSSID" with your actual WiFi network name
   - Replace "YourWiFiPassword" with your actual WiFi password
   - Replace "192.168.1.100" with your Raspberry Pi's IP address
5. Select the correct board and port from the Tools menu
6. Click the Upload button

## Part 3: Testing the System

1. Power on both the Raspberry Pi and ESP32
2. Open a web browser and navigate to http://[Your_Raspberry_Pi_IP] to view the letterbox monitoring dashboard
3. Calibrate the letterbox by making sure it's empty (the ESP32 will automatically calibrate on first boot)
4. Test by placing an item in the letterbox and seeing if it detects it
5. Verify that you receive push notifications on your devices through the Techulus Push service

## Troubleshooting

### ESP32 Cannot Connect to WiFi
- Double-check the WiFi SSID and password
- Ensure your WiFi router is operating on a frequency compatible with ESP32 (2.4GHz)
- If still not working, the ESP32 will fall back to AP mode, allowing you to connect to it directly

### Raspberry Pi Server Not Working
- Check if the service is running: `sudo systemctl status letterbox.service`
- Check logs for errors: `journalctl -u letterbox.service`
- Ensure port 80 is not being used by another service

### No Push Notifications
- Verify your Techulus API key is correct
- Check the Raspberry Pi logs for API request errors
- Make sure your devices are registered with Techulus Push service

## Customization

You can customize the system by:
- Modifying the HTML template for a different look and feel
- Adjusting the thresholds in the ESP32 code for more accurate mail detection
- Adding additional features to the web interface
- Setting up email notifications in addition to push notifications