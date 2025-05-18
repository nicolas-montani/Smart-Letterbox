from flask import Flask, render_template, jsonify, request
import json
import time
import os
import paho.mqtt.client as mqtt
from datetime import datetime
import threading
import pathlib
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("letterbox_server.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("letterbox_server")

# Get the directory where this script is located
script_dir = pathlib.Path(__file__).parent.absolute()
# Create a templates directory in the same directory as the script
templates_dir = os.path.join(script_dir, 'templates')
# Create a static directory for CSS, JS, etc.
static_dir = os.path.join(script_dir, 'static')

# Initialize Flask app with the correct template and static folders
app = Flask(__name__, template_folder=templates_dir, static_folder=static_dir)

# Configuration
DATA_FILE = os.path.join(script_dir, "letterbox_data.json")
LOG_FILE = os.path.join(script_dir, "letterbox_history.json")

# MQTT Configuration
MQTT_BROKER = "localhost"  # Use localhost for the broker connection
MQTT_PORT = 1883
MQTT_DATA_TOPIC = "letterbox/data"
MQTT_NOTIFICATION_TOPIC = "NewLetter"  # Topic for letter notifications

# Initial data
letterbox_data = {
    "durations": [0, 0, 0],  # Array of 3 duration measurements
    "distances": [0, 0, 0],  # Array of 3 distance measurements (calculated from durations)
    "batteryPercentage": 0,
    "batteryCapacity": 10000,
    "estimatedUsedCapacity": 0,
    "estimatedRemainingTime": 0,
    "runTimeHours": 0,
    "powerSource": "USB Accumulator",
    "timestamp": "Never",
    "lastUpdateTime": "Never"
}

# Variable to store the previous average distance for letter detection
previous_avg_distance = 0
LETTER_DETECTION_THRESHOLD = 5  # 5mm threshold for letter detection

# Function to calculate distance in mm from duration in microseconds
def calculate_distance_mm(duration):
    # Speed of sound is 343.2 m/s or 0.3432 mm/microsecond
    # Distance = (duration * speed of sound) / 2 (for round trip)
    return (duration * 0.3432) / 2 if duration > 0 else 0

# History data for the graph
letterbox_history = []
MAX_HISTORY_ENTRIES = 1000  # Limit the number of entries to prevent the file from growing too large

# Load data from file if exists
def load_data():
    global letterbox_data, letterbox_history, previous_avg_distance
    try:
        if os.path.exists(DATA_FILE):
            with open(DATA_FILE, 'r') as f:
                letterbox_data = json.load(f)
            logger.info("Current data loaded from file")
            
            # Initialize previous_avg_distance from loaded data
            if "distances" in letterbox_data and isinstance(letterbox_data["distances"], list) and len(letterbox_data["distances"]) > 0:
                previous_avg_distance = sum(letterbox_data["distances"]) / len(letterbox_data["distances"])
        
        if os.path.exists(LOG_FILE):
            with open(LOG_FILE, 'r') as f:
                letterbox_history = json.load(f)
            logger.info(f"History data loaded from file ({len(letterbox_history)} entries)")
    except Exception as e:
        logger.error(f"Error loading data: {e}")

# Save data to file
def save_data():
    try:
        with open(DATA_FILE, 'w') as f:
            json.dump(letterbox_data, f)
        logger.info("Current data saved to file")
    except Exception as e:
        logger.error(f"Error saving current data: {e}")

# Save history data to file
def save_history():
    try:
        # Limit the size of the history
        global letterbox_history
        if len(letterbox_history) > MAX_HISTORY_ENTRIES:
            letterbox_history = letterbox_history[-MAX_HISTORY_ENTRIES:]
        
        with open(LOG_FILE, 'w') as f:
            json.dump(letterbox_history, f)
        logger.info(f"History data saved to file ({len(letterbox_history)} entries)")
    except Exception as e:
        logger.error(f"Error saving history data: {e}")

# Function to check for letter status changes
def check_letter_status(current_avg_distance):
    global previous_avg_distance, mqtt_client
    
    # Skip if this is the first measurement
    if previous_avg_distance == 0:
        previous_avg_distance = current_avg_distance
        return
    
    # Calculate the difference between current and previous average distance
    distance_diff = current_avg_distance - previous_avg_distance
    
    # Check if the difference exceeds the threshold
    if abs(distance_diff) > LETTER_DETECTION_THRESHOLD:
        message = ""
        if distance_diff < 0:  # Distance decreased (something added to letterbox)
            message = "New letter has arrived! Distance decreased by {:.2f}mm".format(abs(distance_diff))
            logger.info(message)
        else:  # Distance increased (something removed from letterbox)
            message = "Letter removed! Distance increased by {:.2f}mm".format(abs(distance_diff))
            logger.info(message)
        
        # Publish notification to MQTT topic
        try:
            notification = {
                "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "message": message,
                "previous_distance": previous_avg_distance,
                "current_distance": current_avg_distance,
                "difference": distance_diff
            }
            mqtt_client.publish(MQTT_NOTIFICATION_TOPIC, json.dumps(notification))
            logger.info(f"Published notification to {MQTT_NOTIFICATION_TOPIC}")
        except Exception as e:
            logger.error(f"Error publishing notification: {e}")
    
    # Update previous average distance
    previous_avg_distance = current_avg_distance

# MQTT callbacks
def on_connect(client, userdata, flags, reason_code, properties=None):
    logger.info(f"Connected to MQTT broker with result code {reason_code}")
    # Subscribe to topics
    client.subscribe(MQTT_DATA_TOPIC)

def on_message(client, userdata, msg, properties=None):
    global letterbox_data, letterbox_history
    
    # Get current time
    current_time = datetime.now().strftime("%H:%M:%S")
    current_date = datetime.now().strftime("%Y-%m-%d")
    
    try:
        # Decode and parse the JSON message
        payload = json.loads(msg.payload.decode())
        logger.info(f"Received message on topic {msg.topic}: {payload}")
        
        if msg.topic == MQTT_DATA_TOPIC:
            # Get durations array from payload
            durations = payload.get("durations", letterbox_data["durations"])
            
            # Calculate distances in mm from durations
            distances = [calculate_distance_mm(duration) for duration in durations]
            
            # Calculate average distance
            avg_distance = sum(distances) / len(distances) if distances else 0
            
            # Check for letter status changes
            check_letter_status(avg_distance)
            
            # Update our data storage with incoming data
            letterbox_data.update({
                "durations": durations,
                "distances": distances,  # Calculated from durations
                "batteryPercentage": payload.get("batteryPercentage", letterbox_data["batteryPercentage"]),
                "batteryCapacity": payload.get("batteryCapacity", letterbox_data["batteryCapacity"]),
                "estimatedUsedCapacity": payload.get("estimatedUsedCapacity", letterbox_data["estimatedUsedCapacity"]),
                "estimatedRemainingTime": payload.get("estimatedRemainingTime", letterbox_data["estimatedRemainingTime"]),
                "runTimeHours": payload.get("runTimeHours", letterbox_data["runTimeHours"]),
                "powerSource": payload.get("powerSource", letterbox_data["powerSource"]),
                "timestamp": payload.get("timestamp", letterbox_data["timestamp"]),
                "lastUpdateTime": current_time
            })
            
            # Add to history with timestamp for the graph
            history_entry = {
                "date": current_date,
                "time": current_time,
                "durations": durations,
                "distances": distances,
                "avg_distance": avg_distance,
                "batteryPercentage": payload.get("batteryPercentage", 0),
                "estimatedUsedCapacity": payload.get("estimatedUsedCapacity", 0)
            }
            letterbox_history.append(history_entry)
            
            # Save updated data
            save_data()
            
            # Save history periodically (every 10 entries to avoid excessive writes)
            if len(letterbox_history) % 10 == 0:
                save_history()
            
    except json.JSONDecodeError as e:
        logger.error(f"Error decoding JSON: {e}")
    except Exception as e:
        logger.error(f"Error processing message: {e}")

# MQTT error callback
def on_disconnect(client, userdata, rc, properties=None):
    if rc != 0:
        logger.error(f"Unexpected MQTT disconnection with code {rc}. Will attempt to reconnect.")
    else:
        logger.info("MQTT client disconnected successfully")

# Setup MQTT client
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)  # Use API version 2
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.on_disconnect = on_disconnect

