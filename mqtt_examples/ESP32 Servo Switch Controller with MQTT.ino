#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// WiFi credentials
const char* ssid = "HOSTEL1";
const char* password = "12345678";

// MQTT Configuration
const char* mqtt_server = "2332bf283a3042789deec54af864c4d4.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "admin";
const char* mqtt_password = "Admin@123";

// MQTT Topics
const char* topic_command = "home/switch/command";     // Subscribe to commands
const char* topic_state = "home/switch/state";         // Publish current state
const char* topic_status = "home/switch/status";       // Publish device status
const char* topic_availability = "home/switch/availability"; // Device availability

// Device ID (unique identifier for your device)
String deviceId = "ESP32_Switch_" + String((uint32_t)ESP.getEfuseMac(), HEX);

// Create instances
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);
Servo switchServo;

// Pin configuration
const int servoPin = 18;  // GPIO18
const int ledPin = 2;     // Built-in LED (GPIO2 on most ESP32 boards)

// State variables
bool isOn = false;
unsigned long lastActionTime = 0;
const unsigned long actionCooldown = 1000; // 1 second cooldown between actions
unsigned long lastMqttReconnect = 0;
const unsigned long mqttReconnectInterval = 5000; // 5 seconds between reconnection attempts
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000; // Send heartbeat every 30 seconds

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Servo Switch Controller with MQTT ===");
  
  // Initialize EEPROM to remember state
  EEPROM.begin(512);
  isOn = EEPROM.read(0);
  if (isOn != 0 && isOn != 1) {
    isOn = false; // Default to OFF if EEPROM is uninitialized
  }
  
  // Initialize pins
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, isOn ? HIGH : LOW); // ESP32 LED is not inverted by default
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  switchServo.setPeriodHertz(50);    // standard 50 hz servo
  switchServo.attach(servoPin, 500, 2500); // attaches the servo with min/max pulse width
  switchServo.write(90);  // Initial position (center/neutral)
  delay(500);
  
  Serial.println("Hardware initialized");
  Serial.println("Device ID: " + deviceId);
  Serial.println("Restored state: " + String(isOn ? "ON" : "OFF"));
  
  // Connect to Wi-Fi
  connectToWiFi();
  
  // Configure time (needed for SSL)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println(" Time synchronized");
  
  // Setup MQTT
  setupMQTT();
  
  // Start mDNS service
  if (MDNS.begin("switch")) {
    Serial.println("mDNS responder started");
    Serial.println("Access via: http://switch.local");
    
    // Add service to mDNS-SD
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "board", "ESP32");
    MDNS.addServiceTxt("http", "tcp", "path", "/");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
  
  // Setup web server routes
  setupWebServer();
  
  // Start the server
  server.begin();
  Serial.println("Web server started successfully");
  Serial.println("=== Setup Complete ===\n");
}

void loop() {
  // Handle WiFi reconnection if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting reconnection...");
    connectToWiFi();
    
    // Restart mDNS after WiFi reconnection
    if (MDNS.begin("switch")) {
      MDNS.addService("http", "tcp", 80);
    }
  }
  
  // Handle MQTT connection
  if (!mqttClient.connected()) {
    if (millis() - lastMqttReconnect > mqttReconnectInterval) {
      lastMqttReconnect = millis();
      if (reconnectMQTT()) {
        Serial.println("MQTT reconnected successfully");
        publishState();  // Publish current state after reconnection
        publishAvailability("online");
      }
    }
  } else {
    mqttClient.loop();
    
    // Send periodic heartbeat
    if (millis() - lastHeartbeat > heartbeatInterval) {
      lastHeartbeat = millis();
      publishStatus();
    }
  }
  
  // Handle web server requests
  server.handleClient();
  
  // Small delay to prevent watchdog reset
  delay(10);
}

void setupMQTT() {
  // Configure SSL client to skip certificate verification (for simplicity)
  // In production, you should use proper certificate verification
  espClient.setInsecure();
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  
  Serial.println("MQTT configured");
  Serial.println("Server: " + String(mqtt_server) + ":" + String(mqtt_port));
}

