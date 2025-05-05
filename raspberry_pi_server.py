from flask import Flask, render_template, jsonify, request
import json
import time
import requests
import os
from datetime import datetime

app = Flask(__name__)

# Configuration
TECHULUS_API_KEY = "YOUR_API_KEY"  # Replace with your actual Techulus Push API key
NOTIFICATION_URL = "https://push.techulus.com/api/v1/notify"
DATA_FILE = "letterbox_data.json"

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

# Load data at startup
load_data()

# Routes
@app.route('/')
def index():
    return render_template('index.html', data=letterbox_data)

@app.route('/letterbox/data', methods=['POST'])
def update_data():
    global letterbox_data
    
    # Get current time
    current_time = datetime.now().strftime("%H:%M:%S")
    
    # Process incoming data
    incoming_data = request.json
    
    # Update our data storage with incoming data
    letterbox_data.update({
        "mailPresent": incoming_data.get("mailPresent", letterbox_data["mailPresent"]),
        "lastMailTime": incoming_data.get("lastMailTime", letterbox_data["lastMailTime"]),
        "distance": incoming_data.get("distance", letterbox_data["distance"]),
        "baseline": incoming_data.get("baseline", letterbox_data["baseline"]),
        "lastChange": incoming_data.get("lastChange", letterbox_data["lastChange"]),
        "mailChangeCount": incoming_data.get("mailChangeCount", letterbox_data["mailChangeCount"]),
        "mailCount": incoming_data.get("mailCount", letterbox_data["mailCount"]),
        "lastUpdateTime": current_time
    })
    
    # Check if notification should be sent
    if incoming_data.get("sendNotification", False):
        notification_message = incoming_data.get("notificationMessage", "Letterbox status has changed!")
        send_notification(notification_message)
    
    # Save updated data
    save_data()
    
    return jsonify({"success": True, "timestamp": current_time})

@app.route('/api/data')
def get_data():
    return jsonify(letterbox_data)

def send_notification(message):
    try:
        # Prepare notification payload
        payload = {
            "title": "Letterbox Alert",
            "body": message,
            "sound": "default",
            "channel": "letterbox",
            "link": f"http://{request.host}/",  # Link back to the letterbox dashboard
            "timeSensitive": True
        }
        
        # Set headers
        headers = {
            "x-api-key": TECHULUS_API_KEY,
            "Content-Type": "application/json",
            "Accept": "*/*"
        }
        
        # Send notification request
        response = requests.post(
            NOTIFICATION_URL,
            headers=headers,
            json=payload
        )
        
        print(f"Notification sent: {message}")
        print(f"Response: {response.status_code} - {response.text}")
        
        return True
    except Exception as e:
        print(f"Error sending notification: {e}")
        return False

if __name__ == '__main__':
    # Create templates directory if it doesn't exist
    os.makedirs('templates', exist_ok=True)
    
    # Create template file
    with open('templates/index.html', 'w') as f:
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
        </div>
        <div class='distance-info'>Current distance: <span id='distance'>0</span> mm</div>
        <div class='distance-info'>Baseline distance: <span id='baseline'>0</span> mm</div>
        <div class='distance-info'>Difference: <span id='difference' class='difference'>0</span> mm</div>
        <div class='distance-info'>Last distance change: <span id='lastChange'>Never</span></div>
        <div class='last-update'>Last update: <span id='lastUpdate'>Never</span></div>
    </div>
</body>
</html>""")
    
    # Run the server
    app.run(host='0.0.0.0', port=80, debug=True)