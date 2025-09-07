/**
 * ESP32 MQTT Relay Controller with Web Interface
 * 
 * Features:
 * - Secure MQTT connection to HiveMQ Cloud
 * - Web interface accessible via IP and mDNS (relay.local)
 * - Relay control via MQTT messages and web interface
 * - Buzzer feedback for relay operations
 * - Auto-reconnection for WiFi and MQTT
 * - Production-ready error handling
 * 
 * Hardware Connections:
 * - Relay: GPIO 4
 * - Buzzer: GPIO 5
 * 
 * MQTT Commands:
 * - Send "1" to turn relay ON
 * - Send "0" to turn relay OFF
 * 
 * Author: Lovnish Verma
 * Version: 1.0
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>

// =============================================================================
// CONFIGURATION SECTION
// =============================================================================

// WiFi Configuration
const char* WIFI_SSID = "jassi5";
const char* WIFI_PASSWORD = "jassi1234";

// MQTT Configuration - HiveMQ Cloud
const char* MQTT_SERVER = "2332bf283a3042789deec54af864c4d4.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;                    // Secure MQTT port
const char* MQTT_USERNAME = "admin";
const char* MQTT_PASSWORD = "Admin@123";
const char* MQTT_TOPIC = "mytopic/nielit";
const int MQTT_QOS = 0;

// mDNS Configuration
const char* MDNS_HOSTNAME = "relay";           // Access via http://relay.local

// Hardware Pin Configuration
const int BUZZER_PIN = 5;                      // GPIO 5 for buzzer
const int RELAY_PIN = 4;                       // GPIO 4 for relay control

// Timing Configuration
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;    // 15 seconds
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;   // 5 seconds
const unsigned long BEEP_DURATION = 100;              // 100ms beep
const unsigned long WEB_REFRESH_INTERVAL = 10000;     // 10 seconds auto-refresh

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Network and MQTT clients
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WebServer webServer(80);

// Device identification
String deviceId;

// State variables
bool relayState = false;                       // Current relay state (false = OFF, true = ON)
char lastMqttMessage[2] = "";                 // Store last MQTT message to prevent duplicates
unsigned long lastMqttReconnectAttempt = 0;   // Last MQTT reconnection attempt time

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Generate unique device ID based on ESP32 MAC address
 */
void generateDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  deviceId = "ESP32_Relay_" + String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
  deviceId.toUpperCase();
}

/**
 * Produce a short beep sound
 */
void beep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(BEEP_DURATION);
  digitalWrite(BUZZER_PIN, LOW);
}

/**
 * Set relay state and update LED/buzzer
 * @param state - true for ON, false for OFF
 */
void setRelayState(bool state) {
  relayState = state;
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  beep();
  Serial.println("Relay turned " + String(state ? "ON" : "OFF"));
}

/**
 * Publish current relay state to MQTT
 */
void publishRelayState() {
  if (mqttClient.connected()) {
    String stateMessage = relayState ? "1" : "0";
    if (mqttClient.publish(MQTT_TOPIC, stateMessage.c_str(), true)) {
      Serial.println("Published relay state: " + stateMessage);
    } else {
      Serial.println("Failed to publish relay state");
    }
  }
}

// =============================================================================
// WIFI FUNCTIONS
// =============================================================================

/**
 * Connect to WiFi network with timeout
 * @return true if connected, false if timeout
 */
bool connectToWiFi() {
  Serial.println("Connecting to WiFi: " + String(WIFI_SSID));
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected successfully!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    Serial.println("Signal strength: " + String(WiFi.RSSI()) + " dBm");
    return true;
  } else {
    Serial.println("\n✗ WiFi connection failed!");
    return false;
  }
}

/**
 * Check WiFi connection and reconnect if needed
 */
void maintainWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting reconnection...");
    if (connectToWiFi()) {
      // Restart mDNS after WiFi reconnection
      setupMDNS();
    }
  }
}

// =============================================================================
// MDNS FUNCTIONS
// =============================================================================

/**
 * Setup mDNS service for local network access
 * @return true if successful, false if failed
 */
bool setupMDNS() {
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("✓ mDNS responder started");
    Serial.println("Access via: http://" + String(MDNS_HOSTNAME) + ".local");
    
    // Add HTTP service advertisement
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "board", "ESP32");
    MDNS.addServiceTxt("http", "tcp", "device", deviceId.c_str());
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    
    return true;
  } else {
    Serial.println("✗ Error setting up mDNS responder!");
    return false;
  }
}

// =============================================================================
// MQTT FUNCTIONS
// =============================================================================

/**
 * MQTT message callback function
 * @param topic - MQTT topic
 * @param payload - Message payload
 * @param length - Payload length
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("MQTT message received:");
  Serial.println("Topic: " + String(topic));
  Serial.println("Payload: " + message);
  
  // Process only single character messages and avoid duplicates
  if (length == 1 && payload[0] != lastMqttMessage[0]) {
    if (payload[0] == '0') {
      setRelayState(false);
    } else if (payload[0] == '1') {
      setRelayState(true);
    } else {
      Serial.println("Unknown MQTT command: " + message);
      return;
    }
    
    // Store last message to prevent duplicate processing
    strncpy(lastMqttMessage, (char*)payload, sizeof(lastMqttMessage));
  }
}

/**
 * Connect to MQTT broker
 * @return true if connected, false if failed
 */
