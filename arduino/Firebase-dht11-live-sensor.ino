#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>  // ✅ Include secure client
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11

const char* ssid = "project";
const char* password = "12345678";

const char* firebaseHost = "https://live-sensor-default-rtdb.firebaseio.com/sensordata.json";

DHT dht(DHTPIN, DHTTYPE);

// 🔒 Use secure client for HTTPS
WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("WiFi connected");
  dht.begin();

  // 👇 Skip certificate verification (for demo only)
  client.setInsecure();
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(2000);
    return;
  }

  String jsonData = "{\"sensorid\":\"DHT-11\",\"samplename\":\"Weather\",\"temp\":" + 
                    String(temperature) + ",\"hum\":" + String(humidity) + "}";

  HTTPClient http;
  http.begin(client, firebaseHost);  // 🔒 use secure client
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT(jsonData);

  if (httpResponseCode > 0) {
    Serial.println("HTTP response code: " + String(httpResponseCode));
    Serial.println(http.getString());
  } else {
    Serial.println("Error sending data to Firebase");
  }

  http.end();
  delay(5000);
}
