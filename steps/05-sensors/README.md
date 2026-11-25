# 05. 조도·PIR·적외선 센서 + OLED 상태 화면 (Sensors)

온습도(04)에 조도, 움직임, 물체 감지를 더해 방 상태를 한 화면에 보여 주는 모니터.
센서마다 **읽는 주기를 다르게** 돌리는 구조를 처음 적용했다.

- **시뮬레이션:** [Wokwi에서 실행하기](WOKWI_LINK)
- **언어:** C++ (Arduino framework)
- **라이브러리:** DHT sensor library (Adafruit), Adafruit Unified Sensor, Adafruit SSD1306, Adafruit GFX Library

---

## 회로

04 회로(DHT → GPIO4, OLED → GPIO21/22)에 아래를 추가했다.

### 조도 센서 모듈
| 모듈 핀 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| AO (아날로그 출력) | GPIO34 (ADC1, 입력 전용) |
| DO (디지털 출력) | 사용 안 함 |

### PIR 움직임 센서 (HC-SR501)
| 센서 핀 | ESP32 |
|---|---|
| VCC | 3V3 (Wokwi) / **5V(VIN)** (실물) |
| OUT | GPIO27 |
| GND | GND |

### 적외선 장애물 센서 → 슬라이드 스위치로 대체
Wokwi에는 적외선 장애물 센서가 없다. 실물 센서는 **물체가 가까이 오면 OUT이 LOW**가 되므로, 슬라이드 스위치로 같은 동작을 흉내 냈다.

| 스위치 핀 | ESP32 |
|---|---|
| 2 (가운데) | GPIO26 |
| 1 | GND |
| 3 | 연결 안 함 |

스위치를 1번 쪽으로 밀면 GPIO26이 GND에 연결되어 LOW = 물체 감지.

### 전체 핀 배치
| 부품 | 핀 | 신호 |
|---|---|---|
| DHT | 4 | 디지털 (DHT 전용 통신) |
| OLED | 21 (SDA), 22 (SCL) | I2C |
| 조도 | 34 | 아날로그 (ADC1) |
| PIR | 27 | 디지털 |
| 적외선 | 26 | 디지털 (Active Low) |

> 아날로그 입력인 조도 센서만 ADC1 핀(34)에 연결했다. 나머지는 디지털 신호라 ADC2 핀(4, 26, 27)이어도 Wi-Fi 사용 시 문제없다.

### 실물 센서 참고
| 센서 | 동작 | 주의사항 |
|---|---|---|
| 조도 센서 모듈 | 밝기에 따라 AO 전압이 바뀜 | 모듈의 가변저항은 DO 기준값만 바꾸고 AO에는 영향 없음 |
| HC-SR501 (PIR) | 체온(적외선) 변화를 감지하면 OUT이 HIGH | VCC는 5V. 출력은 3.3V라 ESP32에 바로 연결 가능. 전원 후 약 1분 안정화, 그동안 오작동 가능. 가변저항 2개로 감도와 HIGH 유지 시간 조절 |
| 적외선 장애물 센서 | 적외선을 쏘고 반사되면 OUT이 LOW | 가변저항으로 감지 거리 조절(대략 2~30cm). 강한 햇빛에서 오작동 가능 |

---

## 실습 A — 조도 값 읽고 분류하기

```cpp
#define LDR_PIN 34

const bool LDR_BRIGHT_IS_LOW = true;   // 밝을수록 값이 작아지면 true
unsigned long lastPrint = 0;

// 0~4095 값을 "밝기 %"로 변환 (100 = 가장 밝음)
int lightPercent(int raw) {
  int p = map(raw, 0, 4095, 0, 100);
  return LDR_BRIGHT_IS_LOW ? 100 - p : p;
}

const char* lightLabel(int pct) {
  if (pct >= 60) return "BRIGHT";
  if (pct >= 25) return "DIM";
  return "DARK";
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (millis() - lastPrint > 300) {
    lastPrint = millis();
    int raw = analogRead(LDR_PIN);
    int pct = lightPercent(raw);
    Serial.printf("raw: %4d  light: %3d%%  %s\n", raw, pct, lightLabel(pct));
  }
}
```

