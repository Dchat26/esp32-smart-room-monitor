#pragma once
#include <Arduino.h>

// 모든 모듈이 공유하는 센서 상태
struct SensorState {
  float temperature = NAN;
  float humidity    = NAN;
  float heatIndex   = NAN;
  bool  dhtOk       = false;
  int   dhtErrorCount = 0;

  int   lightPct    = 0;

  bool  motion      = false;
  bool  obstacle    = false;
  int   objectCount = 0;
};

extern SensorState state;   // 실제 변수는 .ino에 있음
