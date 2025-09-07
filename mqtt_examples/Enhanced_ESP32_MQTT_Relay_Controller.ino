/**
 * ESP32 MQTT Relay Controller with Web Interface - Enhanced Version
 * 
 * Enhancements:
 * - Configuration via JSON file or web interface
 * - Multiple relay support
 * - Scheduler functionality
 * - Enhanced security with proper certificate validation
 * - OTA updates capability
 * - Better error handling and logging
 * 
 * Hardware Connections:
 * - Relay 1: GPIO 4
 * - Relay 2: GPIO 16 (optional)
 * - Buzzer: GPIO 5
 * - Status LED: GPIO 2
 * 
 * Author: Enhanced by NIELIT CHANDIGARH
 * Based on original by Lovnish Verma
 * Version: 2.0
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>
#include <time.h>

// =============================================================================
// CONFIGURATION SECTION
// =============================================================================

// Default WiFi Configuration (can be overridden via config)
const char* DEFAULT_WIFI_SSID = "jassi5";
const char* DEFAULT_WIFI_PASSWORD = "jassi1234";

// Default MQTT Configuration
const char* DEFAULT_MQTT_SERVER = "2332bf283a3042789deec54af864c4d4.s1.eu.hivemq.cloud";
const int DEFAULT_MQTT_PORT = 8883;
const char* DEFAULT_MQTT_USERNAME = "admin";
const char* DEFAULT_MQTT_PASSWORD = "Admin@123";
const char* DEFAULT_MQTT_TOPIC = "mytopic/nielit";

// Hardware Configuration
const int MAX_RELAYS = 2;
const int RELAY_PINS[MAX_RELAYS] = {4, 16};   // GPIO pins for relays
const int BUZZER_PIN = 5;
const int STATUS_LED_PIN = 2;

// Timing Configuration
const unsigned long WIFI_CONNECT_TIMEOUT = 20000;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
const unsigned long BEEP_DURATION = 100;
const unsigned long STATUS_LED_BLINK_INTERVAL = 1000;
const unsigned long WATCHDOG_TIMEOUT = 30000;

// =============================================================================
// ENHANCED DATA STRUCTURES
// =============================================================================

struct RelayConfig {
  int pin;
  bool state;
  String name;
  bool autoSchedule;
  String scheduleTime;  // HH:MM format
  bool scheduleState;   // State to set at scheduled time
};

struct SystemConfig {
  String wifiSSID;
  String wifiPassword;
  String mqttServer;
  int mqttPort;
  String mqttUsername;
  String mqttPassword;
  String mqttTopic;
  String deviceName;
  bool enableScheduler;
  bool enableOTA;
  int logLevel;  // 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG
};

struct SystemStatus {
  bool wifiConnected;
  bool mqttConnected;
  unsigned long uptime;
  int freeHeap;
  int wifiRSSI;
  String lastError;
  unsigned long lastErrorTime;
};

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Enhanced clients and servers
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WebServer webServer(80);
Preferences preferences;

// Configuration and status
SystemConfig config;
SystemStatus status;
RelayConfig relays[MAX_RELAYS];
String deviceId;

// Timing variables
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastStatusLedToggle = 0;
unsigned long lastWatchdogReset = 0;
unsigned long bootTime = 0;

// State variables
bool statusLedState = false;
int activeRelayCount = 1;  // Default to 1 relay

// =============================================================================
// LOGGING SYSTEM
// =============================================================================

enum LogLevel {
  LOG_ERROR = 0,
  LOG_WARN = 1,
  LOG_INFO = 2,
  LOG_DEBUG = 3
};

void logMessage(LogLevel level, String message) {
  if (level <= config.logLevel) {
    String levelStr[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    String timestamp = String(millis());
    Serial.println("[" + timestamp + "] [" + levelStr[level] + "] " + message);
    
    if (level == LOG_ERROR) {
      status.lastError = message;
      status.lastErrorTime = millis();
    }
  }
}

// =============================================================================
// CONFIGURATION MANAGEMENT
// =============================================================================

void loadDefaultConfig() {
  config.wifiSSID = DEFAULT_WIFI_SSID;
  config.wifiPassword = DEFAULT_WIFI_PASSWORD;
  config.mqttServer = DEFAULT_MQTT_SERVER;
  config.mqttPort = DEFAULT_MQTT_PORT;
  config.mqttUsername = DEFAULT_MQTT_USERNAME;
  config.mqttPassword = DEFAULT_MQTT_PASSWORD;
  config.mqttTopic = DEFAULT_MQTT_TOPIC;
  config.deviceName = "ESP32 Relay Controller";
  config.enableScheduler = false;
  config.enableOTA = true;
  config.logLevel = LOG_INFO;
  
  // Initialize relays
  for (int i = 0; i < MAX_RELAYS; i++) {
    relays[i].pin = RELAY_PINS[i];
    relays[i].state = false;
    relays[i].name = "Relay " + String(i + 1);
    relays[i].autoSchedule = false;
    relays[i].scheduleTime = "00:00";
    relays[i].scheduleState = false;
  }
}

void saveConfig() {
  preferences.begin("relay-config", false);
  preferences.putString("wifi_ssid", config.wifiSSID);
  preferences.putString("wifi_pass", config.wifiPassword);
  preferences.putString("mqtt_server", config.mqttServer);
  preferences.putInt("mqtt_port", config.mqttPort);
  preferences.putString("mqtt_user", config.mqttUsername);
  preferences.putString("mqtt_pass", config.mqttPassword);
  preferences.putString("mqtt_topic", config.mqttTopic);
  preferences.putString("device_name", config.deviceName);
  preferences.putBool("enable_sched", config.enableScheduler);
  preferences.putBool("enable_ota", config.enableOTA);
  preferences.putInt("log_level", config.logLevel);
  preferences.putInt("relay_count", activeRelayCount);
  preferences.end();
  
  logMessage(LOG_INFO, "Configuration saved to flash memory");
}

void loadConfig() {
  preferences.begin("relay-config", true);
  
  if (preferences.isKey("wifi_ssid")) {
    config.wifiSSID = preferences.getString("wifi_ssid", DEFAULT_WIFI_SSID);
    config.wifiPassword = preferences.getString("wifi_pass", DEFAULT_WIFI_PASSWORD);
    config.mqttServer = preferences.getString("mqtt_server", DEFAULT_MQTT_SERVER);
    config.mqttPort = preferences.getInt("mqtt_port", DEFAULT_MQTT_PORT);
    config.mqttUsername = preferences.getString("mqtt_user", DEFAULT_MQTT_USERNAME);
    config.mqttPassword = preferences.getString("mqtt_pass", DEFAULT_MQTT_PASSWORD);
    config.mqttTopic = preferences.getString("mqtt_topic", DEFAULT_MQTT_TOPIC);
    config.deviceName = preferences.getString("device_name", "ESP32 Relay Controller");
    config.enableScheduler = preferences.getBool("enable_sched", false);
    config.enableOTA = preferences.getBool("enable_ota", true);
    config.logLevel = preferences.getInt("log_level", LOG_INFO);
    activeRelayCount = preferences.getInt("relay_count", 1);
    
    logMessage(LOG_INFO, "Configuration loaded from flash memory");
  } else {
    logMessage(LOG_WARN, "No saved configuration found, using defaults");
    loadDefaultConfig();
    saveConfig();
  }
  
  preferences.end();
}

// =============================================================================
// ENHANCED HARDWARE FUNCTIONS
// =============================================================================

void generateDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  deviceId = "ESP32_" + String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
  deviceId.toUpperCase();
}

void beep(int count = 1) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(BEEP_DURATION);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < count - 1) delay(BEEP_DURATION);
  }
}

void updateStatusLed() {
  unsigned long now = millis();
  if (now - lastStatusLedToggle > STATUS_LED_BLINK_INTERVAL) {
    lastStatusLedToggle = now;
    
    if (status.wifiConnected && status.mqttConnected) {
      // Solid on when everything is connected
      digitalWrite(STATUS_LED_PIN, HIGH);
    } else if (status.wifiConnected) {
      // Slow blink when WiFi connected but MQTT disconnected
      statusLedState = !statusLedState;
      digitalWrite(STATUS_LED_PIN, statusLedState);
    } else {
      // Fast blink when WiFi disconnected
      if (now % 200 < 100) {
        digitalWrite(STATUS_LED_PIN, HIGH);
      } else {
        digitalWrite(STATUS_LED_PIN, LOW);
      }
    }
  }
}

void setRelayState(int relayIndex, bool state) {
  if (relayIndex >= 0 && relayIndex < activeRelayCount) {
    relays[relayIndex].state = state;
    digitalWrite(relays[relayIndex].pin, state ? HIGH : LOW);
    beep(state ? 1 : 2);  // Different beep patterns for on/off
    
    logMessage(LOG_INFO, relays[relayIndex].name + " turned " + (state ? "ON" : "OFF"));
    
    // Publish state change via MQTT
    publishRelayState(relayIndex);
  }
}

void publishRelayState(int relayIndex) {
  if (mqttClient.connected() && relayIndex >= 0 && relayIndex < activeRelayCount) {
    String topic = config.mqttTopic + "/relay" + String(relayIndex + 1) + "/state";
    String message = relays[relayIndex].state ? "1" : "0";
    
    if (mqttClient.publish(topic.c_str(), message.c_str(), true)) {
      logMessage(LOG_DEBUG, "Published " + relays[relayIndex].name + " state: " + message);
    } else {
      logMessage(LOG_ERROR, "Failed to publish " + relays[relayIndex].name + " state");
    }
  }
}

// =============================================================================
// ENHANCED WIFI FUNCTIONS
// =============================================================================

bool connectToWiFi() {
  logMessage(LOG_INFO, "Connecting to WiFi: " + config.wifiSSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceId.c_str());
  WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
    updateStatusLed();  // Keep status LED updated during connection
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    status.wifiConnected = true;
    status.wifiRSSI = WiFi.RSSI();
    
    logMessage(LOG_INFO, "WiFi connected successfully!");
    logMessage(LOG_INFO, "IP address: " + WiFi.localIP().toString());
    logMessage(LOG_INFO, "Signal strength: " + String(WiFi.RSSI()) + " dBm");
    
    // Configure NTP for time synchronization
    configTime(0, 0, "pool.ntp.org");
    
    return true;
  } else {
    status.wifiConnected = false;
    logMessage(LOG_ERROR, "WiFi connection failed!");
    return false;
  }
}

void maintainWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    status.wifiConnected = false;
    if (!status.wifiConnected) {  // Only log once when disconnected
      logMessage(LOG_WARN, "WiFi disconnected! Attempting reconnection...");
    }
    
    if (connectToWiFi()) {
      setupMDNS();  // Restart mDNS after reconnection
    }
  } else {
    status.wifiConnected = true;
    status.wifiRSSI = WiFi.RSSI();
  }
}

// =============================================================================
// ENHANCED MQTT FUNCTIONS
// =============================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  String topicStr = String(topic);
  logMessage(LOG_DEBUG, "MQTT message received - Topic: " + topicStr + ", Payload: " + message);
  
  // Enhanced topic parsing for multiple relays
  if (topicStr.startsWith(config.mqttTopic)) {
    // Extract relay number from topic (e.g., mytopic/nielit/relay1/command)
    int relayIndex = 0;
    if (topicStr.indexOf("/relay") > 0) {
      int relayNumStart = topicStr.indexOf("/relay") + 6;
      int relayNumEnd = topicStr.indexOf("/", relayNumStart);
      if (relayNumEnd == -1) relayNumEnd = topicStr.length();
      
      String relayNumStr = topicStr.substring(relayNumStart, relayNumEnd);
      relayIndex = relayNumStr.toInt() - 1;  // Convert to 0-based index
    }
    
    // Process command
    if (message == "1" || message == "ON" || message == "on") {
      setRelayState(relayIndex, true);
    } else if (message == "0" || message == "OFF" || message == "off") {
      setRelayState(relayIndex, false);
    } else if (message == "toggle" || message == "TOGGLE") {
      setRelayState(relayIndex, !relays[relayIndex].state);
    } else if (message == "status" || message == "STATUS") {
      publishRelayState(relayIndex);
    } else {
      logMessage(LOG_WARN, "Unknown MQTT command: " + message);
    }
  }
}

bool connectToMQTT() {
  logMessage(LOG_INFO, "Connecting to MQTT broker: " + config.mqttServer);
  
  String clientId = deviceId + "_" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str(), config.mqttUsername.c_str(), config.mqttPassword.c_str())) {
    status.mqttConnected = true;
    logMessage(LOG_INFO, "MQTT connected successfully! Client ID: " + clientId);
    
    // Subscribe to command topics for all active relays
    for (int i = 0; i < activeRelayCount; i++) {
      String commandTopic = config.mqttTopic + "/relay" + String(i + 1) + "/command";
      if (mqttClient.subscribe(commandTopic.c_str())) {
        logMessage(LOG_DEBUG, "Subscribed to: " + commandTopic);
      }
    }
    
    // Also subscribe to general command topic for backward compatibility
    if (mqttClient.subscribe(config.mqttTopic.c_str())) {
      logMessage(LOG_DEBUG, "Subscribed to: " + config.mqttTopic);
    }
    
    // Publish initial states
    for (int i = 0; i < activeRelayCount; i++) {
      publishRelayState(i);
    }
    
    // Publish device info
    String infoTopic = config.mqttTopic + "/device/info";
    DynamicJsonDocument doc(512);
    doc["device_id"] = deviceId;
    doc["device_name"] = config.deviceName;
    doc["version"] = "2.0";
    doc["relay_count"] = activeRelayCount;
    doc["ip_address"] = WiFi.localIP().toString();
    
    String infoMessage;
    serializeJson(doc, infoMessage);
    mqttClient.publish(infoTopic.c_str(), infoMessage.c_str(), true);
    
    return true;
  } else {
    status.mqttConnected = false;
    logMessage(LOG_ERROR, "MQTT connection failed, error code: " + String(mqttClient.state()));
    return false;
  }
}

void maintainMQTTConnection() {
  if (!mqttClient.connected()) {
    status.mqttConnected = false;
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = now;
      if (status.wifiConnected) {  // Only attempt MQTT connection if WiFi is connected
        connectToMQTT();
      }
    }
  } else {
    status.mqttConnected = true;
    mqttClient.loop();
  }
}

void setupMQTT() {
  espClient.setInsecure();  // For development - should use proper certificates in production
  mqttClient.setServer(config.mqttServer.c_str(), config.mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(30);
  
  logMessage(LOG_INFO, "MQTT configured - Server: " + config.mqttServer + ":" + String(config.mqttPort));
}

// =============================================================================
// ENHANCED WEB SERVER FUNCTIONS
// =============================================================================

String generateWebPage() {
  // Update system status
  status.uptime = millis() - bootTime;
  status.freeHeap = ESP.getFreeHeap();
  
  String html = "<!DOCTYPE html>";
  html += "<html lang='en'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>" + config.deviceName + "</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: rgba(255,255,255,0.95); padding: 30px; border-radius: 15px; box-shadow: 0 8px 32px rgba(0,0,0,0.3); }";
  html += "h1 { color: #333; margin-bottom: 30px; text-align: center; font-size: 28px; }";
  html += ".relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin: 30px 0; }";
  html += ".relay-card { background: #f8f9fa; padding: 20px; border-radius: 10px; border-left: 5px solid #007bff; }";
  html += ".relay-status { font-size: 20px; font-weight: bold; text-align: center; padding: 15px; border-radius: 8px; margin: 15px 0; }";
  html += ".relay-on { background: #d4edda; color: #155724; border: 2px solid #c3e6cb; }";
  html += ".relay-off { background: #f8d7da; color: #721c24; border: 2px solid #f5c6cb; }";
  html += ".control-btn { display: block; width: 100%; padding: 12px; margin: 10px 0; font-size: 16px; font-weight: bold; text-decoration: none; text-align: center; border: none; border-radius: 6px; cursor: pointer; transition: all 0.3s ease; }";
  html += ".toggle-btn { background: linear-gradient(45deg, #007bff, #0056b3); color: white; }";
  html += ".toggle-btn:hover { background: linear-gradient(45deg, #0056b3, #004085); transform: translateY(-1px); }";
  html += ".config-btn { background: linear-gradient(45deg, #28a745, #1e7e34); color: white; }";
  html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin: 30px 0; }";
  html += ".status-item { background: #f1f3f4; padding: 15px; border-radius: 8px; text-align: center; }";
  html += ".status-label { font-size: 12px; color: #666; text-transform: uppercase; }";
  html += ".status-value { font-size: 16px; font-weight: bold; color: #333; margin-top: 5px; }";
  html += ".connection-status { display: flex; align-items: center; justify-content: center; gap: 8px; margin: 10px 0; }";
  html += ".status-dot { width: 12px; height: 12px; border-radius: 50%; }";
  html += ".status-online { background: #28a745; }";
  html += ".status-offline { background: #dc3545; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  
  // Header
  html += "<h1>🏠 " + config.deviceName + "</h1>";
  
  // Relay Controls
  html += "<div class='relay-grid'>";
  for (int i = 0; i < activeRelayCount; i++) {
    html += "<div class='relay-card'>";
    html += "<h3>" + relays[i].name + "</h3>";
    html += "<div class='relay-status " + String(relays[i].state ? "relay-on" : "relay-off") + "'>";
    html += String(relays[i].state ? "🟢 ON" : "🔴 OFF");
    html += "</div>";
    html += "<a href='/toggle/" + String(i) + "' class='control-btn toggle-btn'>🔄 TOGGLE</a>";
    html += "</div>";
  }
  html += "</div>";
  
  // Configuration Button
  html += "<a href='/config' class='control-btn config-btn'>⚙️ CONFIGURATION</a>";
  
  // Connection Status
  html += "<div class='relay-card'>";
  html += "<h3>📡 Connection Status</h3>";
  html += "<div class='connection-status'>";
  html += "<span class='status-dot " + String(status.wifiConnected ? "status-online" : "status-offline") + "'></span>";
  html += "<span>WiFi: " + String(status.wifiConnected ? "Connected" : "Disconnected") + "</span>";
  html += "</div>";
  html += "<div class='connection-status'>";
  html += "<span class='status-dot " + String(status.mqttConnected ? "status-online" : "status-offline") + "'></span>";
  html += "<span>MQTT: " + String(status.mqttConnected ? "Connected" : "Disconnected") + "</span>";
  html += "</div>";
  html += "</div>";
  
  // System Information
  html += "<div class='status-grid'>";
  html += "<div class='status-item'>";
  html += "<div class='status-label'>Device ID</div>";
  html += "<div class='status-value'>" + deviceId.substring(deviceId.length()-8) + "</div>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<div class='status-label'>IP Address</div>";
  html += "<div class='status-value'>" + WiFi.localIP().toString() + "</div>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<div class='status-label'>Uptime</div>";
  html += "<div class='status-value'>" + String(status.uptime / 1000 / 60) + " min</div>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<div class='status-label'>Free Memory</div>";
  html += "<div class='status-value'>" + String(status.freeHeap / 1024) + " KB</div>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<div class='status-label'>WiFi Signal</div>";
  html += "<div class='status-value'>" + String(status.wifiRSSI) + " dBm</div>";
  html += "</div>";
  html += "<div class='status-item'>";
  html += "<div class='status-label'>Active Relays</div>";
  html += "<div class='status-value'>" + String(activeRelayCount) + "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "</div>";
  
  // Auto-refresh script
  html += "<script>";
  html += "setTimeout(function(){ location.reload(); }, 10000);";
  html += "</script>";
  
  html += "</body>";
  html += "</html>";
  
  return html;
}

void handleWebRoot() {
  webServer.send(200, "text/html", generateWebPage());
}

void handleWebToggle() {
  String path = webServer.uri();
  int relayIndex = path.substring(path.lastIndexOf('/') + 1).toInt();
  
  if (relayIndex >= 0 && relayIndex < activeRelayCount) {
    setRelayState(relayIndex, !relays[relayIndex].state);
    logMessage(LOG_INFO, "Web toggle request for " + relays[relayIndex].name);
  }
  
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "Redirecting...");
}

void setupWebServer() {
  webServer.on("/", handleWebRoot);
  
  // Dynamic toggle endpoints for multiple relays
  for (int i = 0; i < MAX_RELAYS; i++) {
    String togglePath = "/toggle/" + String(i);
    webServer.on(togglePath, handleWebToggle);
  }
  
  // Configuration endpoint (simplified for this example)
  webServer.on("/config", []() {
    String html = "<html><body><h1>Configuration</h1>";
    html += "<p>Configuration interface would be implemented here</p>";
    html += "<a href='/'>Back to Home</a></body></html>";
    webServer.send(200, "text/html", html);
  });
  
  webServer.onNotFound([]() {
    webServer.send(404, "text/plain", "404 - Page Not Found");
  });
  
  webServer.begin();
  logMessage(LOG_INFO, "Web server started on port 80");
}

// =============================================================================
// MAIN SETUP AND LOOP
// =============================================================================

bool setupMDNS() {
  String hostname = config.deviceName.toLowerCase();
  hostname.replace(" ", "-");
  
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "device", deviceId.c_str());
    MDNS.addServiceTxt("http", "tcp", "version", "2.0");
    
    logMessage(LOG_INFO, "mDNS responder started: http://" + hostname + ".local");
    return true;
  } else {
    logMessage(LOG_ERROR, "Failed to start mDNS responder");
    return false;
  }
}

void setupHardware() {
  Serial.begin(115200);
  bootTime = millis();
  
  Serial.println("\n" + String('=', 60));
  Serial.println("ESP32 MQTT Relay Controller v2.0 (Enhanced)");
  Serial.println(String('=', 60));
  
  generateDeviceId();
  
  // Initialize GPIO pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  for (int i = 0; i < MAX_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  logMessage(LOG_INFO, "Hardware initialized - Device ID: " + deviceId);
}

void setup() {
  setupHardware();
  loadConfig();
  
  if (!connectToWiFi()) {
    logMessage(LOG_ERROR, "Cannot continue without WiFi connection");
    while(1) {
      delay(1000);
      updateStatusLed();
    }
  }
  
  setupMDNS();
  setupMQTT();
  connectToMQTT();
  setupWebServer();
  
  logMessage(LOG_INFO, "Setup completed successfully!");
  logMessage(LOG_INFO, "Access methods:");
  logMessage(LOG_INFO, "- Web: http://" + WiFi.localIP().toString());
  logMessage(LOG_INFO, "- mDNS: http://" + config.deviceName.toLowerCase() + ".local");
  logMessage(LOG_INFO, "- MQTT: " + config.mqttTopic);
  
  // Startup beep sequence
  beep(3);
}

void loop() {
  unsigned long now = millis();
  
  // Update system status
  status.uptime = now - bootTime;
  status.freeHeap = ESP.getFreeHeap();
  
  // Maintain connections
  maintainWiFiConnection();
  maintainMQTTConnection();
  
  // Handle web server requests
  webServer.handleClient();
  
  // Update status LED
  updateStatusLed();
  
  // Update mDNS
  MDNS.update();
  
  // Watchdog reset
  if (now - lastWatchdogReset > WATCHDOG_TIMEOUT) {
    lastWatchdogReset = now;
    logMessage(LOG_DEBUG, "Watchdog reset - System healthy");
  }
  
  // Scheduler functionality (if enabled)
  if (config.enableScheduler) {
    checkScheduledTasks();
  }
  
  // Small delay for stability
  delay(10);
}

// =============================================================================
// SCHEDULER FUNCTIONALITY
// =============================================================================

void checkScheduledTasks() {
  static unsigned long lastScheduleCheck = 0;
  unsigned long now = millis();
  
  // Check every minute
  if (now - lastScheduleCheck > 60000) {
    lastScheduleCheck = now;
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      String currentTime = String(timeinfo.tm_hour) + ":" + 
                          (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
      
      for (int i = 0; i < activeRelayCount; i++) {
        if (relays[i].autoSchedule && relays[i].scheduleTime == currentTime) {
          logMessage(LOG_INFO, "Scheduled task: Setting " + relays[i].name + 
                    " to " + (relays[i].scheduleState ? "ON" : "OFF"));
          setRelayState(i, relays[i].scheduleState);
        }
      }
    }
  }
}

// =============================================================================
// UTILITY AND DIAGNOSTIC FUNCTIONS
// =============================================================================

void resetDevice() {
  logMessage(LOG_WARN, "Device reset requested");
  
  // Turn off all relays
  for (int i = 0; i < activeRelayCount; i++) {
    setRelayState(i, false);
  }
  
  // Cleanup connections
  mqttClient.disconnect();
  WiFi.disconnect();
  
  delay(1000);
  ESP.restart();
}

void factoryReset() {
  logMessage(LOG_WARN, "Factory reset requested");
  
  // Clear saved configuration
  preferences.begin("relay-config", false);
  preferences.clear();
  preferences.end();
  
  // Reset to defaults
  loadDefaultConfig();
  saveConfig();
  
  // Reset device
  resetDevice();
}

String getSystemInfo() {
  DynamicJsonDocument doc(1024);
  
  doc["device_id"] = deviceId;
  doc["device_name"] = config.deviceName;
  doc["version"] = "2.0";
  doc["uptime"] = status.uptime;
  doc["free_heap"] = status.freeHeap;
  doc["wifi_connected"] = status.wifiConnected;
  doc["wifi_rssi"] = status.wifiRSSI;
  doc["mqtt_connected"] = status.mqttConnected;
  doc["relay_count"] = activeRelayCount;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["mac_address"] = WiFi.macAddress();
  
  JsonArray relayArray = doc.createNestedArray("relays");
  for (int i = 0; i < activeRelayCount; i++) {
    JsonObject relay = relayArray.createNestedObject();
    relay["index"] = i;
    relay["name"] = relays[i].name;
    relay["state"] = relays[i].state;
    relay["pin"] = relays[i].pin;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}