bool connectToMQTT() {
  Serial.println("Connecting to MQTT broker...");
  
  // Generate unique client ID for this connection
  String clientId = deviceId + "_" + String(random(0xffff), HEX);
  
  // Attempt connection with credentials
  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("✓ MQTT connected successfully!");
    Serial.println("Client ID: " + clientId);
    
    // Subscribe to command topic
    if (mqttClient.subscribe(MQTT_TOPIC, MQTT_QOS)) {
      Serial.println("✓ Subscribed to topic: " + String(MQTT_TOPIC));
    } else {
      Serial.println("✗ Failed to subscribe to topic");
    }
    
    // Clear any retained messages and publish initial state
    mqttClient.publish(MQTT_TOPIC, "", true);
    publishRelayState();
    
    return true;
  } else {
    Serial.println("✗ MQTT connection failed, error code: " + String(mqttClient.state()));
    return false;
  }
}

/**
 * Maintain MQTT connection with automatic reconnection
 */
void maintainMQTTConnection() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = now;
      connectToMQTT();
    }
  } else {
    mqttClient.loop();
  }
}

/**
 * Setup MQTT client configuration
 */
void setupMQTT() {
  // Configure SSL client (skip certificate verification for simplicity)
  espClient.setInsecure();
  
  // Configure MQTT client
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  
  Serial.println("MQTT configured:");
  Serial.println("Server: " + String(MQTT_SERVER) + ":" + String(MQTT_PORT));
  Serial.println("Topic: " + String(MQTT_TOPIC));
}

// =============================================================================
// WEB SERVER FUNCTIONS
// =============================================================================

/**
 * Generate HTML for the main web page
 * @return Complete HTML string
 */
String generateWebPage() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 MQTT Relay Controller</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }";
  html += ".container { max-width: 500px; margin: 0 auto; background: rgba(255,255,255,0.95); padding: 30px; border-radius: 15px; box-shadow: 0 8px 32px rgba(0,0,0,0.3); backdrop-filter: blur(10px); }";
  html += "h1 { color: #333; margin-bottom: 30px; text-align: center; font-size: 28px; }";
  html += ".status-card { background: #f8f9fa; padding: 20px; border-radius: 10px; margin: 20px 0; border-left: 5px solid #007bff; }";
  html += ".relay-status { font-size: 24px; font-weight: bold; text-align: center; padding: 20px; border-radius: 10px; margin: 20px 0; }";
  html += ".relay-on { background: #d4edda; color: #155724; border: 2px solid #c3e6cb; }";
  html += ".relay-off { background: #f8d7da; color: #721c24; border: 2px solid #f5c6cb; }";
  html += ".control-btn { display: block; width: 100%; padding: 15px; margin: 15px 0; font-size: 18px; font-weight: bold; text-decoration: none; text-align: center; border: none; border-radius: 8px; cursor: pointer; transition: all 0.3s ease; }";
  html += ".toggle-btn { background: linear-gradient(45deg, #007bff, #0056b3); color: white; }";
  html += ".toggle-btn:hover { background: linear-gradient(45deg, #0056b3, #004085); transform: translateY(-2px); box-shadow: 0 4px 12px rgba(0,123,255,0.3); }";
  html += ".info-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-top: 30px; }";
  html += ".info-item { background: #f1f3f4; padding: 15px; border-radius: 8px; text-align: center; }";
  html += ".info-label { font-size: 12px; color: #666; text-transform: uppercase; letter-spacing: 0.5px; }";
  html += ".info-value { font-size: 16px; font-weight: bold; color: #333; margin-top: 5px; }";
  html += ".connection-status { display: flex; align-items: center; justify-content: center; gap: 8px; margin: 10px 0; }";
  html += ".status-dot { width: 12px; height: 12px; border-radius: 50%; }";
  html += ".status-online { background: #28a745; }";
  html += ".status-offline { background: #dc3545; }";
  html += "@media (max-width: 600px) { .container { margin: 10px; padding: 20px; } .info-grid { grid-template-columns: 1fr; } }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  
  // Header
  html += "<h1>🔌 ESP32 Relay Controller</h1>";
  
  // Relay Status
  html += "<div class='relay-status " + String(relayState ? "relay-on" : "relay-off") + "'>";
  html += String(relayState ? "🟢 RELAY ON" : "🔴 RELAY OFF");
  html += "</div>";
  
  // Control Button
  html += "<a href='/toggle' class='control-btn toggle-btn'>";
  html += "🔄 TOGGLE RELAY";
  html += "</a>";
  
  // Connection Status
  html += "<div class='status-card'>";
  html += "<div class='connection-status'>";
  html += "<span class='status-dot " + String(WiFi.status() == WL_CONNECTED ? "status-online" : "status-offline") + "'></span>";
  html += "<span>WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</span>";
  html += "</div>";
  html += "<div class='connection-status'>";
  html += "<span class='status-dot " + String(mqttClient.connected() ? "status-online" : "status-offline") + "'></span>";
  html += "<span>MQTT: " + String(mqttClient.connected() ? "Connected" : "Disconnected") + "</span>";
  html += "</div>";
  html += "</div>";
  
  // System Information
  html += "<div class='info-grid'>";
  html += "<div class='info-item'>";
  html += "<div class='info-label'>Device ID</div>";
  html += "<div class='info-value'>" + deviceId.substring(deviceId.length()-8) + "</div>";
  html += "</div>";
  html += "<div class='info-item'>";
  html += "<div class='info-label'>IP Address</div>";
  html += "<div class='info-value'>" + WiFi.localIP().toString() + "</div>";
  html += "</div>";
  html += "<div class='info-item'>";
  html += "<div class='info-label'>mDNS URL</div>";
  html += "<div class='info-value'>relay.local</div>";
  html += "</div>";
  html += "<div class='info-item'>";
  html += "<div class='info-label'>WiFi Signal</div>";
  html += "<div class='info-value'>" + String(WiFi.RSSI()) + " dBm</div>";
  html += "</div>";
  html += "<div class='info-item'>";
  html += "<div class='info-label'>MQTT Topic</div>";
  html += "<div class='info-value'>" + String(MQTT_TOPIC) + "</div>";
  html += "</div>";
  html += "<div class='info-item'>";
  html += "<div class='info-label'>Free Memory</div>";
  html += "<div class='info-value'>" + String(ESP.getFreeHeap()/1024) + " KB</div>";
  html += "</div>";
  html += "</div>";
  
  html += "</div>";
  
  // Auto-refresh script
  html += "<script>";
  html += "setTimeout(function(){ location.reload(); }, " + String(WEB_REFRESH_INTERVAL) + ");";
  html += "</script>";
  
  html += "</body>";
  html += "</html>";
  
  return html;
}

