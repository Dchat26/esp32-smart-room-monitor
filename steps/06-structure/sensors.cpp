#include "sensors.h"
#include "config.h"
#include "state.h"
#include "debounce.h"
#include <DHT.h>

// 이 파일 안에서만 쓰는 객체 (static)
static DHT dht(PIN_DHT, DHT_TYPE);
static DebouncedInput pir;
static DebouncedInput ir;

static int toLightPercent(int raw) {
  int p = map(raw, 0, 4095, 0, 100);
  return LDR_BRIGHT_IS_LOW ? 100 - p : p;
}

const char* lightLabel(int pct) {
  if (pct >= 60) return "BRIGHT";
  if (pct >= 25) return "DIM";
  return "DARK";
}

void sensorsBegin() {
  dht.begin();
  pir.begin(PIN_PIR, false, PIR_DEBOUNCE_MS, INPUT);       // 감지 시 HIGH
  ir.begin(PIN_IR, true, IR_DEBOUNCE_MS, INPUT_PULLUP);    // 감지 시 LOW
}

void sensorsReadInputs() {
  pir.update();
  ir.update();

  state.motion = pir.isActive();
  if (pir.activated())   Serial.println("Motion detected");
  if (pir.deactivated()) Serial.println("Motion ended");

  state.obstacle = ir.isActive();
  if (ir.activated()) {
    state.objectCount++;
    Serial.printf("Object detected #%d\n", state.objectCount);
  }
}

void sensorsReadLight() {
  state.lightPct = toLightPercent(analogRead(PIN_LDR));
}

void sensorsReadDht() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    state.dhtOk = false;
    state.dhtErrorCount++;
    Serial.printf("DHT read failed (count: %d)\n", state.dhtErrorCount);
    return;
  }

  state.dhtOk = true;
  state.temperature = t;
  state.humidity = h;
  state.heatIndex = dht.computeHeatIndex(t, h, false);
}
