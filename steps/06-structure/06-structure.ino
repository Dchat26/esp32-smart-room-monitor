#include "config.h"
#include "state.h"
#include "debounce.h"
#include "sensors.h"
#include "screen.h"

// ===== 전역 상태 (state.h의 extern이 가리키는 실제 변수) =====
SensorState state;

// ===== 입력 =====
DebouncedInput button;
int currentPage = 0;
bool redrawNow = false;

// ===== 타이머 =====
unsigned long lastInput = 0, lastLight = 0, lastDht = 0, lastScreen = 0;

void handleButton() {
  button.update();
  if (button.activated()) {
    currentPage = (currentPage + 1) % PAGE_COUNT;   // 0 → 1 → 2 → 0
    redrawNow = true;                               // 페이지 바뀌면 바로 그리기
    Serial.printf("Page -> %d\n", currentPage + 1);
  }
}

void setup() {
  Serial.begin(115200);

  sensorsBegin();
  button.begin(PIN_BTN, true, BTN_DEBOUNCE_MS, INPUT_PULLUP);

  if (!screenBegin()) {
    Serial.println("OLED init failed");
    while (true) delay(1000);
  }
  screenShowBoot();
}

void loop() {
  unsigned long now = millis();

  if (now - lastInput >= INPUT_INTERVAL) {
    lastInput = now;
    sensorsReadInputs();
    handleButton();
  }
  if (now - lastLight >= LIGHT_INTERVAL) {
    lastLight = now;
    sensorsReadLight();
  }
  if (now - lastDht >= DHT_INTERVAL) {
    lastDht = now;
    sensorsReadDht();
  }
  if (redrawNow || now - lastScreen >= SCREEN_INTERVAL) {
    lastScreen = now;
    redrawNow = false;
    screenDraw(currentPage);
  }
}
