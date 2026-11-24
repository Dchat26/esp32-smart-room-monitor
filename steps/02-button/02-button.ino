const int LED_PIN = 16;
const int BTN_PIN = 19;
const unsigned long DEBOUNCE_MS = 50;

bool ledOn = false;
bool stableState = HIGH;
bool lastReading = HIGH; 
unsigned long lastChangeTime = 0;
int pressCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
}

void loop() {
  bool reading = digitalRead(BTN_PIN);

  if (reading != lastReading) {
    lastChangeTime = millis();
  }

  if (millis() - lastChangeTime > DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) { 
      ledOn = !ledOn;
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      pressCount++;
      Serial.print("press #");
      Serial.println(pressCount);
    }
  }
  lastReading = reading;
}
