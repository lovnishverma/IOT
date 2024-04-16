#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

const char* ssid = "Meet131";
const char* password = "8699453054@056";
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttTopic = "mytopic/bag";
const int qos = 0;
const char* clientId = "clientId-karsog";

WiFiClient espClient;
PubSubClient client(espClient);

char lastMessage[2] = "";
bool relayState = true;
bool localSwitchState = true;

const int buzzerPin = 5;
const int relayPin = 4;

ESP8266WebServer server(80);

void beep() {
  digitalWrite(buzzerPin, HIGH);
  delay(100);
  digitalWrite(buzzerPin, LOW);
}

void callback(char* topic, byte* payload, unsigned int length);

void reconnect() {
  while (!client.connected()) {
    if (client.connect(clientId)) {
      client.subscribe(mqttTopic, qos);
      client.publish(mqttTopic, "", true);
      client.publish(mqttTopic, "1", true);
    } else {
      delay(2000);
    }
  }
}

void handleRoot() {
  String html = "<html><body><h1>Lovnish's MQTT Toggle Switch</h1>";
  html += "<p>Relay State: " + String(relayState ? "ON" : "OFF") + "</p>";
  html += "<p><a href=\"/toggle\">Toggle Relay</a></p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleToggle() {
  relayState = !relayState;
  digitalWrite(relayPin, relayState ? HIGH : LOW);
  beep();

  String statePayload = relayState ? "1" : "0";
  client.publish(mqttTopic, statePayload.c_str(), true);

  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayState ? HIGH : LOW);
  Serial.begin(115200);
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  while (!client.connected()) {
    reconnect();
  }
  MDNS.begin("lovnish");
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.begin();
}

void loop() {
  MDNS.update();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  server.handleClient();
}

void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (!error && doc.containsKey("command")) {
    const char* command = doc["command"];
    if (strcmp(command, "0") == 0) {
      relayState = false;
      beep();
    } else if (strcmp(command, "1") == 0) {
      relayState = true;
      beep();
    }
    digitalWrite(relayPin, relayState ? HIGH : LOW);
  }
}
