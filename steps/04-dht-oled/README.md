# 04. 온습도 센서 + OLED 표시 (DHT + OLED)

DHT 센서로 온도·습도를 읽고, I2C 통신으로 연결한 OLED에 온도·습도·체감온도를 표시하는 예제

- **시뮬레이션:** [Wokwi에서 실행하기](https://wokwi.com/projects/481656350103809025)
- **언어:** C++ (Arduino framework)
- **라이브러리:** DHT sensor library (Adafruit), Adafruit Unified Sensor, Adafruit SSD1306, Adafruit GFX Library

---

## 회로

### DHT22 (시뮬레이션) / DHT11 (실물)
| 센서 핀 | ESP32 |
|---|---|
| VCC | 3V3 |
| DATA (Wokwi 표기: SDA) | GPIO4 |
| GND | GND |

> Wokwi DHT22의 데이터 핀 이름은 `SDA`지만 I2C와 무관한 DHT 전용 데이터 핀이다.
> 실물 DHT11 **모듈**은 풀업 저항이 기판에 내장되어 있다. 센서 단품(다리 4개)은 DATA–VCC 사이에 10kΩ 풀업 저항이 필요하다.

### OLED (SSD1306, 128×64, I2C)
| OLED 핀 | ESP32 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO22 |
| SDA | GPIO21 |

### DHT11과 DHT22
Wokwi에는 DHT11이 없어 DHT22로 실습했다. 통신 방식이 같아 `DHT_TYPE` 한 줄만 바꾸면 실물 DHT11에서 동작한다.

| | DHT11 (키트) | DHT22 (Wokwi) |
|---|---|---|
| 온도 범위 / 오차 | 0~50°C / ±2°C | -40~80°C / ±0.5°C |
| 습도 범위 / 오차 | 20~90% / ±5% | 0~100% / ±2~5% |
| 최소 읽기 간격 | 1초 | 2초 |

두 센서 모두 대응하도록 **2초 간격**으로 읽는다.

---

## 실습 A — 온습도 읽어서 시리얼 출력

```cpp
#include <DHT.h>

#define DHT_PIN  4
#define DHT_TYPE DHT22     // 실물 DHT11이면 DHT11

DHT dht(DHT_PIN, DHT_TYPE);

const unsigned long READ_INTERVAL = 2000;
unsigned long lastRead = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
}

void loop() {
  if (millis() - lastRead >= READ_INTERVAL) {
    lastRead = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();   // 섭씨

    if (isnan(h) || isnan(t)) {        // 읽기 실패 확인
      Serial.println("DHT read failed");
      return;
    }
    Serial.printf("Temp: %.1f C   Humidity: %.1f %%\n", t, h);
  }
}
```

### 새로 사용한 문법
| 문법 | 설명 |
|---|---|
| `#include <...>` | 라이브러리 불러오기 |
| `#define` | 이름에 값을 붙임. 컴파일 전에 글자 그대로 치환된다 |
| `DHT dht(...)` | 라이브러리 **객체** 생성. 이후 `dht.함수()`로 센서 제어 |
| `float` | 소수점이 있는 숫자 타입 |
| `isnan()` | 값이 NaN(Not a Number)인지 확인. 센서 읽기 실패 시 NaN 반환 |
| `%.1f` | 소수를 소수점 한 자리까지 출력 |

### DHT 통신
- DHT는 **데이터 선 하나**로 정해진 타이밍에 맞춰 신호를 주고받는 자체 통신 방식을 쓴다.
- 센서 내부 측정에 시간이 걸려, 너무 자주 읽으면 이전 값이 나오거나 실패한다.
- 복잡한 타이밍 처리는 라이브러리가 대신하고, 코드에서는 `readTemperature()`, `readHumidity()`만 호출한다.

---

## 실습 B — OLED에 글자 띄우기

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);                      // I2C 시작 (SDA, SCL)

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
    while (true) delay(1000);
  }

  display.clearDisplay();                  // 버퍼 지우기
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Hello, ESP32!");
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println("OLED OK");
  display.display();                       // 버퍼 → 화면 전송
}