# Load data at startup
load_data()

# Routes
@app.route('/')
def index():
    return render_template('index.html', data=letterbox_data)

@app.route('/api/data')
def get_data():
    return jsonify(letterbox_data)

@app.route('/api/history')
def get_history():
    # Get timeframe parameter from request, default to 'all'
    timeframe = request.args.get('timeframe', 'all')
    
    # Get current time for time-based filtering
    current_time = datetime.now()
    
    # Filter data based on timeframe
    filtered_data = []
    
    if timeframe == 'all':
        # Return all data (limited to last 1000 entries for performance)
        filtered_data = letterbox_history[-1000:] if len(letterbox_history) > 1000 else letterbox_history
    else:
        # Parse the timeframe to determine the time window
        try:
            if timeframe.endswith('h'):  # Hours
                hours = int(timeframe[:-1])
                cutoff_time = current_time - datetime.timedelta(hours=hours)
            elif timeframe.endswith('d'):  # Days
                days = int(timeframe[:-1])
                cutoff_time = current_time - datetime.timedelta(days=days)
            elif timeframe.endswith('w'):  # Weeks
                weeks = int(timeframe[:-1])
                cutoff_time = current_time - datetime.timedelta(weeks=weeks)
            elif timeframe.endswith('m'):  # Months (approximate)
                months = int(timeframe[:-1])
                cutoff_time = current_time - datetime.timedelta(days=months*30)
            else:
                # Default to last 100 entries if timeframe format is invalid
                return jsonify(letterbox_history[-100:] if len(letterbox_history) > 100 else letterbox_history)
            
            # Convert cutoff_time to string format for comparison
            cutoff_date_str = cutoff_time.strftime("%Y-%m-%d")
            cutoff_time_str = cutoff_time.strftime("%H:%M:%S")
            
            # Filter data based on date and time
            for entry in letterbox_history:
                entry_date = entry.get('date', '')
                entry_time = entry.get('time', '')
                
                # Compare dates first
                if entry_date > cutoff_date_str:
                    filtered_data.append(entry)
                elif entry_date == cutoff_date_str and entry_time >= cutoff_time_str:
                    filtered_data.append(entry)
        except (ValueError, AttributeError):
            # Return last 100 entries if there's an error parsing the timeframe
            filtered_data = letterbox_history[-100:] if len(letterbox_history) > 100 else letterbox_history
    
    return jsonify(filtered_data)

