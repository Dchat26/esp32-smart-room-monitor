#include "actuators.h"
#include "config.h"
#include "state.h"

void setLed(bool on) {
  state.ledOn = on;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
  Serial.printf("LED %s\n", on ? "ON" : "OFF");
}

void setRelay(bool on) {
  state.relayOn = on;
  bool level = RELAY_ACTIVE_LOW ? !on : on;   // Active Low면 신호를 반대로
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
  Serial.printf("Relay %s\n", on ? "ON" : "OFF");
}

void actuatorsBegin() {
  // 핀을 출력으로 바꾸기 전에 "꺼짐" 신호를 먼저 써 둠 (부팅 순간 릴레이가 튀지 않게)
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  setLed(false);
  setRelay(false);
}
