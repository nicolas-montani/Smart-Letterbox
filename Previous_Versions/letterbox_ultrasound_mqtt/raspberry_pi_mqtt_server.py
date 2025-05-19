from flask import Flask, render_template, jsonify, request
import json
import time
import requests
import os
import paho.mqtt.client as mqtt
from datetime import datetime
import threading
import pathlib

# Get the directory where this script is located
script_dir = pathlib.Path(__file__).parent.absolute()
# Create a templates directory in the same directory as the script
templates_dir = os.path.join(script_dir, 'templates')

# Initialize Flask app with the correct template folder
app = Flask(__name__, template_folder=templates_dir)

# Configuration
TECHULUS_API_KEY = "09339a89-5f96-4fee-a1de-acdbd8e54ca"  # Replace with your actual Techulus Push API key 8
NOTIFICATION_URL = "https://push.techulus.com/api/v1/notify"
DATA_FILE = os.path.join(script_dir, "letterbox_data.json")

# MQTT Configuration
MQTT_BROKER = "0.0.0.0"  # Bind to all network interfaces to be accessible from other devices
MQTT_PORT = 1883
MQTT_DATA_TOPIC = "letterbox/data"
MQTT_NOTIFICATION_TOPIC = "letterbox/notification"

# Initial data
letterbox_data = {
    "mailPresent": False,
    "lastMailTime": "Never",
    "distance": 0,
    "baseline": 0,
    "lastChange": "Never",
    "mailChangeCount": 0,
    "mailCount": 0,
    "lastUpdateTime": "Never"
}

# Load data from file if exists
def load_data():
    global letterbox_data
    try:
        if os.path.exists(DATA_FILE):
            with open(DATA_FILE, 'r') as f:
                letterbox_data = json.load(f)
            print("Data loaded from file")
    except Exception as e:
        print(f"Error loading data: {e}")

# Save data to file
def save_data():
    try:
        with open(DATA_FILE, 'w') as f:
            json.dump(letterbox_data, f)
        print("Data saved to file")
    except Exception as e:
        print(f"Error saving data: {e}")

# MQTT callbacks
def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"Connected to MQTT broker with result code {reason_code}")
    # Subscribe to topics
    client.subscribe(MQTT_DATA_TOPIC)
    client.subscribe(MQTT_NOTIFICATION_TOPIC)

def on_message(client, userdata, msg, properties=None):
    global letterbox_data
    
    # Get current time
    current_time = datetime.now().strftime("%H:%M:%S")
    
    try:
        # Decode and parse the JSON message
        payload = json.loads(msg.payload.decode())
        print(f"Received message on topic {msg.topic}: {payload}")
        
        if msg.topic == MQTT_DATA_TOPIC:
            # Update our data storage with incoming data
            letterbox_data.update({
                "mailPresent": payload.get("mailPresent", letterbox_data["mailPresent"]),
                "lastMailTime": payload.get("lastMailTime", letterbox_data["lastMailTime"]),
                "distance": payload.get("distance", letterbox_data["distance"]),
                "baseline": payload.get("baseline", letterbox_data["baseline"]),
                "lastChange": payload.get("lastChange", letterbox_data["lastChange"]),
                "mailChangeCount": payload.get("mailChangeCount", letterbox_data["mailChangeCount"]),
                "mailCount": payload.get("mailCount", letterbox_data["mailCount"]),
                "lastUpdateTime": current_time
            })
            
            # Save updated data
            save_data()
            
        elif msg.topic == MQTT_NOTIFICATION_TOPIC:
            # Process notification message
            notification_message = payload.get("message", "Letterbox status has changed!")
            # Send notification without host URL since we're outside of a request context
            send_notification(notification_message)
            
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON: {e}")
    except Exception as e:
        print(f"Error processing message: {e}")

# Setup MQTT client
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)  # Use API version 2
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

# Load data at startup
load_data()

# Routes
@app.route('/')
def index():
    return render_template('index.html', data=letterbox_data)

@app.route('/api/data')
def get_data():
    return jsonify(letterbox_data)