### 원리
- 포토레지스터(CdS)는 **빛이 강할수록 저항이 작아진다.**
- 모듈 안에서 고정 저항과 **전압 분배기**를 이뤄(03의 가변저항과 같은 원리), 밝기에 따라 AO 전압이 바뀐다.
- 이 모듈은 **밝을수록 `raw` 값이 작아진다.** 그래서 `LDR_BRIGHT_IS_LOW = true`로 값을 뒤집어 "밝을수록 큰 %"로 변환했다.
- 센서 원본 값을 바로 쓰지 않고 **의미 있는 단위(밝기 %)로 변환**해 두면, 센서 방향이 달라도 변환 함수 한 곳만 고치면 된다.

| 설정 | 결과 |
|---|---|
| `LDR_BRIGHT_IS_LOW = true` | 밝게 하면 `BRIGHT` (정상) |
| `LDR_BRIGHT_IS_LOW = false` | 밝게 하면 `DARK` (반대로 표시) |

### `const char*`
문자열을 돌려주는 함수의 반환 타입. `"BRIGHT"` 같은 고정 문자열이 저장된 위치를 돌려준다.

---

## 실습 B — 움직임과 물체 감지 이벤트

```cpp
#define PIR_PIN 27
#define IR_PIN  26

bool motion = false, lastMotion = false;
bool obstacle = false, lastObstacle = false;
int objectCount = 0;
unsigned long lastRead = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);          // PIR 모듈이 직접 HIGH/LOW 출력
  pinMode(IR_PIN, INPUT_PULLUP);    // 감지되면 LOW (Active Low)
}

void loop() {
  if (millis() - lastRead >= 100) {   // 0.1초마다 확인
    lastRead = millis();

    // 움직임: 시작과 끝을 각각 감지
    motion = (digitalRead(PIR_PIN) == HIGH);
    if (motion && !lastMotion)  Serial.println("Motion detected");
    if (!motion && lastMotion)  Serial.println("Motion ended");
    lastMotion = motion;

    // 물체: 감지되는 순간만 카운트
    obstacle = (digitalRead(IR_PIN) == LOW);
    if (obstacle && !lastObstacle) {
      objectCount++;
      Serial.printf("Object detected #%d\n", objectCount);
    }
    lastObstacle = obstacle;
  }
}
```

### 원리
- 02의 **엣지 검출**을 그대로 적용했다.
  - PIR: 시작(상승 엣지)과 끝(하강 엣지)을 모두 감지
  - 물체: 감지되는 순간(하강 엣지)만 카운트
- PIR 모듈은 한 번 감지하면 **정해진 시간 동안 HIGH를 유지**한다. 실물 HC-SR501은 가변저항으로 이 시간을 조절한다.
- **0.1초 간격으로 읽는 것이 간단한 디바운싱 역할**을 한다. 바운싱은 수 ms 안에 끝나므로 100ms 간격이면 흔들리는 순간을 거의 건너뛴다. (아래 실험 참고)

---

## 실습 C — 방 상태 모니터 화면 (최종 코드)

### 작업별 주기
| 작업 | 주기 | 이유 |
|---|---|---|
| DHT 읽기 | 2초 | 센서 최소 읽기 간격 |
| 조도·PIR·물체 읽기 | 0.1초 | 빠르게 반응해야 함 |
| 화면 갱신 | 0.25초 | 사람 눈에 충분, 너무 자주 그리면 낭비 |

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ===== 핀 =====
#define DHT_PIN   4
#define DHT_TYPE  DHT22      // 실물 DHT11이면 DHT11
#define LDR_PIN   34
#define PIR_PIN   27
#define IR_PIN    26

// ===== 화면 =====
#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C

// ===== 주기 =====
const unsigned long DHT_INTERVAL    = 2000;
const unsigned long SENSOR_INTERVAL = 100;
const unsigned long SCREEN_INTERVAL = 250;

