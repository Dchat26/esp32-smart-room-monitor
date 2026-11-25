#include "debounce.h"

void DebouncedInput::begin(int p, bool aLow, unsigned long ms, uint8_t mode) {
  pin = p;
  activeLow = aLow;
  debounceMs = ms;
  pinMode(pin, mode);

  lastReading = digitalRead(pin);
  stableReading = lastReading;
  active = activeLow ? (stableReading == LOW) : (stableReading == HIGH);
}

void DebouncedInput::update() {
  justActivated = false;            // 이벤트는 한 번의 update 동안만 유효
  justDeactivated = false;

  bool reading = digitalRead(pin);

  // 값이 흔들리면 타이머 리셋
  if (reading != lastReading) {
    lastChangeTime = millis();
    lastReading = reading;
  }

  // 일정 시간 유지된 변화만 인정
  if (millis() - lastChangeTime >= debounceMs && reading != stableReading) {
    stableReading = reading;
    bool nowActive = activeLow ? (reading == LOW) : (reading == HIGH);

    if (nowActive && !active)  justActivated = true;
    if (!nowActive && active)  justDeactivated = true;
    active = nowActive;
  }
}
