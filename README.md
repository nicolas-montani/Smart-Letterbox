# Building a Smart Mailbox
A projct as part of the 'Embedded Sensing Systems' course at the university of St.Gallen in Spring 2025. 


## General Overview
This repository consists of two implementations of the Smart Letterbox system: a base version using HTTP for communication, and an enhanced version operating over MQTT. These implementations represent the evolution of our design thinking and technical approach throughout the project.

## Main Objectives
Our primary objectives for the Smart Letterbox project were:

- connectivity and Reliability: Create a system that maintains robust connectivity even in challenging WiFi environments, ensuring continuous mail monitoring.
- Simple User Experience: Provide an intuitive interface that gives users clear information about their mail status without requiring technical knowledge.
- Real-time Notifications: Alert users promptly when mail arrives or is removed, making the traditional letterbox "smart" by connecting it to the user's digital life.
- Low Maintenance Operation: Design a system that operates reliably for extended periods without requiring frequent maintenance or interventions.

## Design Decisions

### Sensor Selection
Sensor Selection
We decided early on to use an ultrasonic sensor (HC-SR04) as our primary sensing mechanism. Our early MVP testing demonstrated that this sensor could reliably detect distance changes in the millimeter range, which proved sufficient for our mail detection needs. The ultrasonic approach offered several advantages:

- Non-contact measurement: No mechanical parts to wear out
- Immunity to light conditions: Functions equally well in dark letterboxes
- Simple calibration: Self-calibrates to establish the "empty" baseline
- Low power consumption: Suitable for battery-powered operation

We originally considered including a scale sensor to measure weight changes when mail arrived. However, we encountered practical challenges with implementing this in a letterbox context. Additionally, we learned that another project group was already pursuing a weight-based approach, which motivated us to differentiate our solution.

### Prototype Design
One key decision was to construct our prototype using a cardboard box rather than modifying an actual letterbox. This pragmatic choice was driven by several factors:

- Our team members were distributed across three different cities, making it impractical to work on a shared physical letterbox
The cardboard prototype allowed each team member to work on the setup in the university office at Torstrasse.
- It simplified iteration and modification during development
- It created a controlled environment for sensor testing without external variables

### Battery and Power Considerations
After evaluating power requirements, we made the following decisions:

ESP32 as the edge device: The ESP32 offeres sufficient processing capabilities and robust WiFi connectivity features that were crucial for reliability. It is powerable with a simple external battery via mini-usb. 

USB power with battery backup: For our prototype, we designed the system to primarily use USB power, with provisions for connecting a power bank as a backup power source. This decision balanced:
- Practical testing needs (continuous operation during development)
- Real-world resilience (backup power during outages)
- Future extensibility (could be converted to solar power with minor modifications)


Power optimization in software: Rather than compromising on hardware capabilities, we implemented power-saving measures in our code:
- periodic measurement and transmission rather than continuous sensing
- Intelligent WiFi reconnection with progressive backoff
- Fallback to AP mode when connection repeatedly fails (saving energy wasted on futile reconnection attempts)

### Networking Architecture
1) Initial HTTP Implementation:
- Direct HTTP POST requests from ESP32 to Raspberry Pi
- Simple request-response model
- Straightforward to implement and debug
- Resonable decision, as we measured all our actual letterboxes at home to be reachable by our home-wifi. 

2) Advanced MQTT Implementation
- Publish-subscribe model with Mosquitto broker
- Lower overhead per message
- Better suited for IoT applications
- More efficient for battery-powered devices
- Enhanced scalability for potential expansion

Advantages of the MQTT aproach are:
- Reduced power consumption
- More reliable message delivery
- Better failure recovery
- Cleaner separation of concerns in the codebase

### Compute Distribution
We designed a hybrid edge-server compute architecture:
### ESP32 Edge Processing:
- Sensor data collection and filtering
- Mail presence detection algorithms
- Basic data processing and formatting
- State tracking (mail count, baseline measurements)
### Raspberry Pi Server Processing:
- Data storage and persistence
- Web interface hosting
- Notification management
- Historical data analysis

This distribution enabled real-time mail detection at the edge while offloading more resource-intensive operations to the Raspberry Pi.


## Trade-Offs

Throughout our design process, we navigated several significant trade-offs:

### Simplicity vs. Robustness:
- Decided to go with simpler hardware setup and focus on advanced software features.
- We chose to add complexity in the form of WiFi reconnection logic and AP mode fallback
- This increased code complexity but dramatically improved system reliability

### Accuracy vs. Battery Life:
- Multiple sensor readings improved accuracy but consumed more power
- We balanced this by taking clustered readings (3 measurements in quick succession) at reasonable intervals

### HTTP extendet to MQTT:
- MQTT added complexity in setup (requiring broker configuration)
- However, it provided significant benefits in efficiency and reliability
- Provides a solution which is more typically used in IoT offering benefits for constrained devices

### Notification Mechanisms (Thrid-party vs open source solution):
- We selected Techulus Push over email or SMS notifications
- This simplified implementation but created a dependency on a third-party service
- The trade-off prioritized development speed and user experience over complete autonomy


## System build
The Smart Letterbox system can be reproduced by following the detailed instructions provided in our documentation files:

Hardware Components: HC-SR04 ultrasonic sensor, ESP32 microcontroller, and Raspberry Pi
Software Components:

ESP32 firmware: letterbox_ultrasound_mqtt.ino
Raspberry Pi server: raspberry_pi_mqtt_server.py
Web interface: Automatically generated by the server script


Construction Details:

For physical assembly and wiring, refer to setup_instructions.md
Connect the HC-SR04 as specified: VCC to 5V, GND to GND, TRIG to GPIO 2, ECHO to GPIO 4