bool reconnectMQTT() {
  Serial.print("Attempting MQTT connection...");
  
  // Create a random client ID
  String clientId = deviceId + "_" + String(random(0xffff), HEX);
  
  // Set Last Will and Testament
  String willTopic = topic_availability;
  String willMessage = "offline";
  
  if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password, 
                        willTopic.c_str(), 1, true, willMessage.c_str())) {
    Serial.println(" connected!");
    
    // Subscribe to command topic
    if (mqttClient.subscribe(topic_command, 1)) {
      Serial.println("Subscribed to: " + String(topic_command));
    } else {
      Serial.println("Failed to subscribe to command topic");
    }
    
    // Publish availability
    publishAvailability("online");
    
    return true;
  } else {
    Serial.print(" failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" retrying in 5 seconds");
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("MQTT Message received:");
  Serial.println("Topic: " + String(topic));
  Serial.println("Payload: " + message);
  
  // Check cooldown
  if (millis() - lastActionTime < actionCooldown) {
    Serial.println("Action rejected: cooldown active");
    return;
  }
  
  // Process commands
  if (String(topic) == topic_command) {
    message.toLowerCase();
    message.trim();
    
    if (message == "on" || message == "1" || message == "true") {
      if (!isOn) {
        Serial.println("MQTT Command: Turn ON");
        toggleSwitch(true);
        publishState();
      } else {
        Serial.println("Switch is already ON");
      }
    } 
    else if (message == "off" || message == "0" || message == "false") {
      if (isOn) {
        Serial.println("MQTT Command: Turn OFF");
        toggleSwitch(false);
        publishState();
      } else {
        Serial.println("Switch is already OFF");
      }
    }
    else if (message == "toggle") {
      Serial.println("MQTT Command: Toggle");
      toggleSwitch(!isOn);
      publishState();
    }
    else if (message == "status") {
      Serial.println("MQTT Command: Status request");
      publishState();
      publishStatus();
    }
    else {
      Serial.println("Unknown MQTT command: " + message);
    }
  }
}

void publishState() {
  if (mqttClient.connected()) {
    String state = isOn ? "ON" : "OFF";
    if (mqttClient.publish(topic_state, state.c_str(), true)) {
      Serial.println("State published: " + state);
    } else {
      Serial.println("Failed to publish state");
    }
  }
}

void publishStatus() {
  if (mqttClient.connected()) {
    // Create JSON status message
    String status = "{";
    status += "\"device_id\":\"" + deviceId + "\",";
    status += "\"state\":\"" + String(isOn ? "ON" : "OFF") + "\",";
    status += "\"uptime\":" + String(millis() / 1000) + ",";
    status += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    status += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    status += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    status += "\"timestamp\":" + String(millis());
    status += "}";
    
    if (mqttClient.publish(topic_status, status.c_str())) {
      Serial.println("Status published");
    } else {
      Serial.println("Failed to publish status");
    }
  }
}

void publishAvailability(const char* availability) {
  if (mqttClient.connected()) {
    if (mqttClient.publish(topic_availability, availability, true)) {
      Serial.println("Availability published: " + String(availability));
    } else {
      Serial.println("Failed to publish availability");
    }
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("Please check your credentials and try again");
  }
}

void setupWebServer() {
  // Root page - main control interface
  server.on("/", handleRoot);
  
  // Control endpoints
  server.on("/on", handleSwitchOn);
  server.on("/off", handleSwitchOff);
  server.on("/toggle", handleToggle);
  server.on("/status", handleStatus);
  
  // Handle 404 errors
  server.onNotFound(handleNotFound);
}

void handleRoot() {
  String html = generateHTML();
  server.send(200, "text/html", html);
}

void handleSwitchOn() {
  if (millis() - lastActionTime < actionCooldown) {
    server.send(429, "text/plain", "Too many requests. Please wait.");
    return;
  }
  
  if (!isOn) {
    toggleSwitch(true);
    publishState(); // Publish state change via MQTT
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Redirecting...");
  } else {
    server.send(200, "text/plain", "Switch is already ON");
  }
}

void handleSwitchOff() {
  if (millis() - lastActionTime < actionCooldown) {
    server.send(429, "text/plain", "Too many requests. Please wait.");
    return;
  }
  
  if (isOn) {
    toggleSwitch(false);
    publishState(); // Publish state change via MQTT
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Redirecting...");
  } else {
    server.send(200, "text/plain", "Switch is already OFF");
  }
}

void handleToggle() {
  if (millis() - lastActionTime < actionCooldown) {
    server.send(429, "text/plain", "Too many requests. Please wait.");
    return;
  }
  
  toggleSwitch(!isOn);
  publishState(); // Publish state change via MQTT
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecting...");
}

void handleStatus() {
  String json = "{";
  json += "\"device_id\":\"" + deviceId + "\",";
  json += "\"status\":\"" + String(isOn ? "ON" : "OFF") + "\",";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"mqtt_connected\":" + String(mqttClient.connected() ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: " + server.uri() + "\n";
  message += "Method: " + String((server.method() == HTTP_GET) ? "GET" : "POST") + "\n";
  message += "Arguments: " + String(server.args()) + "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}

String generateHTML() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Servo Switch Controller</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background: #f0f0f0; margin: 0; padding: 20px; }";
  html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; margin-bottom: 30px; }";
  html += ".status { font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 5px; font-weight: bold; }";
  html += ".status.on { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".status.off { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += ".button { display: inline-block; padding: 15px 30px; margin: 10px; font-size: 18px; text-decoration: none; border-radius: 5px; border: none; cursor: pointer; transition: all 0.3s; }";
  html += ".button.on { background: #28a745; color: white; }";
  html += ".button.on:hover { background: #218838; }";
  html += ".button.off { background: #dc3545; color: white; }";
  html += ".button.off:hover { background: #c82333; }";
  html += ".button.toggle { background: #007bff; color: white; }";
  html += ".button.toggle:hover { background: #0056b3; }";
  html += ".info { margin-top: 30px; font-size: 14px; color: #666; }";
  html += ".info div { margin: 5px 0; }";
  html += ".mqtt-status { padding: 10px; margin: 10px 0; border-radius: 5px; font-weight: bold; }";
  html += ".mqtt-connected { background: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; }";
  html += ".mqtt-disconnected { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += "@media (max-width: 480px) { .container { margin: 10px; padding: 20px; } }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>🔌 Servo Switch Controller</h1>";
  
  // Status display
  html += "<div class='status " + String(isOn ? "on" : "off") + "'>";
  html += "Status: " + String(isOn ? "🟢 ON" : "🔴 OFF");
  html += "</div>";
  
  // MQTT Status
  html += "<div class='mqtt-status " + String(mqttClient.connected() ? "mqtt-connected" : "mqtt-disconnected") + "'>";
  html += "MQTT: " + String(mqttClient.connected() ? "🌐 Connected" : "❌ Disconnected");
  html += "</div>";
  
  // Control buttons
  html += "<div>";
  if (!isOn) {
    html += "<a href='/on' class='button on'>Turn ON</a>";
  }
  if (isOn) {
    html += "<a href='/off' class='button off'>Turn OFF</a>";
  }
  html += "<a href='/toggle' class='button toggle'>Toggle</a>";
  html += "</div>";
  
  // System information
  html += "<div class='info'>";
  html += "<div><strong>System Info:</strong></div>";
  html += "<div>Device ID: " + deviceId + "</div>";
  html += "<div>IP: " + WiFi.localIP().toString() + "</div>";
  html += "<div>Signal: " + String(WiFi.RSSI()) + " dBm</div>";
  html += "<div>Uptime: " + String(millis() / 1000) + "s</div>";
  html += "<div>Free Memory: " + String(ESP.getFreeHeap()) + " bytes</div>";
  html += "<div><strong>MQTT Topics:</strong></div>";
  html += "<div>Command: " + String(topic_command) + "</div>";
  html += "<div>State: " + String(topic_state) + "</div>";
  html += "</div>";
  
  html += "</div>";
  
  // Auto-refresh script
  html += "<script>";
  html += "setTimeout(function(){ location.reload(); }, 30000);"; // Refresh every 30 seconds
  html += "</script>";
  
  html += "</body>";
  html += "</html>";
  
  return html;
}

void toggleSwitch(bool turnOn) {
  lastActionTime = millis();
  
  Serial.println("=== Switch Operation ===");
  Serial.print("Turning switch ");
  Serial.println(turnOn ? "ON" : "OFF");
  
  // Update LED status immediately
  digitalWrite(ledPin, turnOn ? HIGH : LOW); // ESP32 LED is not inverted by default
  
  if (turnOn) {
    // Move servo to ON position (clockwise from center)
    Serial.println("Moving servo to ON position (180°)");
    switchServo.write(180);
    delay(800); // Wait for servo to reach position and press switch
    
    // Return to neutral position
    Serial.println("Returning servo to neutral position");
    switchServo.write(90);  // Return to center (90° is middle position)
    delay(500);
    
    isOn = true;
    Serial.println("✅ Switch turned ON successfully");
  } else {
    // Move servo to OFF position (counterclockwise from center)
    Serial.println("Moving servo to OFF position (0°)");
    switchServo.write(0);
    delay(800); // Wait for servo to reach position and press switch
    
    // Return to neutral position
    Serial.println("Returning servo to neutral position");
    switchServo.write(90);  // Return to center (90° is middle position)
    delay(500);
    
    isOn = false;
    Serial.println("✅ Switch turned OFF successfully");
  }
  
  // Save state to EEPROM
  EEPROM.write(0, isOn);
  EEPROM.commit();
  Serial.println("State saved to EEPROM");
  Serial.println("=== Operation Complete ===\n");
}