@app.route('/api/clear-history', methods=['POST'])
def clear_history():
    """Route to clear the history data"""
    global letterbox_history
    try:
        letterbox_history = []
        save_history()
        return jsonify({"success": True, "message": "History cleared successfully"})
    except Exception as e:
        return jsonify({"success": False, "message": f"Error clearing history: {str(e)}"})

def start_mqtt_client():
    global mqtt_client
    try:
        # Connect to MQTT broker
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        # Start the MQTT loop in a background thread
        mqtt_client.loop_start()
        logger.info(f"MQTT client started and connected to broker at {MQTT_BROKER}:{MQTT_PORT}")
        logger.info(f"Subscribed to topic: {MQTT_DATA_TOPIC}")
        return True
    except Exception as e:
        logger.error(f"Error starting MQTT client: {e}")
        logger.error("MQTT functionality will be disabled. The web interface will still work.")
        logger.error("Make sure the Mosquitto MQTT broker is installed and running:")
        logger.error("  sudo apt update")
        logger.error("  sudo apt install -y mosquitto mosquitto-clients")
        logger.error("  sudo systemctl enable mosquitto.service")
        logger.error("  sudo systemctl start mosquitto.service")
        return False

if __name__ == '__main__':
    # Create directories if they don't exist
    os.makedirs(templates_dir, exist_ok=True)
    os.makedirs(static_dir, exist_ok=True)
    
    # Create CSS file
    with open(os.path.join(static_dir, 'style.css'), 'w') as f:
        f.write("""
body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    margin: 0;
    padding: 0;
    background-color: #f5f5f5;
    color: #333;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

header {
    background-color: #2c3e50;
    color: white;
    padding: 1rem;
    text-align: center;
    margin-bottom: 2rem;
    border-radius: 5px;
}

h1, h2, h3 {
    margin: 0;
}

.dashboard {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 20px;
    margin-bottom: 2rem;
}

.card {
    background-color: white;
    border-radius: 8px;
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    padding: 20px;
    transition: transform 0.3s ease;
}

.card:hover {
    transform: translateY(-5px);
}

.card-title {
    font-size: 1.2rem;
    margin-bottom: 15px;
    color: #2c3e50;
    border-bottom: 2px solid #ecf0f1;
    padding-bottom: 10px;
}

.card-value {
    font-size: 2rem;
    font-weight: bold;
    margin-bottom: 10px;
    color: #3498db;
}

.card-value.warning {
    color: #e74c3c;
}

.card-subtitle {
    font-size: 0.9rem;
    color: #7f8c8d;
}

.graph-container {
    background-color: white;
    border-radius: 8px;
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    padding: 20px;
    margin-bottom: 2rem;
}

.btn {
    background-color: #3498db;
    color: white;
    border: none;
    padding: 10px 15px;
    border-radius: 5px;
    cursor: pointer;
    font-size: 1rem;
    transition: background-color 0.3s ease;
}

.btn:hover {
    background-color: #2980b9;
}

.btn-danger {
    background-color: #e74c3c;
}

.btn-danger:hover {
    background-color: #c0392b;
}

.btn-container {
    display: flex;
    justify-content: center;
    gap: 10px;
    margin-bottom: 2rem;
}

.timeframe-selector {
    display: flex;
    justify-content: center;
    gap: 10px;
    margin-bottom: 1rem;
}

.btn-timeframe {
    background-color: #95a5a6;
    color: white;
    border: none;
    padding: 5px 10px;
    border-radius: 5px;
    cursor: pointer;
    font-size: 0.9rem;
    transition: background-color 0.3s ease;
}

.btn-timeframe:hover {
    background-color: #7f8c8d;
}

.btn-timeframe.active {
    background-color: #3498db;
}

.btn-timeframe.active:hover {
    background-color: #2980b9;
}

.footer {
    text-align: center;
    margin-top: 2rem;
    padding: 1rem;
    background-color: #2c3e50;
    color: white;
    border-radius: 5px;
}

.battery-indicator {
    width: 100%;
    height: 20px;
    background-color: #ecf0f1;
    border-radius: 10px;
    margin-top: 10px;
    overflow: hidden;
}

.battery-level {
    height: 100%;
    background-color: #2ecc71;
    border-radius: 10px;
    transition: width 0.5s ease;
}

.battery-level.warning {
    background-color: #e74c3c;
}

.battery-level.low {
    background-color: #f39c12;
}

.last-update {
    text-align: right;
    font-size: 0.8rem;
    color: #7f8c8d;
    margin-top: 1rem;
}

@media (max-width: 768px) {
    .dashboard {
        grid-template-columns: 1fr;
    }
}
""")
    
    # Start MQTT client in a separate thread
    mqtt_thread = threading.Thread(target=start_mqtt_client)
    mqtt_thread.daemon = True
    mqtt_thread.start()
    
    # Run the Flask server
    app.run(host='0.0.0.0', port=80, debug=True)