void loop() {}
```

### I2C 통신
선 **2개**로 여러 장치와 통신하는 방식.

| 선 | 역할 |
|---|---|
| **SDA** (Serial Data) | 데이터가 오가는 선 |
| **SCL** (Serial Clock) | 데이터를 읽을 타이밍(박자)을 맞추는 선 |

- 장치마다 **주소**가 있어 같은 두 선에 여러 장치를 연결해도 구분할 수 있다. 이 OLED의 주소는 `0x3C`.
- ESP32의 기본 I2C 핀은 GPIO21(SDA), GPIO22(SCL).

### 버퍼 방식
- `print()`, `drawLine()` 등은 화면에 바로 그리지 않고 ESP32 메모리의 **가상 화면(버퍼)**에 그린다.
- `display.display()`를 호출해야 버퍼 전체가 실제 화면으로 전송된다.
- 흐름: `clearDisplay()` → 그리기 → `display()`

---

## 화면 좌표와 레이아웃

### 좌표계
- 그래픽 좌표는 **(x, y) = (열, 행)** 순서. 행렬·배열의 (행, 열)과 반대다.
- 왼쪽 위가 (0, 0), 오른쪽 아래가 (127, 63). **y는 아래로 갈수록 커진다** (수학 좌표와 반대).

```
(0,0) ─────────────→ x (127)
  │
  │
  ↓ y (63)
```

- `setCursor(x, y)` = "오른쪽으로 x칸, 아래로 y칸"
- 배열로 다룰 때는 `buffer[y][x]`처럼 행(y)이 앞에 온다. 순서 실수가 잦은 부분.

### 글자 크기 규격
기본 글꼴은 크기 1에서 한 글자가 **가로 6px × 세로 8px** (글자 5×7 + 여백 1px). `setTextSize(n)`은 n배 확대.

| 크기 | 한 글자 | 한 줄 최대 | 최대 줄 수 |
|---|---|---|---|
| 1 | 6 × 8 | 21자 | 8줄 |
| 2 | 12 × 16 | 10자 | 4줄 |
| 3 | 18 × 24 | 7자 | 2줄 |

```
글자 폭  = 글자 수 × 6 × 크기
글자 높이 = 8 × 크기
가운데 정렬 x = (128 − 글자 폭) ÷ 2
오른쪽 정렬 x = 128 − 글자 폭
```

### 이 화면의 배치
| 요소 | 크기 | y 시작 | 높이 | y 끝 |
|---|---|---|---|---|
| 제목 `Smart Room Monitor` | 1 | 0 | 8 | 7 |
| 구분선 | - | 10 | 1 | 10 |
| 온도 | 2 | 16 | 16 | 31 |
| 습도 | 2 | 36 | 16 | 51 |
| 체감온도 | 1 | 56 | 8 | 63 |

요소 사이에 2~5px 여백을 두고, 마지막 줄이 화면 끝(63)을 넘지 않게 배치했다.

### 글자 수가 바뀌는 값 가운데 정렬
```cpp
void printCentered(const char* text, int y, int size) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);  // 실제 폭·높이 측정
  display.setCursor((SCREEN_W - w) / 2, y);
  display.print(text);
}
```

### 레이아웃 작업 순서
1. 128×64 모눈에 대략 스케치
2. 공식으로 좌표 계산, 좌표는 상수로 관리
3. Wokwi에서 확인하며 몇 px씩 미세 조정
4. `-10.5 C`, `100.0 %`처럼 **가장 긴 값**으로 잘림 여부 테스트

> 실물 0.96인치 OLED 중 **두 가지 색 버전**은 위쪽 16줄(y 0~15)이 노란색, 아래가 파란색으로 고정되어 있다. 위 16px에는 제목·상태, 아래에는 본문을 두는 것이 일반적이다. 이 화면은 제목과 구분선이 y 0~10, 본문이 y 16부터라 두 가지 색 OLED에서도 자연스럽게 나뉜다.

---

## 실습 C — 온습도를 OLED에 표시 (최종 코드)

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ===== 핀 / 설정 =====
#define DHT_PIN   4
#define DHT_TYPE  DHT22    // 실물 DHT11이면 DHT11
#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C

const unsigned long READ_INTERVAL = 2000;

// ===== 객체 =====
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ===== 상태 =====
unsigned long lastRead = 0;
float temperature = NAN;
float humidity = NAN;
float heatIndex = NAN;
bool sensorOk = false;
int errorCount = 0;

// ===== 센서 읽기 =====
void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    sensorOk = false;
    errorCount++;
    Serial.printf("DHT read failed (count: %d)\n", errorCount);
    return;
  }

  sensorOk = true;
  temperature = t;
  humidity = h;
  heatIndex = dht.computeHeatIndex(t, h, false);  // 체감온도 (false = 섭씨)

  Serial.printf("Temp: %.1f C  Humidity: %.1f %%  Feels: %.1f C\n",
                temperature, humidity, heatIndex);
}

// ===== 화면 그리기 =====
void drawScreen() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart Room Monitor");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  if (!sensorOk) {
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.print("SENSOR");
    display.setCursor(0, 44);
    display.print("ERROR");
    display.display();
    return;
  }

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.printf("%.1f C", temperature);
  display.setCursor(0, 36);
  display.printf("%.1f %%", humidity);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("Feels like %.1f C", heatIndex);

  display.display();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
    while (true) delay(1000);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Smart Room Monitor");
  display.println("Starting...");
  display.display();
}

void loop() {
  if (millis() - lastRead >= READ_INTERVAL) {
    lastRead = millis();
    readSensor();
    drawScreen();
  }
}
```