@app.route('/api/test-notification')
def test_notification():
    """Route to test the notification system"""
    try:
        # Send a test notification with the current host URL
        send_notification("This is a test notification from your Smart Letterbox", request.host)
        return jsonify({"success": True, "message": "Test notification sent"})
    except Exception as e:
        return jsonify({"success": False, "message": f"Error sending notification: {str(e)}"})

def send_notification(message, host_url=None):
    try:
        # Prepare notification payload
        payload = {
            "title": "Letterbox Alert",
            "body": message,
            "sound": "default",
            "channel": "letterbox",
            "timeSensitive": True
        }
        
        # Add link if host_url is provided
        if host_url:
            payload["link"] = f"http://{host_url}/"
        
        # Set headers
        headers = {
            "x-api-key": TECHULUS_API_KEY,
            "Content-Type": "application/json",
            "Accept": "*/*"
        }
        
        # Print debug info
        print(f"Sending notification with payload: {payload}")
        print(f"Using API key: {TECHULUS_API_KEY}")
        print(f"To URL: {NOTIFICATION_URL}")
        
        # Send notification request
        response = requests.post(
            NOTIFICATION_URL,
            headers=headers,
            json=payload
        )
        
        # Print detailed response
        print(f"Notification sent: {message}")
        print(f"Response status: {response.status_code}")
        print(f"Response headers: {response.headers}")
        print(f"Response body: {response.text}")
        
        # Check if the response indicates success
        if response.status_code >= 200 and response.status_code < 300:
            print("Notification sent successfully!")
            return True
        else:
            print(f"Notification failed with status code: {response.status_code}")
            return False
    except Exception as e:
        print(f"Error sending notification: {e}")
        print(f"Error type: {type(e).__name__}")
        import traceback
        print(f"Traceback: {traceback.format_exc()}")
        return False

def start_mqtt_client():
    global mqtt_client
    try:
        # Connect to MQTT broker
        # For the Raspberry Pi script, we should use "localhost" since the broker is on the same machine
        mqtt_client.connect("localhost", MQTT_PORT, 60)
        
        # Start the MQTT loop in a background thread
        mqtt_client.loop_start()
        print("MQTT client started")
        return True
    except Exception as e:
        print(f"Error starting MQTT client: {e}")
        print("MQTT functionality will be disabled. The web interface will still work.")
        print("Make sure the Mosquitto MQTT broker is installed and running:")
        print("  sudo apt update")
        print("  sudo apt install -y mosquitto mosquitto-clients")
        print("  sudo systemctl enable mosquitto.service")
        print("  sudo systemctl start mosquitto.service")
        return False

