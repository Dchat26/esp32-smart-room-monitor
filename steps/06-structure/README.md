# 06. 통합 구조 — 기능별 파일 분리 + 공통 디바운싱 (Structure)

05의 단일 파일 코드를 **역할별 모듈로 분리**하고, 버튼·적외선·PIR에 쓰는 **공통 디바운싱 클래스**를 만들어 05에서 확인한 바운싱 문제를 해결했다. 버튼으로 OLED 화면 페이지를 넘기는 기능을 추가했다.

- **시뮬레이션:** [Wokwi에서 실행하기](https://wokwi.com/projects/481660846077374465)
- **언어:** C++ (Arduino framework)
- **라이브러리:** DHT sensor library (Adafruit), Adafruit Unified Sensor, Adafruit SSD1306, Adafruit GFX Library

---

## 왜 파일을 나누는가

05 코드는 한 파일에 약 200줄이었다. 이후 Wi-Fi, 웹서버, 경보 기능까지 붙이면 500줄을 넘는데, 한 파일에 모두 있으면 원하는 코드를 찾기 어렵고 한 곳을 고치다 다른 기능을 망가뜨리기 쉽다.

## 파일 구조

```
06-structure/
├── 06-structure.ino   ← 시작점: setup(), loop() 스케줄러, 버튼 처리
├── config.h           ← 핀 번호, 주기, 설정값
├── state.h            ← 모든 모듈이 공유하는 센서 상태
├── debounce.h / .cpp  ← 디바운싱 클래스 (버튼, 적외선, PIR 공통)
├── sensors.h / .cpp   ← 센서 읽기
├── screen.h / .cpp    ← OLED 화면 그리기
├── diagram.json
└── libraries.txt
```

### 모듈 관계
```
            06-structure.ino (스케줄러)
           /        |         \
     sensors      screen     debounce (버튼)
        |   \       |
  debounce   \      |
  (PIR, IR)   ↘     ↓
              state (공유 상태)
                    ↑
                 config (설정)
```
- `sensors`는 `state`에 **쓰기만**, `screen`은 `state`를 **읽기만** 한다.
- 모든 모듈은 `config.h`의 설정값을 참조한다. 핀이나 주기를 바꿀 때 이 파일만 고치면 된다.

### `.h`와 `.cpp`
| 파일 | 역할 | 비유 |
|---|---|---|
| `.h` (헤더) | "이런 함수가 있다"는 **선언** | 메뉴판 |
| `.cpp` (소스) | 함수가 실제로 하는 일의 **구현** | 주방 |

다른 파일은 `#include "sensors.h"`로 선언만 보고 함수를 호출한다. 구현 내부는 몰라도 된다.

---

## 회로

05 회로에 버튼을 추가했다.

| 부품 | 핀 | 비고 |
|---|---|---|
| DHT | 4 | |
| OLED | 21 (SDA), 22 (SCL) | I2C |
| 조도 | 34 | ADC1 |
| PIR | 27 | 감지 시 HIGH |
| 적외선 (슬라이드 스위치) | 26 | 감지 시 LOW |
| **버튼** | **19** | 누르면 LOW, 페이지 전환 |

> 버튼은 **번호가 다른 핀 두 개(1번 하나 + 2번 하나)**에 연결한다. l/r(좌우)은 상관없다. 1.l과 1.r은 내부에서 같은 선이다.

| 조합 | 결과 |
|---|---|
| 1.l + 2.l / 1.r + 2.r / 1.l + 2.r / 2.l + 1.r | ✅ |
| 1.l + 1.r / 2.l + 2.r | ❌ 항상 연결된 상태 |

---

## config.h — 설정

```cpp
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
```

| 문법 | 설명 |
|---|---|
| `#pragma once` | 헤더가 여러 번 include돼도 한 번만 읽히게 함. 헤더 맨 위에 항상 넣는다 |
| `constexpr` | 컴파일 시점에 값이 정해지는 상수. `#define`과 달리 **타입이 있어** 실수를 컴파일러가 잡는다 |

- `DHT_TYPE`만 라이브러리 규칙 때문에 `#define`을 사용했다.
- **입력 주기를 100ms → 5ms로 줄였다.** 05에서는 바운싱 때문에 주기를 늘렸지만, 이제 디바운싱 클래스가 처리하므로 자주 읽어도 안전하다.
- 디바운싱 시간을 **입력 종류별로 따로** 둔다. 사람이 누르는 버튼과 물체가 지나가는 센서는 신호가 바뀌는 속도가 다르기 때문이다.

---

## state.h — 공유 상태

```cpp
#pragma once
#include <Arduino.h>

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
```

| 문법 | 설명 |
|---|---|
| `struct` | 관련된 변수를 하나로 묶은 구조체. `state.temperature`처럼 접근 |
| `extern` | "이 변수는 다른 파일에 실제로 있다"는 선언. 변수는 `.ino`에 하나만 만들고 다른 파일이 함께 쓴다 |

흩어져 있던 상태 변수를 한곳에 모아, 이후 웹으로 데이터를 보낼 때도 `state`만 넘기면 된다.

---

## debounce.h / .cpp — 공통 디바운싱 클래스

### debounce.h
```cpp
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
```

### debounce.cpp
```cpp
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
```

### 클래스
데이터(변수)와 그 데이터를 다루는 함수를 **한 덩어리로 묶은 설계도**. `DebouncedInput button;`처럼 만들면 버튼·적외선·PIR이 각자 **자기만의 타이머와 상태**를 가진다. 02에서 디바운싱용 변수 4~5개를 직접 만들던 것을 객체 하나로 해결한다.

| 문법 | 설명 |
|---|---|
| `public:` | 밖에서 쓸 수 있는 부분 |
| `private:` | 클래스 안에서만 쓰는 부분. 밖에서 실수로 건드릴 수 없다 |
| `DebouncedInput::update()` | `DebouncedInput` 클래스의 `update` 함수 정의 |
| `const` (함수 뒤) | 값을 바꾸지 않고 읽기만 하는 함수 |

### "활성(active)" 개념
버튼·적외선은 감지 시 LOW(Active Low), PIR은 HIGH다. `activeLow` 설정으로 이 차이를 클래스 안에서 처리하므로, 사용하는 쪽은 HIGH/LOW를 신경 쓰지 않고 **"감지됐나?"만 확인**하면 된다.

### 디바운싱 방식 비교

| 방법 | 기준 | 장점 | 단점 |
|---|---|---|---|
| 읽기 주기 늘리기 (05) | 정해진 시점에만 읽기 | 가장 단순 | 운이 나쁘면 튐, 빠른 입력을 놓침 |
| 잠금 (lockout) | 입력을 받은 뒤 일정 시간 무시 | 첫 입력에 즉시 반응 | 잠금 시간 안의 진짜 입력도 버림 |
| **안정 시간 (06)** | **값이 일정 시간 흔들림 없이 유지** | **흔들림을 확실히 걸러냄** | 디바운싱 시간만큼 반응이 늦음 |

```
신호:   HIGH ─┐┌┐┌─────────────────
             └┘└┘ LOW
타이머:       ↺↺↺↺ 0 → 10 → 20 → 30ms ✅ 인정
```

- 시간을 칸으로 나눠 횟수를 세는 것이 아니라, **마지막으로 흔들린 뒤 얼마나 조용했는지**를 잰다.
- 입력 횟수를 제한하지 않는다. 누름·뗌이 각각 디바운싱 시간 이상 유지되면 누른 횟수만큼 정확히 인식한다.

### 디바운싱 시간의 한계와 선택 기준
누름·뗌 상태가 **디바운싱 시간보다 짧게** 바뀌면 타이머가 계속 리셋되어 인정되지 않는다. 시스템 입장에서는 "계속 흔들리는 신호"와 "극단적으로 빠른 연타"를 구분할 수 없기 때문이다.

```
바운싱 시간  <  디바운싱 시간  <  가장 짧은 진짜 입력
(보통 수 ms~20ms)   (30~50ms)        (사람 클릭은 대개 이보다 김)
```

| 설정 | 문제 |
|---|---|
| 너무 짧게 | 흔들림을 못 걸러 여러 번 인식 |
| 너무 길게 | 빠른 진짜 입력까지 버림 |

---

## sensors.h / .cpp — 센서 읽기

### sensors.h
```cpp
#pragma once

void sensorsBegin();
void sensorsReadInputs();   // PIR, 적외선 (디바운싱 + 이벤트)
void sensorsReadLight();    // 조도
void sensorsReadDht();      // 온습도

const char* lightLabel(int pct);
```

### sensors.cpp
```cpp
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
```

- **`static` (파일 전역 변수·함수 앞):** 이 파일 안에서만 쓸 수 있게 숨긴다. `dht`, `pir`, `ir`는 센서 모듈 내부 사정이므로 다른 파일은 `sensorsReadDht()` 같은 함수로만 접근한다 (캡슐화).
- 05의 `lastMotion`, `lastObstacle` 변수가 사라졌다. 엣지 검출을 `DebouncedInput`이 대신한다.

---

## screen.h / .cpp — 화면

### 페이지 구성
| 페이지 | 제목 | 내용 |
|---|---|---|
| 1 | ROOM STATUS | 온습도, 조도, 움직임, 물체 (05 화면) |
| 2 | CLIMATE | 온도·습도 큰 글씨, 체감온도 |
| 3 | SYSTEM | 가동 시간, DHT 실패 횟수, 물체 누적, 남은 메모리 |

```
ROOM STATUS          1/3
─────────────────────────
Temp/Hum 24.0C 40%
Light     72% BRIGHT
Motion   [ DETECTED ]
Object   NEAR  #3
```

### screen.h
```cpp
#pragma once

bool screenBegin();
void screenShowBoot();
void screenDraw(int page);
```

### screen.cpp
```cpp
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
```

- 화면 객체 이름을 `display` → `oled`로 바꿔 함수 이름과 헷갈리지 않게 했다.
- `clearDisplay()`와 `display()`는 `screenDraw()` **한 곳에서만** 호출한다. 페이지 함수는 그리기만 하므로 페이지를 추가해도 빠뜨릴 일이 없다.
- `ESP.getFreeHeap()`: 남은 메모리(바이트). Wi-Fi·웹서버 추가 후 변화를 확인할 예정.

---

## 06-structure.ino — 시작점

```cpp
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
```

- `.ino`에는 **무엇을 언제 실행할지(스케줄)**만 남고, 실제 일은 각 모듈이 한다. 이 파일만 읽어도 전체 흐름을 알 수 있다.
- `% PAGE_COUNT`: 나머지 연산으로 마지막 페이지 다음에 0으로 돌아간다.
- `redrawNow`: 버튼을 누르자마자 화면을 다시 그린다. 없으면 최대 0.25초 늦게 바뀐다.

### 작업 스케줄
| 작업 | 주기 | 함수 |
|---|---|---|
| 버튼, PIR, 적외선 | 5ms | `sensorsReadInputs()`, `handleButton()` |
| 조도 | 200ms | `sensorsReadLight()` |
| 온습도 | 2초 | `sensorsReadDht()` |
| 화면 | 250ms (버튼 시 즉시) | `screenDraw()` |

---

## 실험

- [x] 컴파일 성공, 상태 화면과 페이지 번호(`1/3`) 표시
- [x] 버튼으로 `1/3 → 2/3 → 3/3 → 1/3` 한 칸씩 전환
- [x] SYSTEM 페이지 가동 시간 1초씩 증가

### 디바운싱 효과 검증 (변수 하나만 바꾸기)
| 실험 | `INPUT_INTERVAL` | `IR_DEBOUNCE_MS` | 결과 |
|---|---|---|---|
| 참고 | 5ms | 0 | 문제 없음 — 5ms 읽기 주기 자체가 짧은 흔들림을 건너뛰어 효과가 섞임 |
| ① | 1ms | 0 | 감지 1회에 **가끔 2번 카운트** ❌ |
| ② | 1ms | 30 | 항상 1번 카운트 ✅ |

- 읽기 주기와 디바운싱이 함께 영향을 주므로, **읽기 주기를 1ms로 고정하고 디바운싱 값만 바꿔** 비교했다.
- ①과 ②는 디바운싱 값만 다르므로, 결과 차이는 디바운싱 때문이라고 말할 수 있다.
- 바운싱은 확률적이므로 각 조건에서 여러 번 반복해 확인했다.
- → [트러블슈팅 4번](../../docs/troubleshooting.md#4-읽기-간격을-줄이자-물체-감지가-2번-카운트됨-바운싱) 해결

### 디바운싱 시간이 너무 길 때 (`BTN_DEBOUNCE_MS = 300`)
| 동작 | 결과 |
|---|---|
| 빠르게 클릭 | 페이지가 **아예 넘어가지 않음** |
| 300ms 이상 누르고 있음 | 다음 페이지로 넘어감 |
| 넘어간 뒤 바로 다시 누름 | 인식 안 됨. 잠시 기다렸다 눌러야 함 |

```
빠른 클릭:   누름 ──(200ms)── 뗌      → 300ms를 못 채움 → 무시
길게 누름:   누름 ──(300ms)── ✅ 인정
바로 다시:   뗌 ─(100ms)─ 누름        → 뗀 상태가 300ms를 못 채워 확정되지 않음
                                       → 시스템은 계속 누른 상태로 판단
```

- 디바운싱은 **누를 때와 뗄 때 모두** 적용된다. 다음 누름이 인정되려면 뗀 상태도 디바운싱 시간 이상 유지되어야 한다.

### 50ms에서도 가끔 넘어가지 않는 경우
- 시뮬레이션 속도는 평균 **100%**로 Wokwi 환경 문제는 아니었다.
- 화면이 바뀌는 것을 보고 누르면 바로바로 넘어가지만, **화면이 바뀌기 전에 다시 누르면 인식되지 않았다.**

```
누름 ─── 50ms 대기 ─── ✅ 인정 ─── 화면 그리기 (loop 멈춤) ─── 화면 바뀜
                                  └──── 이 사이에 다시 누르면 놓침 ────┘
```

| 원인 | 설명 |
|---|---|
| 뗀 상태 유지 시간 부족 | 화면이 바뀌기 전에 다시 누르면 뗐다 누르는 간격이 50ms보다 짧아, 계속 누른 것으로 판단 |
| 화면 그리기 중 입력 못 읽음 | `oled.display()`가 I2C로 1024바이트를 보내는 동안 `loop()`가 멈춘다. 그 사이에 뗐다 누르기를 모두 끝내면 뗀 순간을 한 번도 보지 못한다 |

**판단:** 화면 변화를 보고 다음을 누르는 일반적인 사용에서는 문제가 없다. 화면이 바뀌기 전 연타는 사람이 누르는 속도의 한계에 가깝다.

**개선 선택지**
| 방법 | 효과 | 난이도 |
|---|---|---|
| `BTN_DEBOUNCE_MS`를 30ms로 | 뗀 상태 인정이 빨라져 첫 번째 원인 감소 | 쉬움 |
| 화면 그리는 시간 줄이기 (값이 바뀔 때만 그리기 등) | 입력을 못 읽는 구간 단축 | 보통 |
| 잠금 방식 디바운싱 | 첫 변화에 즉시 반응. 반응 속도가 중요한 버튼에 적합 | 보통 |
| 인터럽트(`attachInterrupt`)로 버튼 읽기 | `loop()`가 멈춰 있어도 핀 변화를 놓치지 않음 | 어려움 |

> **폴링 vs 인터럽트:** 지금은 주기적으로 핀을 확인하는 **폴링** 방식이다. **인터럽트**는 핀 신호가 바뀌는 순간 하던 일을 잠깐 멈추고 정해 둔 함수를 바로 실행해 변화를 놓치지 않지만, 다루기가 까다롭다.

### 측정 예정: 화면 그리기 시간
감으로 값을 바꾸지 않고, 화면 그리기가 `loop()`를 얼마나 멈추는지 먼저 측정한다.

```cpp
unsigned long t0 = micros();              // 시작 시각 (마이크로초)
screenDraw(currentPage);
unsigned long dt = micros() - t0;         // 걸린 시간

static unsigned long maxDraw = 0;         // 지금까지의 최댓값 (함수가 끝나도 유지)
if (dt > maxDraw) {
  maxDraw = dt;
  Serial.printf("screenDraw max: %lu us\n", maxDraw);
}
```

- [ ] `screenDraw()` 최대 소요 시간 측정
- [ ] `BTN_DEBOUNCE_MS`를 30ms로 줄여 체감 비교
- [ ] 실물 버튼으로 동일하게 확인

---

## 05 → 06 변화 요약

| 항목 | 05 | 06 |
|---|---|---|
| 파일 | 1개 | 역할별 6개 모듈 |
| 설정값 | 코드 곳곳 | `config.h` 한곳 |
| 상태 변수 | 흩어진 전역 변수 | `SensorState` 구조체 |
| 디바운싱 | 읽기 주기 100ms로 우회 | 안정 시간 디바운싱 클래스 |
| 입력 읽기 주기 | 100ms | 5ms (반응 속도 향상) |
| 엣지 검출 | `last...` 변수 직접 관리 | `activated()` / `deactivated()` |
| 화면 | 1페이지 | 버튼으로 3페이지 전환 |

---

## 배운 점
- 코드를 역할별 모듈로 나누면 찾기 쉽고, 한 기능을 고칠 때 다른 기능에 미치는 영향이 줄어든다.
- 헤더(선언)와 소스(구현)를 나누고 `static`으로 내부를 숨기면, 다른 모듈은 공개된 함수로만 접근하게 된다 (캡슐화).
- 반복되는 로직을 클래스로 만들면 같은 기능을 여러 입력에 각자의 상태로 재사용할 수 있다.
- 디바운싱 방식마다 장단점이 있고, 디바운싱 시간은 바운싱 시간과 실제 입력 속도 사이에서 정해야 한다.
- 원인을 검증할 때는 변수를 하나만 바꾸고 나머지는 고정해야 결과를 확실히 해석할 수 있다.
- 디바운싱은 누름과 뗌 모두에 적용되므로, 값이 길수록 연속 입력이 어려워진다.
- 폴링 방식에서는 `loop()`를 멈추는 작업(화면 전송 등)이 입력 반응성에 직접 영향을 준다. 값을 조정하기 전에 먼저 측정해 원인을 확인한다.
