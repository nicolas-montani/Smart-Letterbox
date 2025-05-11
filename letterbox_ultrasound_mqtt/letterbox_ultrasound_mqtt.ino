#include <WiFi.h>
#include <PubSubClient.h>  // MQTT client library

// WiFi Connection settings
const char* ssid = "Self_Destruction_Device";         // Replace with your WiFi network name
const char* password = "123456789";  // Replace with your WiFi password

// MQTT Settings
const char* mqtt_server = "172.20.10.6";  // Replace with your Raspberry Pi's IP address
const int mqtt_port = 1883;                 // Default MQTT port
const char* mqtt_client_id = "letterbox_sensor";
const char* mqtt_topic = "letterbox/data";
const char* mqtt_notification_topic = "letterbox/notification";

// Access Point settings (fallback if WiFi connection fails)
const char* apSSID = "LetterboxAP";
const char* apPassword = "letterbox123";

// Ultrasonic sensor pins
#define TRIG_PIN 2  // D2 on ESP32
#define ECHO_PIN 4  // D4 on ESP32

// Variables
int distance = 0;          // Current measured distance
int baselineDistance = 0;  // Empty letterbox distance
long duration = 0;
bool mailPresent = false;
bool previousMailState = false;
String lastMailTime = "Never";
String lastDistanceChange = "Never";
int distanceThreshold = 30; // mm threshold for detecting mail
int minChangeThreshold = 10; // mm threshold for logging a distance change
int mailChangeCount = 0;    // Counter for changes in mail status
int mailCount = 0;          // Counter specifically for mail deliveries
int lastSignificantDistance = 0; // To track last significant distance for detecting additional mail

// Connection mode
bool inAPMode = false;

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Last publish time
unsigned long lastPublishTime = 0;
const long publishInterval = 10000; // Publish data every 10 seconds

void setup() {
  Serial.begin(115200);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Connect to WiFi with improved reliability
  connectToWiFi();
  
  // Setup MQTT connection if in station mode
  if (!inAPMode) {
    client.setServer(mqtt_server, mqtt_port);
  }
  
  // Take initial measurement as baseline
  calibrateBaseline();
}

// Variables for WiFi reconnection
unsigned long lastWiFiReconnectAttempt = 0;
const long wifiReconnectInterval = 30000; // 30 seconds between reconnection attempts
int wifiReconnectAttempts = 0;
const int maxWifiReconnectAttempts = 5; // Maximum number of reconnection attempts before switching to AP mode

void loop() {
  unsigned long currentMillis = millis();
  
  // Monitor WiFi connection if in station mode
  if (!inAPMode && WiFi.status() != WL_CONNECTED) {
    // Only attempt reconnection at specified intervals
    if (currentMillis - lastWiFiReconnectAttempt > wifiReconnectInterval) {
      lastWiFiReconnectAttempt = currentMillis;
      wifiReconnectAttempts++;
      
      Serial.print("WiFi connection lost. Reconnection attempt ");
      Serial.print(wifiReconnectAttempts);
      Serial.print(" of ");
      Serial.println(maxWifiReconnectAttempts);
      
      // Try to reconnect
      WiFi.disconnect(true);
      delay(1000);
      WiFi.begin(ssid, password);
      
      // Wait a bit to see if connection is successful
      int quickCheckAttempts = 0;
      while (WiFi.status() != WL_CONNECTED && quickCheckAttempts < 10) {
        delay(500);
        Serial.print(".");
        quickCheckAttempts++;
      }
      
      // If reconnection successful, reset counter
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi reconnected successfully!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP().toString());
        wifiReconnectAttempts = 0;
      }
      // If max attempts reached, switch to AP mode
      else if (wifiReconnectAttempts >= maxWifiReconnectAttempts) {
        Serial.println("\nFailed to reconnect to WiFi after multiple attempts.");
        Serial.println("Switching to Access Point mode...");
        // Switch to AP mode
        connectToWiFi(); // This will set up AP mode if WiFi connection fails
      }
    }
  }

  // Handle MQTT connection if in station mode and connected to WiFi
  if (!inAPMode && WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
  }

  // Measure distance with ultrasonic sensor
  measureDistance();
  
  // Publish data periodically
  // Note: currentMillis is already declared at the beginning of loop()
  if (currentMillis - lastPublishTime > publishInterval) {
    lastPublishTime = currentMillis;
    if (!inAPMode && client.connected()) {
      publishData();
    }
  }
  
  // Small delay
  delay(500);
}