/**
 * Handle root web page request
 */
void handleWebRoot() {
  String html = generateWebPage();
  webServer.send(200, "text/html", html);
}

/**
 * Handle relay toggle request
 */
void handleWebToggle() {
  // Toggle relay state
  setRelayState(!relayState);
  
  // Publish new state to MQTT
  publishRelayState();
  
  // Redirect back to main page
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "Redirecting...");
}

/**
 * Handle 404 not found requests
 */
void handleWebNotFound() {
  String message = "404 - File Not Found\n\n";
  message += "URI: " + webServer.uri() + "\n";
  message += "Method: " + String((webServer.method() == HTTP_GET) ? "GET" : "POST") + "\n";
  
  webServer.send(404, "text/plain", message);
}

/**
 * Setup web server routes
 */
void setupWebServer() {
  webServer.on("/", handleWebRoot);
  webServer.on("/toggle", handleWebToggle);
  webServer.onNotFound(handleWebNotFound);
  
  webServer.begin();
  Serial.println("✓ Web server started on port 80");
}

// =============================================================================
// MAIN FUNCTIONS
// =============================================================================

/**
 * Initialize hardware and peripherals
 */
void setupHardware() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n" + String("=").substring(0, 50));
  Serial.println("ESP32 MQTT Relay Controller v1.0");
  Serial.println(String("=").substring(0, 50));
  
  // Generate unique device ID
  generateDeviceId();
  Serial.println("Device ID: " + deviceId);
  
  // Initialize GPIO pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Set initial states
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  
  Serial.println("✓ Hardware initialized");
}

/**
 * Arduino setup function
 */
void setup() {
  // Initialize hardware
  setupHardware();
  
  // Connect to WiFi
  if (!connectToWiFi()) {
    Serial.println("✗ Cannot continue without WiFi connection");
    Serial.println("Please check WiFi credentials and restart");
    while(1) delay(1000); // Halt execution
  }
  
  // Setup mDNS
  setupMDNS();
  
  // Setup MQTT
  setupMQTT();
  connectToMQTT();
  
  // Setup web server
  setupWebServer();
  
  // Initialization complete
  Serial.println(String("=").substring(0, 50));
  Serial.println("✓ Setup completed successfully!");
  Serial.println("Access methods:");
  Serial.println("- Web: http://" + WiFi.localIP().toString());
  Serial.println("- mDNS: http://" + String(MDNS_HOSTNAME) + ".local");
  Serial.println("- MQTT: Send '1' or '0' to " + String(MQTT_TOPIC));
  Serial.println(String("=").substring(0, 50) + "\n");
  
  // Initial beep to indicate ready
  beep();
}

/**
 * Arduino main loop function
 */
void loop() {
  // Maintain network connections
  maintainWiFiConnection();
  maintainMQTTConnection();
  
  // Handle web server requests
  webServer.handleClient();
  
  // Update mDNS
  MDNS.update();
  
  // Small delay for stability
  delay(10);
}
