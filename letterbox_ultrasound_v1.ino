#include <WiFi.h>
#include <WebServer.h>

// WiFi Access Point settings
const char* apSSID = "LetterboxAP";
const char* apPassword = "letterbox123";

// Ultrasonic sensor pins
#define TRIG_PIN 2  // D2 on ESP32
#define ECHO_PIN 4  // D4 on ESP32

WebServer server(80);

// Variables
int distance = 0;          // Current measured distance
int baselineDistance = 0;  // Empty letterbox distance
long duration = 0;
bool mailPresent = false;
bool previousMailState = false;
String lastMailTime = "Never";
String lastDistanceChange = "Never";
int distanceThreshold = 30; // mm threshold for detecting mail (increased for more tolerance)
int minChangeThreshold = 10; // mm threshold for logging a distance change
int mailChangeCount = 0;    // Counter for changes in mail status
int mailCount = 0;          // Counter specifically for mail deliveries
int lastSignificantDistance = 0; // To track last significant distance for detecting additional mail

void setup() {
  Serial.begin(115200);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Set up WiFi Access Point
  WiFi.softAP(apSSID, apPassword);
  Serial.println("Access Point started");
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP().toString());
  
  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/refresh", handleRefresh);
  server.on("/calibrate", handleCalibrate);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  
  // Take initial measurement as baseline
  calibrateBaseline();
}

void loop() {
  // Handle web clients
  server.handleClient();
  
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

void handleRoot() {
  String html = "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>Smart Letterbox</title>"
                "<style>"
                "body {"
                "  font-family: Arial, sans-serif;"
                "  margin: 0;"
                "  padding: 20px;"
                "  text-align: center;"
                "  background-color: #f0f0f0;"
                "}"
                ".container {"
                "  max-width: 500px;"
                "  margin: 0 auto;"
                "  background-color: white;"
                "  padding: 20px;"
                "  border-radius: 10px;"
                "  box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);"
                "}"
                "h1 {"
                "  color: #333;"
                "}"
                ".status {"
                "  font-size: 24px;"
                "  margin: 20px 0;"
                "  padding: 15px;"
                "  border-radius: 5px;"
                "}"
                ".mail-present {"
                "  background-color: #d4edda;"
                "  color: #155724;"
                "  border: 1px solid #c3e6cb;"
                "}"
                ".no-mail {"
                "  background-color: #f8d7da;"
                "  color: #721c24;"
                "  border: 1px solid #f5c6cb;"
                "}"
                ".info {"
                "  margin: 15px 0;"
                "  font-size: 16px;"
                "}"
                ".mail-count {"
                "  background-color: #e2f0fd;"
                "  color: #0c5460;"
                "  border: 1px solid #bee5eb;"
                "  padding: 10px;"
                "  border-radius: 5px;"
                "  margin: 15px 0;"
                "  font-size: 18px;"
                "  font-weight: bold;"
                "}"
                ".btn {"
                "  border: none;"
                "  padding: 10px 20px;"
                "  font-size: 16px;"
                "  border-radius: 5px;"
                "  cursor: pointer;"
                "  margin: 5px;"
                "}"
                ".refresh-btn {"
                "  background-color: #007bff;"
                "  color: white;"
                "}"
                ".refresh-btn:hover {"
                "  background-color: #0069d9;"
                "}"
                ".calibrate-btn {"
                "  background-color: #28a745;"
                "  color: white;"
                "}"
                ".calibrate-btn:hover {"
                "  background-color: #218838;"
                "}"
                ".distance-info {"
                "  font-size: 14px;"
                "  color: #666;"
                "  margin-top: 10px;"
                "}"
                ".difference {"
                "  font-weight: bold;"
                "}"
                "</style>"
                "<script>"
                "function updateStatus() {"
                "  fetch('/data')"
                "    .then(response => response.json())"
                "    .then(data => {"
                "      document.getElementById('status').className = data.mailPresent ? 'status mail-present' : 'status no-mail';"
                "      document.getElementById('status').innerText = data.mailPresent ? 'Mail is present!' : 'No mail';"
                "      document.getElementById('lastTime').innerText = data.lastMailTime;"
                "      document.getElementById('mailCount').innerText = data.mailCount;"
                "      document.getElementById('distance').innerText = data.distance;"
                "      document.getElementById('baseline').innerText = data.baseline;"
                "      document.getElementById('difference').innerText = data.baseline - data.distance;"
                "      document.getElementById('lastChange').innerText = data.lastChange;"
                "    });"
                "}"
                "function refreshMeasurement() {"
                "  fetch('/refresh')"
                "    .then(response => response.json())"
                "    .then(data => {"
                "      updateStatus();"
                "    });"
                "}"
                "function calibrateBaseline() {"
                "  if(confirm('Make sure the letterbox is empty, then press OK to calibrate.')) {"
                "    fetch('/calibrate')"
                "      .then(response => response.json())"
                "      .then(data => {"
                "        updateStatus();"
                "        alert('Calibration complete! New baseline distance: ' + data.baseline + ' mm');"
                "      });"
                "  }"
                "}"
                "// Update status every 5 seconds"
                "setInterval(updateStatus, 5000);"
                "// Initial update"
                "document.addEventListener('DOMContentLoaded', updateStatus);"
                "</script>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>Smart Letterbox</h1>"
                "<div id='status' class='" + String(mailPresent ? "status mail-present" : "status no-mail") + "'>"
                + String(mailPresent ? "Mail is present!" : "No mail") +
                "</div>"
                "<div class='info'>Last mail received at: <span id='lastTime'>" + lastMailTime + "</span></div>"
                "<div class='mail-count'>Total Mail Received: <span id='mailCount'>" + String(mailCount) + "</span></div>"
                "<div class='btn-container'>"
                "<button class='btn refresh-btn' onclick='refreshMeasurement()'>Check Now</button>"
                "<button class='btn calibrate-btn' onclick='calibrateBaseline()'>Calibrate Empty</button>"
                "</div>"
                "<div class='distance-info'>Current distance: <span id='distance'>" + String(distance) + "</span> mm</div>"
                "<div class='distance-info'>Baseline distance: <span id='baseline'>" + String(baselineDistance) + "</span> mm</div>"
                "<div class='distance-info'>Difference: <span id='difference' class='difference'>" + String(baselineDistance - distance) + "</span> mm</div>"
                "<div class='distance-info'>Last distance change: <span id='lastChange'>" + lastDistanceChange + "</span></div>"
                "</div>"
                "</body>"
                "</html>";
                
  server.send(200, "text/html", html);
}

void handleData() {
  String jsonResponse = "{\"mailPresent\":" + String(mailPresent ? "true" : "false") + 
                       ",\"lastMailTime\":\"" + lastMailTime + 
                       "\",\"distance\":" + String(distance) + 
                       ",\"baseline\":" + String(baselineDistance) + 
                       ",\"lastChange\":\"" + lastDistanceChange + 
                       "\",\"mailChangeCount\":" + String(mailChangeCount) +
                       ",\"mailCount\":" + String(mailCount) + "}";
  server.send(200, "application/json", jsonResponse);
}

void handleRefresh() {
  measureDistance();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleCalibrate() {
  calibrateBaseline();
  
  String jsonResponse = "{\"success\":true,\"baseline\":" + String(baselineDistance) + 
                       ",\"mailChangeCount\":" + String(mailChangeCount) + 
                       ",\"mailCount\":" + String(mailCount) + "}";
  server.send(200, "application/json", jsonResponse);
}