void connectToWiFi() {
  // Reset WiFi connection
  WiFi.disconnect(true);
  delay(1000);
  
  // Set WiFi mode to station
  WiFi.mode(WIFI_STA);
  
  // Print WiFi MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Begin WiFi connection
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  // Wait for connection with improved timeout
  int connectionAttempts = 0;
  const int maxAttempts = 30; // Increased from 20 to 30 attempts
  
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxAttempts) {
    delay(1000); // Increased from 500ms to 1000ms
    Serial.print(".");
    connectionAttempts++;
    
    // Every 10 attempts, try reconnecting
    if (connectionAttempts % 10 == 0) {
      Serial.println("\nStill trying to connect...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
    }
  }
  
  // If WiFi connection fails, start Access Point mode
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed after multiple attempts.");
    Serial.println("Starting Access Point mode as fallback.");
    
    // Disconnect from station mode
    WiFi.disconnect(true);
    delay(1000);
    
    // Set up Access Point
    WiFi.mode(WIFI_AP);
    
    // Create access point with a unique name based on MAC address
    String mac = WiFi.macAddress();
    String uniqueApName = String(apSSID) + "-" + mac.substring(9);
    
    // Start the access point
    if (WiFi.softAP(uniqueApName.c_str(), apPassword)) {
      Serial.println("Access Point started successfully");
      Serial.print("AP Name: ");
      Serial.println(uniqueApName);
      Serial.print("IP address: ");
      Serial.println(WiFi.softAPIP().toString());
      inAPMode = true;
    } else {
      Serial.println("Failed to start Access Point. Restarting ESP32...");
      ESP.restart();
    }
  } else {
    // WiFi connected successfully
    Serial.println("\nConnected to WiFi successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP().toString());
    inAPMode = false;
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
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      attempts++;
    }
  }
}

void calibrateBaseline() {
  // Take multiple measurements and average them for accuracy
  int totalDistance = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 10; i++) { // More readings for better accuracy
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long tempDuration = pulseIn(ECHO_PIN, HIGH, 23529);
    if (tempDuration > 0) {
      int tempDistance = tempDuration * 0.34;
      if (tempDistance > 0 && tempDistance < 4000) {
        totalDistance += tempDistance;
        validReadings++;
      }
    }
    delay(100);
  }
  
  if (validReadings > 0) {
    baselineDistance = totalDistance / validReadings;
    Serial.print("Baseline distance calibrated: ");
    Serial.print(baselineDistance);
    Serial.println(" mm");
    
    // Set current distance to match baseline
    distance = baselineDistance;
    lastSignificantDistance = baselineDistance;
    
    // Reset mail status
    mailPresent = false;
    previousMailState = false;
    
    // Reset mail change count when calibrating
    mailChangeCount = 0;
    mailCount = 0;
    
    // Get current time
    unsigned long currentMillis = millis();
    int seconds = (currentMillis / 1000) % 60;
    int minutes = (currentMillis / 60000) % 60;
    int hours = (currentMillis / 3600000) % 24;
    
    String currentTime = String(hours) + ":" + 
                        (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                        (seconds < 10 ? "0" : "") + String(seconds);
    
    lastDistanceChange = currentTime;
    
    // Send data to Raspberry Pi via MQTT
    publishData();
  } else {
    Serial.println("Calibration failed - no valid readings");
  }
}

