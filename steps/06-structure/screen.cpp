#include "screen.h"
#include "config.h"
#include "state.h"
#include "sensors.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

static const int COL_VALUE = 48;   // 값 시작 x (라벨 8글자 × 6px)

// ===== 공통 그리기 =====
static void drawHeader(const char* title, int page) {
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print(title);

  // 오른쪽 위에 페이지 번호 "1/3" (3글자 × 6px = 18px)
  oled.setCursor(SCREEN_W - 18, 0);
  oled.printf("%d/%d", page + 1, PAGE_COUNT);

  oled.drawLine(0, 10, SCREEN_W - 1, 10, SSD1306_WHITE);
}

static void drawRow(int y, const char* label) {
  oled.setCursor(0, y);
  oled.print(label);
  oled.setCursor(COL_VALUE, y);
}

// ===== 페이지 1: 방 상태 =====
static void drawStatusPage() {
  drawHeader("ROOM STATUS", 0);

  drawRow(14, "Temp/Hum");
  if (state.dhtOk) oled.printf("%.1fC %.0f%%", state.temperature, state.humidity);
  else             oled.print("ERROR");

  drawRow(26, "Light");
  oled.printf("%3d%% %s", state.lightPct, lightLabel(state.lightPct));

  drawRow(38, "Motion");
  if (state.motion) {
    oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    oled.print(" DETECTED ");
    oled.setTextColor(SSD1306_WHITE);
  } else {
    oled.print("none");
  }

  drawRow(50, "Object");
  oled.printf("%s  #%d", state.obstacle ? "NEAR" : "clear", state.objectCount);
}

// ===== 페이지 2: 온습도 상세 =====
static void drawClimatePage() {
  drawHeader("CLIMATE", 1);

  if (!state.dhtOk) {
    oled.setTextSize(2);
    oled.setCursor(0, 24);
    oled.print("SENSOR");
    oled.setCursor(0, 44);
    oled.print("ERROR");
    return;
  }

  oled.setTextSize(2);
  oled.setCursor(0, 16);
  oled.printf("%.1f C", state.temperature);
  oled.setCursor(0, 36);
  oled.printf("%.1f %%", state.humidity);

  oled.setTextSize(1);
  oled.setCursor(0, 56);
  oled.printf("Feels like %.1f C", state.heatIndex);
}

// ===== 페이지 3: 시스템 정보 =====
static void drawSystemPage() {
  drawHeader("SYSTEM", 2);

  unsigned long sec = millis() / 1000;
  drawRow(14, "Uptime");
  oled.printf("%02lu:%02lu:%02lu", sec / 3600, (sec / 60) % 60, sec % 60);

  drawRow(26, "DHT err");
  oled.printf("%d", state.dhtErrorCount);

  drawRow(38, "Objects");
  oled.printf("%d", state.objectCount);

  drawRow(50, "Heap");
  oled.printf("%u KB", ESP.getFreeHeap() / 1024);
}

// ===== 외부 공개 함수 =====
bool screenBegin() {
  Wire.begin(PIN_SDA, PIN_SCL);
  return oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
}

void screenShowBoot() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("Smart Room Monitor");
  oled.println("Starting...");
  oled.display();
}

void screenDraw(int page) {
  oled.clearDisplay();
  switch (page) {
    case 0:  drawStatusPage();  break;
    case 1:  drawClimatePage(); break;
    default: drawSystemPage();  break;
  }
  oled.display();
}
