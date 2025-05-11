#include <WiFi.h>
#include <HTTPClient.h>

// WiFi Connection settings
const char* ssid = "YourWiFiSSID";         // Replace with your WiFi network name
const char* password = "YourWiFiPassword";  // Replace with your WiFi password
const char* raspberryPiIP = "192.168.1.100"; // Replace with your Raspberry Pi's IP address
const int raspberryPiPort = 80;              // Default HTTP port

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

void setup() {
  Serial.begin(115200);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.println("Connecting to WiFi...");
  
  // Wait for connection with timeout
  int connectionAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }
  
  // If WiFi connection fails, start Access Point mode
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed. Starting Access Point mode.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.println("Access Point started");
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP().toString());
    inAPMode = true;
  } else {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP().toString());
    inAPMode = false;
  }
  
  // Take initial measurement as baseline
  calibrateBaseline();
}

void loop() {
  // Monitor WiFi connection if in station mode
  if (!inAPMode && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    WiFi.reconnect();
    delay(5000); // Wait 5 seconds before checking connection again
  }

  // Measure distance with ultrasonic sensor
  measureDistance();
  
  // Small delay
  delay(500);
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
    
    // Send data to Raspberry Pi
    sendDataToRaspberryPi();
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
        
        // Send data to Raspberry Pi with notification flag
        sendDataToRaspberryPi(true, "Additional mail detected!");
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
          
          // Send data to Raspberry Pi with notification flag
          sendDataToRaspberryPi(true, "New mail has arrived!");
        } else {
          // Mail has been removed
          sendDataToRaspberryPi(true, "Mail has been removed!");
        }
        
        mailPresent = newMailState;
        previousMailState = newMailState;
      } else {
        // Regular update without notification
        sendDataToRaspberryPi();
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

void sendDataToRaspberryPi(bool sendNotification = false, String notificationMessage = "") {
  // Only send data if connected to WiFi in station mode
  if (inAPMode || WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi in station mode. Cannot send data to Raspberry Pi.");
    return;
  }
  
  HTTPClient http;
  
  // Prepare URL with endpoint
  String url = "http://" + String(raspberryPiIP) + ":" + String(raspberryPiPort) + "/letterbox/data";
  
  // Prepare JSON payload
  String jsonPayload = "{\"mailPresent\":" + String(mailPresent ? "true" : "false") + 
                      ",\"lastMailTime\":\"" + lastMailTime + 
                      "\",\"distance\":" + String(distance) + 
                      ",\"baseline\":" + String(baselineDistance) + 
                      ",\"lastChange\":\"" + lastDistanceChange + 
                      "\",\"mailChangeCount\":" + String(mailChangeCount) +
                      ",\"mailCount\":" + String(mailCount);
  
  // Add notification info if applicable
  if (sendNotification) {
    jsonPayload += ",\"sendNotification\":true";
    jsonPayload += ",\"notificationMessage\":\"" + notificationMessage + "\"";
  } else {
    jsonPayload += ",\"sendNotification\":false";
  }
  
  jsonPayload += "}";
  
  Serial.print("Sending data to Raspberry Pi: ");
  Serial.println(url);
  Serial.println(jsonPayload);
  
  // Begin HTTP request
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Send POST request
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }
  
  // Free resources
  http.end();
}