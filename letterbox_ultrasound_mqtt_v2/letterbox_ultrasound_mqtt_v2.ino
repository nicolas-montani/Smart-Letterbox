#include <WiFi.h>
#include <PubSubClient.h>  // MQTT client library

// WiFi Connection settings
const char* ssid = "Self_Destruction_Device";  // Replace with your WiFi network name
const char* password = "123456789";            // Replace with your WiFi password

// MQTT Settings
const char* mqtt_server = "172.20.10.6";      // Replace with your Raspberry Pi's IP address
const int mqtt_port = 1883;                    // Default MQTT port
const char* mqtt_client_id = "letterbox_sensor";
const char* mqtt_topic = "letterbox/data";

// Ultrasonic sensor pins
#define TRIG_PIN 2  // D2 on ESP32
#define ECHO_PIN 4  // D4 on ESP32

// USB connected accumulator settings
#define USB_CONNECTED true  // Flag to indicate USB power source

// Variables
long durations[3] = {0, 0, 0};  // Array to store 3 measurements
int measurementCount = 0;        // Counter for measurements
unsigned long lastMeasurementTime = 0;  // Time of last measurement set
int batteryPercentage = 0;
const unsigned long MEASUREMENT_INTERVAL = 10000;  // 10 seconds between measurement sets

// Battery consumption estimation
const float BATTERY_CAPACITY_MAH = 10000.0;  // 10000mAh battery
const float ESP32_AVG_CURRENT_MA = 80.0;     // Average current draw in mA (adjust based on your setup)
unsigned long startTime = 0;                 // Time when the device started
float estimatedUsedCapacity = 0.0;           // Estimated used capacity in mAh
float estimatedRemainingTime = 0.0;          // Estimated remaining time in hours

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);



void setup() {
  Serial.begin(115200);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Record start time for battery consumption estimation
  startTime = millis();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup MQTT connection
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  // Reconnect to WiFi if connection is lost
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    connectToWiFi();
  }

  // Handle MQTT connection
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  Serial.println("Taking a set of 3 measurements...");
  
  // Reset measurement counter
  measurementCount = 0;
  
  // Take 3 measurements
  while (measurementCount < 3) {
    // Measure distance with ultrasonic sensor
    measureDistance();
    
    // Short delay between measurements
    delay(100);
    
    // Increment counter
    measurementCount++;
  }
  
  // Estimate accumulator status and remaining capacity
  measureBattery();
  
  // Publish data if connected
  if (client.connected()) {
    publishData();
  }
  
  Serial.println("Measurements complete. Waiting for 10 seconds...");
  
  // Wait 10 seconds before the next set of measurements
  delay(10000);
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  // Wait for connection with timeout
  int connectionAttempts = 0;
  const int maxAttempts = 20;
  
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed. Will retry in next loop.");
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  int attempts = 0;
  while (!client.connected() && attempts < 5) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
      attempts++;
    }
  }
}

void measureDistance() {
  // Clear the trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read the echo pin, returns the sound wave travel time in microseconds
  long currentDuration = pulseIn(ECHO_PIN, HIGH);
  
  // Store the duration in the array
  durations[measurementCount] = currentDuration;
  
  // Output debug info to serial
  Serial.print("Measurement #");
  Serial.print(measurementCount + 1);
  Serial.print(" - Duration: ");
  Serial.print(currentDuration);
  Serial.println(" microseconds");
}

void measureBattery() {
  // Calculate battery consumption based on runtime
  unsigned long runTimeMillis = millis() - startTime;
  float runTimeHours = runTimeMillis / 3600000.0;  // Convert milliseconds to hours
  
  // Estimate used capacity based on average current draw and run time
  estimatedUsedCapacity = ESP32_AVG_CURRENT_MA * runTimeHours;
  
  // Calculate remaining capacity
  float remainingCapacity = BATTERY_CAPACITY_MAH - estimatedUsedCapacity;
  if (remainingCapacity < 0) remainingCapacity = 0;
  
  // Calculate battery percentage
  batteryPercentage = (int)((remainingCapacity / BATTERY_CAPACITY_MAH) * 100.0);
  
  // Constrain the percentage between 0 and 100
  if (batteryPercentage > 100) batteryPercentage = 100;
  if (batteryPercentage < 0) batteryPercentage = 0;
  
  // Estimate remaining time
  if (batteryPercentage > 0) {
    estimatedRemainingTime = remainingCapacity / ESP32_AVG_CURRENT_MA;
  } else {
    estimatedRemainingTime = 0;
  }
  
  // Debug output
  Serial.print("Accumulator Status | Percentage: ");
  Serial.print(batteryPercentage);
  Serial.print("% | Run Time: ");
  Serial.print(runTimeHours);
  Serial.print("h | Est. Used: ");
  Serial.print(estimatedUsedCapacity);
  Serial.print("mAh | Est. Remaining: ");
  Serial.print(estimatedRemainingTime);
  Serial.println("h");
}

void publishData() {
  // Get current timestamp
  unsigned long currentMillis = millis();
  int seconds = (currentMillis / 1000) % 60;
  int minutes = (currentMillis / 60000) % 60;
  int hours = (currentMillis / 3600000) % 24;
  
  String timestamp = String(hours) + ":" + 
                    (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                    (seconds < 10 ? "0" : "") + String(seconds);
  
  // Calculate run time in hours
  float runTimeHours = (currentMillis - startTime) / 3600000.0;
  
  // Prepare JSON payload with all 3 measurements and accumulator information
  String jsonPayload = "{\"durations\":[" + 
                      String(durations[0]) + "," + 
                      String(durations[1]) + "," + 
                      String(durations[2]) + "]" +
                      ",\"batteryPercentage\":" + String(batteryPercentage) + 
                      ",\"batteryCapacity\":" + String(BATTERY_CAPACITY_MAH) + 
                      ",\"estimatedUsedCapacity\":" + String(estimatedUsedCapacity) + 
                      ",\"estimatedRemainingTime\":" + String(estimatedRemainingTime) + 
                      ",\"runTimeHours\":" + String(runTimeHours) +
                      ",\"powerSource\":\"USB Accumulator\"" +
                      ",\"timestamp\":\"" + timestamp + "\"}";
  
  Serial.print("Publishing data to MQTT topic: ");
  Serial.println(mqtt_topic);
  Serial.println(jsonPayload);
  
  // Publish to MQTT topic
  client.publish(mqtt_topic, jsonPayload.c_str());
}