// ===== 조도 설정 =====
const bool LDR_BRIGHT_IS_LOW = true;

// ===== 객체 =====
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ===== 타이머 =====
unsigned long lastDht = 0, lastSensor = 0, lastScreen = 0;

// ===== 상태 =====
float temperature = NAN, humidity = NAN;
bool dhtOk = false;

int lightPct = 0;

bool motion = false, lastMotion = false;
bool obstacle = false, lastObstacle = false;
int objectCount = 0;

// ===== 조도 변환 =====
int lightPercent(int raw) {
  int p = map(raw, 0, 4095, 0, 100);
  return LDR_BRIGHT_IS_LOW ? 100 - p : p;
}

const char* lightLabel(int pct) {
  if (pct >= 60) return "BRIGHT";
  if (pct >= 25) return "DIM";
  return "DARK";
}

// ===== 센서 읽기 =====
void readDht() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    dhtOk = false;
    Serial.println("DHT read failed");
    return;
  }
  dhtOk = true;
  temperature = t;
  humidity = h;
}

void readSensors() {
  lightPct = lightPercent(analogRead(LDR_PIN));

  motion = (digitalRead(PIR_PIN) == HIGH);
  if (motion && !lastMotion)  Serial.println("Motion detected");
  if (!motion && lastMotion)  Serial.println("Motion ended");
  lastMotion = motion;

  obstacle = (digitalRead(IR_PIN) == LOW);
  if (obstacle && !lastObstacle) {
    objectCount++;
    Serial.printf("Object detected #%d\n", objectCount);
  }
  lastObstacle = obstacle;
}

// ===== 화면 =====
const int COL_VALUE = 48;   // 값이 시작하는 x 좌표 (라벨 8글자 × 6px)

void drawRow(int y, const char* label) {
  display.setCursor(0, y);
  display.print(label);
  display.setCursor(COL_VALUE, y);
}

void drawScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // 제목
  display.setCursor(0, 0);
  display.print("Smart Room Monitor");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // 온습도
  drawRow(14, "Temp/Hum");
  if (dhtOk) display.printf("%.1fC %.0f%%", temperature, humidity);
  else       display.print("ERROR");

  // 조도
  drawRow(26, "Light");
  display.printf("%3d%% %s", lightPct, lightLabel(lightPct));

  // 움직임 (감지 중이면 반전 표시)
  drawRow(38, "Motion");
  if (motion) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(" DETECTED ");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print("none");
  }

  // 물체
  drawRow(50, "Object");
  display.printf("%s  #%d", obstacle ? "NEAR" : "clear", objectCount);

  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(IR_PIN, INPUT_PULLUP);
  dht.begin();
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Smart Room Monitor");
  display.println("Starting...");
  display.display();
}

