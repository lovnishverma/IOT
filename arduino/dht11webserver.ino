#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>

#define DHTPIN 2      // Pin connected to the DHT11 sensor
#define DHTTYPE DHT11 // DHT sensor type

const char* ssid = "isro";
const char* password = "Isro1234";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;

unsigned long previousMillis = 0;
const long interval = 3000;      // Interval between readings (3 seconds)
unsigned long wifiCheckMillis = 0;
const long wifiCheckInterval = 10000;  // Interval for checking WiFi connection (10 seconds)

void setup() {
  Serial.begin(115200);
  dht.begin();
  connectWiFi();
}

void loop() {
  unsigned long currentMillis = millis();

  // Check WiFi connection at regular intervals
  if (WiFi.status() != WL_CONNECTED && currentMillis - wifiCheckMillis >= wifiCheckInterval) {
    wifiCheckMillis = currentMillis;
    reconnectWiFi();
  }

  // Send data at the specified interval
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendSensorData();
  }
}

void sendSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor! Retrying...");
    return;
  }

  String jsonData = "{\"sensorid\":\"DHT-11\",\"samplename\":\"NIELIT Ropar\",\"temp\":" + String(temperature, 2) + ",\"hum\":" + String(humidity, 2) + "}";
  
  HTTPClient http;
  int retryCount = 0;
  bool success = false;

  while (retryCount < 3 && !success) {
    http.begin(client, "http://aiotrainpredict.glitch.me/add-data");
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);  // Set timeout to 10 seconds

    int httpResponseCode = http.POST(jsonData);
    
    if (httpResponseCode > 0) {
      Serial.println("HTTP response code: " + String(httpResponseCode));
      String response = http.getString();
      Serial.println("Response: " + response);
      success = true;
    } else {
      Serial.println("Error sending data: " + http.errorToString(httpResponseCode));
      retryCount++;
      delay(2000);  // Wait 2 seconds before retrying
    }

    http.end();
  }

  if (!success) {
    Serial.println("Failed to send data after 3 attempts.");
  }
}

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 30000) {
      Serial.println("\nFailed to connect to WiFi. Restarting...");
      ESP.restart();
    }
  }
  Serial.println("\nWiFi connected");
}

void reconnectWiFi() {
  Serial.println("Attempting to reconnect to WiFi...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 15000) {
      Serial.println("\nFailed to reconnect to WiFi. Exiting reconnect loop.");
      return;
    }
  }
  Serial.println("\nReconnected to WiFi.");
}