void measureDistance() {
  // Take multiple measurements and average for stability
  int totalDistance = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 3; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long tempDuration = pulseIn(ECHO_PIN, HIGH, 23529);
    if (tempDuration > 0) {
      int tempDistance = tempDuration * 0.34;
      if (tempDistance > 0 && tempDistance < 4000) {
        totalDistance += tempDistance;
        validReadings++;
      }
    }
    delay(10);
  }
  
  // Skip if no valid readings
  if (validReadings == 0) {
    return;
  }
  
  // Calculate average distance
  int newDistance = totalDistance / validReadings;
  
  // Check if distance has changed significantly
  if (abs(newDistance - distance) > minChangeThreshold) {
    // Distance has changed
    distance = newDistance;
    
    // Update time of last distance change
    unsigned long currentMillis = millis();
    int seconds = (currentMillis / 1000) % 60;
    int minutes = (currentMillis / 60000) % 60;
    int hours = (currentMillis / 3600000) % 24;
    
    lastDistanceChange = String(hours) + ":" + 
                        (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
                        (seconds < 10 ? "0" : "") + String(seconds);
    
    // If we have a baseline measurement, determine mail presence
    if (baselineDistance > 0) {
      // If distance is significantly less than baseline, mail is present
      bool newMailState = (distance < baselineDistance - distanceThreshold);
      
      // Check for additional mail - if already mail present and distance decreases significantly more
      if (mailPresent && newMailState && (distance < lastSignificantDistance - distanceThreshold)) {
        // Additional mail detected
        mailCount++;
        lastSignificantDistance = distance;
        lastMailTime = lastDistanceChange;
        
        Serial.print("Additional mail detected! New count: ");
        Serial.println(mailCount);
        
        // Send notification via MQTT
        publishNotification("Additional mail detected!");
      }
      // If mail state has changed
      else if (newMailState != previousMailState) {
        Serial.print("Mail status changed to: ");
        Serial.println(newMailState ? "Present" : "Not Present");
        
        // Increment mail change counter regardless of direction of change
        mailChangeCount++;
        Serial.print("Mail status changes: ");
        Serial.println(mailChangeCount);
        
        // If new mail has arrived
        if (newMailState) {
          lastMailTime = lastDistanceChange;
          // Increment mail count when new mail is detected
          mailCount++;
          Serial.print("Mail count: ");
          Serial.println(mailCount);
          // Update last significant distance for future comparisons
          lastSignificantDistance = distance;
          
          // Send notification via MQTT
          publishNotification("New mail has arrived!");
        } else {
          // Mail has been removed
          publishNotification("Mail has been removed!");
        }
        
        mailPresent = newMailState;
        previousMailState = newMailState;
      }
    }
  }
  
  // Output debug info to serial
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" mm | Baseline: ");
  Serial.print(baselineDistance);
  Serial.print(" mm | Diff: ");
  Serial.print(baselineDistance - distance);
  Serial.print(" mm | Mail: ");
  Serial.println(mailPresent ? "Yes" : "No");
}

void publishData() {
  // Only send data if connected to WiFi in station mode and MQTT is connected
  if (inAPMode || !client.connected()) {
    Serial.println("Not connected to MQTT. Cannot send data.");
    return;
  }
  
  // Prepare JSON payload
  String jsonPayload = "{\"mailPresent\":" + String(mailPresent ? "true" : "false") + 
                      ",\"lastMailTime\":\"" + lastMailTime + 
                      "\",\"distance\":" + String(distance) + 
                      ",\"baseline\":" + String(baselineDistance) + 
                      ",\"lastChange\":\"" + lastDistanceChange + 
                      "\",\"mailChangeCount\":" + String(mailChangeCount) +
                      ",\"mailCount\":" + String(mailCount) + "}";
  
  Serial.print("Publishing data to MQTT topic: ");
  Serial.println(mqtt_topic);
  Serial.println(jsonPayload);
  
  // Publish to MQTT topic
  client.publish(mqtt_topic, jsonPayload.c_str());
}

void publishNotification(String message) {
  // Only send notification if connected to WiFi in station mode and MQTT is connected
  if (inAPMode || !client.connected()) {
    Serial.println("Not connected to MQTT. Cannot send notification.");
    return;
  }
  
  // Prepare JSON payload with notification
  String jsonPayload = "{\"message\":\"" + message + "\"}";
  
  Serial.print("Publishing notification to MQTT topic: ");
  Serial.println(mqtt_notification_topic);
  Serial.println(jsonPayload);
  
  // Publish to MQTT notification topic
  client.publish(mqtt_notification_topic, jsonPayload.c_str());
  
  // Also publish updated data
  publishData();
}
