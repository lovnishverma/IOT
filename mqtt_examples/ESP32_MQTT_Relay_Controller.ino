#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Wi-Fi credentials
const char* ssid = "isro";
const char* password = "Isro1234";

// HiveMQ Cloud Credentials
const char* mqttServer = "1b29169c90f24560b78dea233a792d18.s1.eu.hivemq.cloud";
const int mqttPort = 8883;  // Secure MQTT port
const char* mqttUser = "nielit212";
const char* mqttPassword = "iloveMqtt212";
const char* mqttTopic = "212";
const char* clientId = "ESP32_Client";

// Secure Wi-Fi Client
WiFiClientSecure espClient;
PubSubClient client(espClient);

bool relayStateD2 = false; // Initial relay state for GPIO4
bool relayStateD6 = false; // Initial relay state for GPIO12
const int buzzerPin = 5;   // GPIO 5 for buzzer
const int relayPinD2 = 4;  // GPIO 4 for relay 1
const int relayPinD6 = 12; // GPIO 12 for relay 2

WebServer server(80);

// Function to produce a short beep
void beep() {
  digitalWrite(buzzerPin, HIGH);
  delay(100);
  digitalWrite(buzzerPin, LOW);
}

// MQTT Callback function
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received: ");
  Serial.write(payload, length);
  Serial.println();

  if (length == 1) {
    char command = (char)payload[0];

    if (command == '0') {
      // Turn off both relays
      relayStateD2 = false;
      relayStateD6 = false;
      digitalWrite(relayPinD2, HIGH);
      digitalWrite(relayPinD6, HIGH);
      beep();
    } else if (command == '1') {
      // Turn on both relays
      relayStateD2 = true;
      relayStateD6 = true;
      digitalWrite(relayPinD2, LOW);
      digitalWrite(relayPinD6, LOW);
      beep();
    }
  }
}

// Reconnect to MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to HiveMQ Cloud...");
    espClient.setInsecure(); // Disable SSL certificate verification

    if (client.connect(clientId, mqttUser, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(mqttTopic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 2 seconds...");
      delay(2000);
    }
  }
}

// Web server: root page
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 MQTT Relay Controller</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background: #f0f0f0; margin: 0; padding: 20px; }";
  html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; margin-bottom: 30px; }";
  html += ".status { font-size: 18px; margin: 15px 0; padding: 10px; border-radius: 5px; }";
  html += ".status.on { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".status.off { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += ".button { display: inline-block; padding: 15px 30px; margin: 10px; font-size: 18px; text-decoration: none; border-radius: 5px; background: #007bff; color: white; border: none; cursor: pointer; transition: all 0.3s; }";
  html += ".button:hover { background: #0056b3; }";
  html += ".info { margin-top: 30px; font-size: 14px; color: #666; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>🔌 ESP32 MQTT Relay Controller</h1>";
  
  // Relay status display
  html += "<div class='status " + String(relayStateD2 ? "on" : "off") + "'>";
  html += "Relay 1 (GPIO4): " + String(relayStateD2 ? "🟢 ON" : "🔴 OFF");
  html += "</div>";
  
  html += "<div class='status " + String(relayStateD6 ? "on" : "off") + "'>";
  html += "Relay 2 (GPIO12): " + String(relayStateD6 ? "🟢 ON" : "🔴 OFF");
  html += "</div>";
  
  // Control button
  html += "<a href='/toggle' class='button'>Toggle Both Relays</a>";
  
  // System information
  html += "<div class='info'>";
  html += "<div><strong>System Info:</strong></div>";
  html += "<div>IP: " + WiFi.localIP().toString() + "</div>";
  html += "<div>Signal: " + String(WiFi.RSSI()) + " dBm</div>";
  html += "<div>MQTT: " + String(client.connected() ? "Connected" : "Disconnected") + "</div>";
  html += "</div>";
  
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Web server: toggle both relays
void handleToggle() {
  // Toggle both relays
  relayStateD2 = !relayStateD2;
  relayStateD6 = relayStateD2;  // Keep both relays in the same state

  // Turn both relays on or off based on the toggle state
  digitalWrite(relayPinD2, relayStateD2 ? LOW : HIGH);
  digitalWrite(relayPinD6, relayStateD6 ? LOW : HIGH);
  
  beep();

  // Publish to MQTT to reflect the state
  client.publish(mqttTopic, relayStateD2 ? "1" : "0");

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "Redirecting...");
}

void setup() {
  // Initialize pins
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPinD2, OUTPUT);
  pinMode(relayPinD6, OUTPUT);
  
  // Set initial relay states (HIGH = OFF for active-low relays)
  digitalWrite(relayPinD2, HIGH);
  digitalWrite(relayPinD6, HIGH);
  digitalWrite(buzzerPin, LOW);

  Serial.begin(115200);
  Serial.println("\n=== ESP32 MQTT Relay Controller ===");

  // Connect to WiFi
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
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("Please check your credentials and try again");
    return;
  }

  // Start mDNS for easier local network access
  if (MDNS.begin("esp32")) {
    Serial.println("mDNS responder started");
    Serial.println("Access via: http://esp32.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.begin();
  Serial.println("Web server started");

  // MQTT Setup
  espClient.setInsecure();  // Skip SSL certificate verification
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  Serial.println("=== Setup Complete ===\n");
  reconnect();
}

void loop() {
  // Handle WiFi reconnection if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting reconnection...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconnected!");
      // Restart mDNS after WiFi reconnection
      if (MDNS.begin("esp32")) {
        MDNS.addService("http", "tcp", 80);
      }
    }
  }
  
  // Handle MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();
  server.handleClient();
  
  // Small delay to prevent watchdog reset
  delay(10);
}