void loop() {
  unsigned long now = millis();

  if (now - lastDht >= DHT_INTERVAL) {
    lastDht = now;
    readDht();
  }
  if (now - lastSensor >= SENSOR_INTERVAL) {
    lastSensor = now;
    readSensors();
  }
  if (now - lastScreen >= SCREEN_INTERVAL) {
    lastScreen = now;
    drawScreen();
  }
}
```

### 화면 구성
```
Smart Room Monitor
──────────────────────
Temp/Hum 24.0C 40%
Light     72% BRIGHT
Motion   [ DETECTED ]
Object   NEAR  #3
```

| 요소 | y | 비고 |
|---|---|---|
| 제목 | 0 | |
| 구분선 | 10 | |
| 온습도 | 14 | 센서 실패 시 `ERROR` |
| 조도 | 26 | 밝기 %와 분류 |
| 움직임 | 38 | 감지 시 **반전 표시** |
| 물체 | 50 | 현재 상태와 누적 카운트 |

줄 간격 12px(글자 높이 8px + 여백 4px). 라벨은 x=0, 값은 x=48(8글자 × 6px)에서 시작해 열을 맞췄다.

### 구조 포인트
| 포인트 | 설명 |
|---|---|
| **작업별 타이머** | `loop()`는 각 작업의 시간이 됐는지만 확인하고 해당 작업만 실행한다 (협력적 스케줄링). 하나의 `loop()` 안에서 주기가 다른 작업들이 서로 방해하지 않는다 |
| **`now` 변수** | `millis()`를 한 번만 읽어 세 비교에 같이 써서, 같은 반복 안에서 기준 시각이 흔들리지 않는다 |
| **읽기와 그리기 분리** | 센서 함수는 상태 변수만 갱신하고, `drawScreen()`은 상태 변수만 보고 그린다 |
| **반전 표시** | `setTextColor(검정, 흰색)`으로 글자 배경을 칠해 중요한 상태를 강조 |
| **장애 격리** | DHT가 실패해도 다른 센서와 화면 갱신은 계속 동작한다 |

---

## 실험

- [x] 실습 A: 조도 슬라이더에 따라 `BRIGHT / DIM / DARK` 분류
- [x] 실습 B: PIR 감지 시작·종료 이벤트, 물체 감지 카운트
- [x] 실습 C: 모든 센서 상태가 한 화면에 실시간 표시

### 조도 방향 확인
- 이 모듈은 밝을수록 `raw` 값이 작아진다.
- `LDR_BRIGHT_IS_LOW = true` → 정상 표시, `false` → 밝기가 반대로 표시됨을 확인

### PIR 유지 시간
- PIR 감지 시 `DETECTED` 반전 표시가 나타나고, 유지 시간이 지나면 `none`으로 돌아오는 것을 확인

### 읽기 간격과 바운싱
| `SENSOR_INTERVAL` | 스위치를 빠르게 두 번 클릭 (감지 1회) | 원인 |
|---|---|---|
| 1ms | **2번 카운트** ❌ | 접점이 흔들리는 수 ms 사이에 여러 번 읽어 변화를 2번 감지 |
| 100ms | 1번 카운트 ✅ | 흔들림이 끝난 뒤에 읽어 흔들림을 건너뜀 |

- 읽기 간격을 늘리는 방식은 간단하지만, **100ms보다 짧게 지나가는 물체는 놓칠 수 있는** 한계가 있다.
- 짧게 읽으면 빠른 물체를 잡지만 바운싱에 약하고, 길게 읽으면 바운싱에 강하지만 빠른 물체를 놓친다 (트레이드오프).
- 4단계에서 자주 읽으면서도 값이 일정 시간 유지될 때만 인정하는 **안정 시간 디바운싱**(02 방식)으로 개선 예정.
- → [트러블슈팅 4번](../../docs/troubleshooting.md#4-읽기-간격을-줄이자-물체-감지가-2번-카운트됨-바운싱)

### 장애 격리
- `DHT_PIN`을 틀리게 바꿔도 온습도 줄만 `ERROR`로 표시되고, 조도·움직임·물체 감지와 화면 갱신은 정상 동작함을 확인

### 실물에서 할 것
- [ ] 실물 조도 센서 모듈의 값 방향 확인
- [ ] HC-SR501을 5V에 연결하고 안정화 시간, 감도·유지 시간 가변저항 동작 확인
- [ ] 적외선 장애물 센서의 감지 거리 조절과 Active Low 동작 확인

---

## 배운 점
- 센서 원본 값은 의미 있는 단위로 변환해 두면 센서 특성이 바뀌어도 한 곳만 고치면 된다.
- 엣지 검출은 버튼뿐 아니라 센서 이벤트(움직임 시작·종료, 물체 통과)에도 그대로 쓰인다.
- 작업마다 `millis()` 타이머를 두면, 주기가 다른 여러 작업을 `delay()` 없이 하나의 `loop()`에서 동시에 돌릴 수 있다.
- 읽기 주기는 반응 속도와 노이즈 내성 사이의 트레이드오프이며, 실험으로 직접 확인할 수 있다.
- 작업을 독립적으로 나누면 한 센서가 고장 나도 시스템 전체가 멈추지 않는다.
