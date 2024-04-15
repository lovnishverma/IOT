const int pirPin = 5;    // PIR sensor pin (GPIO 5 / D1)
const int relayPin = 4;  // Relay module pin (GPIO 4 / D2)
const int buzzerPin = 3; // Buzzer pin (Choose appropriate pin)

unsigned long lastMotionTime = 0;
const unsigned long motionDebounceTime = 2000; // Motion debounce time in milliseconds
const unsigned long lightsOnDuration = 60000;  // Lights on duration in milliseconds

bool lightsActivated = false;

void setup() {
  pinMode(pirPin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(relayPin, LOW); // Initialize relay to off state
  digitalWrite(buzzerPin, LOW); // Initialize buzzer to off state

  Serial.begin(9600);
  Serial.println("Office Bathroom Lights Control");
}

void loop() {
  int pirState = digitalRead(pirPin);

  if (pirState == HIGH) {
    if (!lightsActivated) {
      lightsActivated = true;
      Serial.println("Motion detected! Turning on lights.");
      activateLights();
      activateBuzzer(1000); // Activate buzzer with frequency 1000Hz
    }
    lastMotionTime = millis(); // Reset the timer whenever motion is detected
  } else {
    if (millis() - lastMotionTime >= motionDebounceTime && lightsActivated) {
      if (millis() - lastMotionTime >= lightsOnDuration) {
        lightsActivated = false;
        deactivateLights();
        activateBuzzer(2000); // Activate buzzer with frequency 2000Hz
      }
    }
  }
}

void activateLights() {
  digitalWrite(relayPin, LOW); // Turn off the relay
  Serial.println("Lights Activated.");

  // No need for delay here
}

void deactivateLights() {
  digitalWrite(relayPin, HIGH);  // Turn on the relay
  Serial.println("No motion detected. Lights Deactivated.");
}

void activateBuzzer(int frequency) {
  tone(buzzerPin, frequency); // Activate buzzer with given frequency
  Serial.println("Buzzer Activated.");
  delay(200); // Buzzer activation duration
  noTone(buzzerPin); // Turn off buzzer
}