if __name__ == '__main__':
    # Create templates directory if it doesn't exist
    os.makedirs(templates_dir, exist_ok=True)
    
    # Create template file
    with open(os.path.join(templates_dir, 'index.html'), 'w') as f:
        f.write("""<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Smart Letterbox</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            text-align: center;
            background-color: #f0f0f0;
        }
        .container {
            max-width: 500px;
            margin: 0 auto;
            background-color: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
        }
        h1 {
            color: #333;
        }
        .status {
            font-size: 24px;
            margin: 20px 0;
            padding: 15px;
            border-radius: 5px;
        }
        .mail-present {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .no-mail {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .info {
            margin: 15px 0;
            font-size: 16px;
        }
        .mail-count {
            background-color: #e2f0fd;
            color: #0c5460;
            border: 1px solid #bee5eb;
            padding: 10px;
            border-radius: 5px;
            margin: 15px 0;
            font-size: 18px;
            font-weight: bold;
        }
        .btn {
            border: none;
            padding: 10px 20px;
            font-size: 16px;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
        }
        .refresh-btn {
            background-color: #007bff;
            color: white;
        }
        .refresh-btn:hover {
            background-color: #0069d9;
        }
        .distance-info {
            font-size: 14px;
            color: #666;
            margin-top: 10px;
        }
        .difference {
            font-weight: bold;
        }
        .last-update {
            font-size: 12px;
            color: #888;
            margin-top: 20px;
        }
    </style>
    <script>
        function updateStatus() {
            fetch('/api/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').className = data.mailPresent ? 'status mail-present' : 'status no-mail';
                    document.getElementById('status').innerText = data.mailPresent ? 'Mail is present!' : 'No mail';
                    document.getElementById('lastTime').innerText = data.lastMailTime;
                    document.getElementById('mailCount').innerText = data.mailCount;
                    document.getElementById('distance').innerText = data.distance;
                    document.getElementById('baseline').innerText = data.baseline;
                    document.getElementById('difference').innerText = data.baseline - data.distance;
                    document.getElementById('lastChange').innerText = data.lastChange;
                    document.getElementById('lastUpdate').innerText = data.lastUpdateTime;
                });
        }
        
        function testNotification() {
            // Show a loading indicator
            const testBtn = document.querySelector('.test-btn');
            const originalText = testBtn.innerText;
            testBtn.innerText = 'Sending...';
            testBtn.disabled = true;
            
            // Send request to test notification endpoint
            fetch('/api/test-notification')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('Test notification sent! Check your phone.');
                    } else {
                        alert('Error sending notification: ' + data.message);
                    }
                })
                .catch(error => {
                    alert('Error: ' + error.message);
                })
                .finally(() => {
                    // Restore button state
                    testBtn.innerText = originalText;
                    testBtn.disabled = false;
                });
        }
        
        // Update status every 5 seconds
        setInterval(updateStatus, 5000);
        
        // Initial update
        document.addEventListener('DOMContentLoaded', updateStatus);
    </script>
</head>
<body>
    <div class='container'>
        <h1>Smart Letterbox</h1>
        <div id='status' class='status no-mail'>
            No mail
        </div>
        <div class='info'>Last mail received at: <span id='lastTime'>Never</span></div>
        <div class='mail-count'>Total Mail Received: <span id='mailCount'>0</span></div>
        <div class='btn-container'>
            <button class='btn refresh-btn' onclick='updateStatus()'>Refresh</button>
            <button class='btn test-btn' onclick='testNotification()' style='background-color: #28a745; color: white;'>Test Notification</button>
        </div>
        <div class='distance-info'>Current distance: <span id='distance'>0</span> mm</div>
        <div class='distance-info'>Baseline distance: <span id='baseline'>0</span> mm</div>
        <div class='distance-info'>Difference: <span id='difference' class='difference'>0</span> mm</div>
        <div class='distance-info'>Last distance change: <span id='lastChange'>Never</span></div>
        <div class='last-update'>Last update: <span id='lastUpdate'>Never</span></div>
        
        <div style='margin-top: 30px; padding: 15px; background-color: #f8f9fa; border-radius: 5px; text-align: left;'>
            <h3 style='color: #333; margin-top: 0;'>Notification Setup</h3>
            <p>To receive notifications on your phone:</p>
            <ol style='padding-left: 20px;'>
                <li>Download the <a href='https://push.techulus.com/' target='_blank'>Techulus Push</a> app from the App Store or Google Play Store</li>
                <li>Sign in with your email and get your API key from the app</li>
                <li>Update the TECHULUS_API_KEY in the raspberry_pi_mqtt_server.py file</li>
                <li>Restart the server</li>
                <li>Click the "Test Notification" button above to verify it's working</li>
            </ol>
            <p>If you're not receiving notifications:</p>
            <ul style='padding-left: 20px;'>
                <li>Check that your API key is correct</li>
                <li>Ensure the Techulus Push app is installed and running on your phone</li>
                <li>Check that your phone has an internet connection</li>
                <li>Look at the server logs for any error messages</li>
            </ul>
        </div>
    </div>
</body>
</html>""")
    
    # Start MQTT client in a separate thread
    mqtt_thread = threading.Thread(target=start_mqtt_client)
    mqtt_thread.daemon = True
    mqtt_thread.start()
    
    # Run the Flask server
    app.run(host='0.0.0.0', port=80, debug=True)
