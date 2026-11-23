const int LED_PIN = 16;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("ON");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  Serial.println("OFF");
  delay(500);
}
