#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// Wi-Fi credentials
const char* ssid = "isro";
const char* password = "Isro1234";

// HiveMQ Cloud Credentials
const char* mqttServer = "1b29169c90f24560b78dea233a792d18.s1.eu.hivemq.cloud";
const int mqttPort = 8883;  // Secure MQTT port
const char* mqttUser = "nielit212";
const char* mqttPassword = "iloveMqtt212";
const char* mqttTopic = "212";
const char* clientId = "ESP8266_Client";

// Secure Wi-Fi Client
WiFiClientSecure espClient;
PubSubClient client(espClient);

bool relayState = false; // Initial relay state OFF
const int buzzerPin = 5;  // GPIO 5 (D1) for buzzer
const int relayPin = 4;   // GPIO 4 (D2) for relay

ESP8266WebServer server(80);

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
    relayState = (payload[0] == '0');
    digitalWrite(relayPin, relayState ? HIGH : LOW);
    beep();
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
  String html = "<html><body><h1>MQTT Toggle Relay</h1>";
  html += "<p>Relay State: " + String(relayState ? "ON" : "OFF") + "</p>";
  html += "<p><a href=\"/toggle\">Toggle Relay</a></p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Web server: toggle relay
void handleToggle() {
  relayState = !relayState;
  digitalWrite(relayPin, relayState ? HIGH : LOW);
  beep();

  client.publish(mqttTopic, relayState ? "0" : "1");

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "Redirecting...");
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayState ? HIGH : LOW);

  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start mDNS for easier local network access
  MDNS.begin("esp8266");
  Serial.println("mDNS responder started");

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.begin();

  // MQTT Setup
  espClient.setInsecure();  // Skip SSL certificate verification
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  reconnect();
}

void loop() {
  MDNS.update();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  server.handleClient();
}
