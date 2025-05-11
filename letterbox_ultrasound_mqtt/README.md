# Smart Letterbox MQTT Setup

This README provides instructions on how to set up the MQTT broker and make it accessible from the ESP32 on your local network.

## Prerequisites

- Raspberry Pi with Raspbian/Raspberry Pi OS
- ESP32 with Arduino IDE
- Both devices connected to the same local network

## Setting Up the MQTT Broker on Raspberry Pi

1. **Install the Mosquitto MQTT broker**:
   ```bash
   sudo apt update
   sudo apt install -y mosquitto mosquitto-clients
   ```

2. **Configure Mosquitto to accept connections from other devices**:
   Copy the provided `mosquitto.conf` file to the Mosquitto configuration directory:
   ```bash
   sudo cp mosquitto.conf /etc/mosquitto/mosquitto.conf
   ```

3. **Restart Mosquitto to apply the changes**:
   ```bash
   sudo systemctl restart mosquitto
   ```

4. **Enable Mosquitto to start on boot**:
   ```bash
   sudo systemctl enable mosquitto
   ```

5. **Check Mosquitto status**:
   ```bash
   sudo systemctl status mosquitto
   ```

6. **Find your Raspberry Pi's IP address**:
   ```bash
   hostname -I
   ```
   Note down the IP address (e.g., 192.168.1.100)

## Configuring the ESP32

1. **Update the ESP32 code with your Raspberry Pi's IP address**:
   Open `letterbox_ultrasound_mqtt.ino` and update the following line with your Raspberry Pi's IP address:
   ```cpp
   const char* mqtt_server = "192.168.1.100";  // Replace with your Raspberry Pi's IP address
   ```

2. **Update WiFi credentials**:
   ```cpp
   const char* ssid = "Your_WiFi_SSID";
   const char* password = "Your_WiFi_Password";
   ```

3. **Upload the code to your ESP32** using the Arduino IDE

## Running the System

1. **Start the Flask server on the Raspberry Pi**:
   ```bash
   cd /path/to/Smart-Letterbox/letterbox_ultrasound_mqtt
   sudo python3 raspberry_pi_mqtt_server.py
   ```

2. **Monitor MQTT messages** (optional, for debugging):
   ```bash
   mosquitto_sub -t "letterbox/#" -v
   ```

3. **Access the web interface**:
   Open a web browser and navigate to `http://<raspberry-pi-ip>/`

## Improved WiFi Connection Reliability

The ESP32 code has been enhanced with robust WiFi connection handling:

1. **Improved Initial Connection**:
   - Increased connection timeout
   - Multiple reconnection attempts with progressive backoff
   - Detailed connection status logging

2. **Automatic Fallback to Access Point Mode**:
   - If WiFi connection fails after multiple attempts, the ESP32 automatically switches to Access Point mode
   - Creates a unique AP name based on the device's MAC address
   - Allows configuration and monitoring even without a WiFi network

3. **Smart Reconnection Logic**:
   - Monitors WiFi connection status continuously
   - Implements intelligent reconnection with limited attempts
   - Switches to AP mode if reconnection fails repeatedly
   - Prevents battery drain from endless reconnection attempts

## Troubleshooting

1. **Connection refused error**:
   - Make sure Mosquitto is running: `sudo systemctl status mosquitto`
   - Check if Mosquitto is listening on all interfaces: `netstat -tuln | grep 1883`
   - Verify firewall settings: `sudo ufw status` (ensure port 1883 is open)

2. **ESP32 not connecting to MQTT**:
   - Verify the ESP32 is connected to WiFi (check serial monitor output)
   - Double-check the Raspberry Pi IP address in the ESP32 code
   - Ensure both devices are on the same network
   - If in AP mode, connect to the ESP32's access point (named "LetterboxAP-XXXX")

3. **Data not updating on the web interface**:
   - Check the Flask server logs for errors
   - Verify MQTT messages are being received using `mosquitto_sub -t "letterbox/#" -v`
   - Check if the ESP32 is in AP mode (it won't connect to MQTT in this mode)
