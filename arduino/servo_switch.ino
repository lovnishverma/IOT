#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Servo.h>

// WiFi credentials
const char* ssid = "HOSTEL1";
const char* password = "12345678";

// Create web server on port 80
ESP8266WebServer server(80);
Servo switchServo;

// Pin configuration
const int servoPin = 2;  // GPIO2 (D4)
const int ledPin = LED_BUILTIN;  // Built-in LED for status

// State variables
bool isOn = false;
unsigned long lastActionTime = 0;
const unsigned long actionCooldown = 1000; // 1 second cooldown between actions

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP8266 Servo Switch Controller ===");
  
  // Initialize EEPROM to remember state
  EEPROM.begin(512);
  isOn = EEPROM.read(0);
  if (isOn != 0 && isOn != 1) {
    isOn = false; // Default to OFF if EEPROM is uninitialized
  }
  
  // Initialize pins
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, !isOn); // LED is inverted (LOW = ON)
  
  // Initialize servo
  switchServo.attach(servoPin);
  switchServo.write(90);  // Initial position (center/neutral)
  delay(500);
  
  Serial.println("Hardware initialized");
  Serial.println("Restored state: " + String(isOn ? "ON" : "OFF"));
  
  // Connect to Wi-Fi
  connectToWiFi();
  
  // Start mDNS service
  if (MDNS.begin("switch")) {
    Serial.println("mDNS responder started");
    Serial.println("Access via: http://switch.local");
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
  }
  
  // Update mDNS
  MDNS.update();
  
  // Handle web server requests
  server.handleClient();
  
  // Small delay to prevent watchdog reset
  delay(10);
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
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecting...");
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + String(isOn ? "ON" : "OFF") + "\",";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
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
  html += "<title>ESP8266 Servo Switch Controller</title>";
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
  html += "<div>IP: " + WiFi.localIP().toString() + "</div>";
  html += "<div>Signal: " + String(WiFi.RSSI()) + " dBm</div>";
  html += "<div>Uptime: " + String(millis() / 1000) + "s</div>";
  html += "<div>Free Memory: " + String(ESP.getFreeHeap()) + " bytes</div>";
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
  digitalWrite(ledPin, !turnOn); // LED is inverted
  
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