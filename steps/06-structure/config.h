#pragma once
#include <Arduino.h>

// ===== 핀 =====
constexpr int PIN_DHT = 4;
constexpr int PIN_LDR = 34;
constexpr int PIN_PIR = 27;
constexpr int PIN_IR  = 26;
constexpr int PIN_BTN = 19;
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;

// ===== 센서 =====
#define DHT_TYPE DHT22                    // 실물 DHT11이면 DHT11
constexpr bool LDR_BRIGHT_IS_LOW = true;  // 밝을수록 값이 작아지는 모듈

// ===== 화면 =====
constexpr int SCREEN_W = 128;
constexpr int SCREEN_H = 64;
constexpr uint8_t OLED_ADDR = 0x3C;
constexpr int PAGE_COUNT = 3;

// ===== 작업 주기 (ms) =====
constexpr unsigned long INPUT_INTERVAL  = 5;     // 버튼, PIR, 적외선
constexpr unsigned long LIGHT_INTERVAL  = 200;   // 조도
constexpr unsigned long DHT_INTERVAL    = 2000;  // 온습도
constexpr unsigned long SCREEN_INTERVAL = 250;   // 화면 갱신

// ===== 디바운싱 (ms) =====
constexpr unsigned long BTN_DEBOUNCE_MS = 50;
constexpr unsigned long IR_DEBOUNCE_MS  = 30;
constexpr unsigned long PIR_DEBOUNCE_MS = 10;