### 코드 구조
| 영역 | 내용 |
|---|---|
| 설정 | 핀 번호, 센서 종류, 화면 크기, 주소, 읽기 간격 |
| 객체 | `dht`, `display` |
| 상태 | 최근 측정값, 센서 정상 여부, 실패 횟수 |
| 기능 함수 | `readSensor()`: 읽고 상태 저장 / `drawScreen()`: 상태를 보고 그리기 |

- **읽기와 그리기 분리:** `readSensor()`는 상태 변수만 갱신하고, `drawScreen()`은 상태 변수만 보고 그린다. 이후 웹 대시보드를 붙일 때도 같은 상태 변수를 사용하면 된다.
- **실패 처리:** 읽기 실패 시 이전 값을 그대로 보여 주지 않고 `SENSOR ERROR`를 표시하며 실패 횟수를 기록한다.
- **논블로킹:** `millis()`로 2초마다 읽어, 그 사이에도 `loop()`가 멈추지 않는다.

### 체감온도 (Heat Index)
같은 온도라도 습도가 높으면 더 덥게 느껴지는 정도를 계산한 값. `computeHeatIndex()`로 계산하며, 실내 온도가 낮을 때는 실제 온도와 거의 같게 나온다.

---

## 실험

- [x] 실습 A: 2초마다 온도·습도 시리얼 출력
- [x] 실습 B: OLED에 글자 표시
- [x] 실습 C: 온도·습도·체감온도 OLED 표시, 슬라이더 조작 시 화면 갱신

### 일부러 망가뜨려 보기
| 실험 | 결과 | 확인한 개념 |
|---|---|---|
| `DHT_PIN`을 5로 변경 | `SENSOR ERROR` 표시, 시리얼에 실패 횟수 증가 | 센서 실패 처리 |
| `display.display()` 삭제 | 화면이 갱신되지 않음 | 버퍼 방식 |
| `OLED_ADDR`를 `0x3D`로 변경 | `OLED init failed` 출력 후 멈춤 | I2C 주소 |

- [ ] 실물 DHT11 모듈로 `DHT_TYPE`만 바꿔 동작 확인
- [ ] 실물 OLED가 단색인지 두 가지 색인지 확인

---

## 배운 점
- 라이브러리를 쓰면 센서·디스플레이의 복잡한 통신을 직접 구현하지 않고 함수 호출로 다룰 수 있다.
- I2C는 선 2개와 장치 주소로 여러 장치를 연결하는 통신 방식이다.
- OLED는 버퍼에 그린 뒤 한 번에 전송하는 방식이라 `display()` 호출이 필요하다.
- 그래픽 좌표는 (x, y) = (열, 행)이고 y는 아래로 증가한다. 글자 크기 규격으로 위치를 계산할 수 있다.
- 센서 읽기는 항상 실패할 수 있으므로 결과를 검사하고 실패 상태를 사용자에게 보여 줘야 한다.
- 읽기와 표시를 함수로 분리하면 기능을 추가할 때 수정 범위가 줄어든다.