Server Setup:

Follow the step-by-step Raspberry Pi configuration in setup_instructions.md
For MQTT-specific configuration, refer to mqtt_setup_instructions.md
Mosquitto broker installation instructions are available in README.md


Network Configuration:

Configure WiFi credentials in the ESP32 code
Set up the MQTT broker as detailed in mqtt_setup_instructions.md
Ensure both devices are on the same network or properly configured for remote access


Notifications:

Set up Techulus Push integration following the instructions in the web interface
Update the API key in the server code as documented



For full reproducibility, all configuration files and code are thoroughly documented with comments to explain each component's function and interaction within the system.


## System Evaluation

### Hard Requirements Achieved
Our Smart Letterbox implementation successfully met all the specified system requirements:

1) Architecture Requirements:
- Battery-based Microcontroller with Sensors: Implemented using an ESP32 microcontroller with an HC-SR04 ultrasonic sensor for mail detection. The system is designed for operation via USB power with battery backup capability.
- Wireless Communication to Backend: Successfully implemented two communication protocols:
  - HTTP-based communication in the initial version
  - MQTT-based communication in the enhanced version, providing more efficient IoT messaging
- Real-time Dashboard and Notifications: Developed a responsive web dashboard on the Raspberry Pi backend that updates every 5 seconds, coupled with a push notification system using the Techulus API.

2) Functionality Requirements:
- Mail Detection and Notification: System accurately detects when new mail arrives with 90% reliability and sends immediate notifications to users. The average notification delay is under 5 seconds.

- Statistical Dashboard: The web interface displays comprehensive statistics including:
  - Current mail presence status
  - Total mail count
  - Time of last mail delivery
  - Distance measurements (current and baseline)
  - Change history
 
3) Targeted Notifications: Implemented push notifications via the Techulus app, which allows specific users to receive alerts. The notification system includes different message types for mail arrival, removal, and additional deliveries.

### Objective Grading Criteries Ensured
Our Smart Letterbox implementation successfully met all the minimum requirements specified in the grading criteria:

1) Complete and Well-Organized Code:
- Both implementations (HTTP and MQTT) are fully functional with clear code structure
- Functions are logically separated with descriptive names
- Consistent error handling and logging throughout
- Comprehensive comments explaining key algorithms and decision points

2) Detailed Documentation:
- Setup instructions provided in multiple markdown files
- Step-by-step installation guides for both components
- Troubleshooting sections addressing common issues
- Clear explanation of design decisions and implementation process
- Network architecture diagrams included for clarity

3) Repository Completeness:
- All necessary code files included for both ESP32 and Raspberry Pi
- Configuration files and templates provided
- Hardware connection specifications documented
- Reproducible build process clearly outlined

### System Precision and Capabilities
Our evaluation testing revealed the specific performance characteristics of the implemented system:

1) Measurement Precision:

- Ultrasonic sensor achieved ±2mm measurement accuracy
- Multiple reading averaging improved reliability
- Detection threshold of 30mm proved optimal for standard mail items
- System achieved > 90% detection rate in real-world testing

2) Connectivity Features:
- MQTT implementation provided > 80% reconnection success rate
- Automatic fallback to Access Point mode ensured continuous operation
- Web interface accessible from any device on the local network
- Push notifications delivered with less than 5-second latency

### Key System Features:

- Real-time mail status monitoring
- Push notifications for mail arrival and removal
- Automatic baseline calibration
- Detection of additional mail deliveries
- Data persistence across system restarts
- Web dashboard with live updates
- Resilience to network interruptions
- Detailed (serial) logging for troubleshooting

## Future Extensions
Our modular and extensible design provides multiple pathways for future enhancements:

1) Sensor extension:
- Integration of weight sensors to differentiate between mail types
- Addition of light sensors to detect openings of the letterbox door
- Temperature/humidity sensors to monitor environmental conditions

2) Enhanced Analytics:
- Historical data analysis to identify delivery patterns
- Predictive algorithms for anticipated delivery times
- Mail volume tracking over time with visualization

3) LoRa Network Integration:
- Implementing LoRa (Long Range) network connectivity to replace or complement WiFi for communication in areas with poor or no WiFi coverage.
- LoRa offers low-power, long-range communication, ideal for IoT devices like the Smart Letterbox.
- This integration would extend the system's range, enabling smart mailboxes in remote or rural areas where WiFi might not be readily available.

4) Power Optimizations:
- Implementation of deep sleep modes or duty-cycles for extended battery life
- Solar power integration for self-sustaining operation
- Adaptive measurement frequency based on delivery patterns rcognized by either tinyAI on ESP32 or EdgeAI on RasperyPI backend


## TL;DR

### System Design

- Created a Smart Letterbox using ESP32 with ultrasonic sensor for mail detection and Raspberry Pi for processing
- Evolved from HTTP to MQTT communication for improved reliability and efficiency
- Implemented robust WiFi with fallback to Access Point mode, ensuring continuous operation
- Balanced design trade-offs between power consumption, accuracy, and user experience

### System Build

- Hardware: ESP32, HC-SR04 ultrasonic sensor, Raspberry Pi
- Software: Arduino firmware, Flask web server, MQTT broker
- Complete build instructions documented for reproducibility
- Push notifications via Techulus API for real-time alerts

### System Evaluation

- Achieved > 90% mail detection accuracy with ±2mm measurement precision
- MQTT implementation provided > 80 % network reconnection reliability
- Met all hard requirements with complete code, documentation, and reproducibility
- Extensible architecture ready for sensor fusion, analytics, and IoT integration


