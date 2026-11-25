#pragma once
#include <Arduino.h>

class DebouncedInput {
public:
  void begin(int pin, bool activeLow, unsigned long debounceMs, uint8_t mode);
  void update();                                      // 주기적으로 호출

  bool isActive()    const { return active; }          // 지금 활성 상태인가
  bool activated()   const { return justActivated; }   // 이번 update에서 막 활성화됐나
  bool deactivated() const { return justDeactivated; } // 이번 update에서 막 해제됐나

private:
  int  pin = -1;
  bool activeLow = true;
  unsigned long debounceMs = 50;

  bool lastReading = HIGH;          // 직전에 읽은 원본 값
  bool stableReading = HIGH;        // 확정된 원본 값
  unsigned long lastChangeTime = 0;

  bool active = false;
  bool justActivated = false;
  bool justDeactivated = false;
};
