# 03. 가변저항으로 RGB LED 제어하기 (Analog RGB)

가변저항의 아날로그 값을 읽어(ADC) RGB LED의 밝기와 색을 PWM으로 제어하는 예제

- **시뮬레이션:** [Wokwi에서 실행하기](https://wokwi.com/projects/481575221641350145)
- **언어:** C++ (Arduino framework)

---

## 회로

### 가변저항
| 가변저항 핀 | ESP32 |
|---|---|
| VCC | 3V3 |
| SIG (가운데) | GPIO34 |
| GND | GND |

### RGB LED (공통 캐소드) + 220Ω × 3
| RGB LED 핀 | 연결 |
|---|---|
| R | 220Ω → GPIO16 |
| G | 220Ω → GPIO17 |
| B | 220Ω → GPIO18 |
| COM | GND |

### RGB LED 종류
RGB LED는 LED 3개가 한 몸에 들어 있고 다리 하나(COM)를 공유한다.

| 종류 | COM 연결 | 켜지는 조건 |
|---|---|---|
| 공통 캐소드 (Common Cathode) | GND | 핀이 HIGH일 때 |
| 공통 애노드 (Common Anode) | 3V3 | 핀이 **LOW**일 때 (반대) |

코드의 `COMMON_ANODE` 값으로 두 종류 모두 대응한다.

### 저항을 색마다 따로 두는 이유
- 색마다 켜지는 전압이 다르다 (빨강 약 2V, 초록·파랑 약 3V).
- COM 쪽에 저항 하나만 두면 세 색이 저항 하나를 나눠 써서, 켜는 색 조합에 따라 밝기가 제멋대로 바뀐다.
- 그래서 **채널마다 따로 전류를 제한**한다.

---

## 실습 A — 가변저항 값 읽기 (ADC)

```cpp
const int POT_PIN = 34;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (millis() - lastPrint > 200) {     // 0.2초마다 출력
    lastPrint = millis();
    int raw = analogRead(POT_PIN);       // 0 ~ 4095
    float volt = raw * 3.3 / 4095.0;     // 전압으로 환산
    Serial.print("raw: ");
    Serial.print(raw);
    Serial.print("  volt: ");
    Serial.println(volt, 2);
  }
}
```

### 원리
- **가변저항 = 전압 분배기:** 손잡이 위치에 따라 가운데 핀(SIG)의 전압이 0V~3.3V 사이로 바뀐다.
- **ADC (Analog-to-Digital Converter):** 전압을 숫자로 바꾼다. ESP32는 **12비트 ADC** → 2¹² = 4096단계 (0~4095).

| 손잡이 위치 | 전압 | `analogRead()` |
|---|---|---|
| 한쪽 끝 | 0V | 0 |
| 가운데 | 약 1.65V | 약 2048 |
| 반대쪽 끝 | 3.3V | 4095 |

> 실물 ESP32의 ADC는 0V와 3.3V 근처에서 정확도가 떨어지고 약간 비선형적이다. 끝까지 돌려도 정확히 0이나 4095가 나오지 않을 수 있다.

---

## 실습 B — 가변저항으로 밝기 조절 (PWM)

```cpp
const int POT_PIN = 34;
const int R_PIN = 16;

void setup() {
  Serial.begin(115200);
}

void loop() {
  int raw = analogRead(POT_PIN);              // 0 ~ 4095
  int brightness = map(raw, 0, 4095, 0, 255); // 0 ~ 255로 변환
  analogWrite(R_PIN, brightness);             // PWM 출력
}
```

### PWM (Pulse Width Modulation)
ESP32 핀은 3.3V 아니면 0V만 낼 수 있다. 그래서 **아주 빠르게 켰다 껐다** 하면서, 켜져 있는 시간의 비율로 밝기를 표현한다. 1초에 수천 번 깜빡이므로 사람 눈에는 중간 밝기로 보인다.

```
duty 25%  ▇▁▁▁▇▁▁▁▇▁▁▁   → 어두움
duty 50%  ▇▇▁▁▇▇▁▁▇▇▁▁   → 중간
duty 100% ▇▇▇▇▇▇▇▇▇▇▇▇   → 최대
```

- 켜져 있는 시간의 비율 = **듀티 사이클 (duty cycle)**
- `analogWrite(pin, 128)` → 약 50% 듀티
- 01에서는 밝기를 바꾸려면 저항값을 바꿔야 했지만, PWM은 **회로는 그대로 두고 코드만으로** 밝기를 조절한다.

### `map()` 함수
한 범위의 값을 다른 범위로 비례 변환한다.
```cpp
map(값, 원래최소, 원래최대, 새최소, 새최대)
map(2048, 0, 4095, 0, 255)  // → 약 127
```
입력은 0~4095(12비트), PWM 출력은 0~255(8비트)라서 범위를 맞춘다.

---

## 실습 C — 가변저항으로 무지개색 고르기 (최종 코드)

```cpp
const int POT_PIN = 34;
const int R_PIN = 16;
const int G_PIN = 17;
const int B_PIN = 18;

const bool COMMON_ANODE = false;   // 공통 애노드 RGB LED라면 true

unsigned long lastPrint = 0;

// 한 색 채널 출력 (공통 애노드면 값을 반대로)
void writeChannel(int pin, int value) {
  if (COMMON_ANODE) value = 255 - value;
  analogWrite(pin, value);
}

void setColor(int r, int g, int b) {
  writeChannel(R_PIN, r);
  writeChannel(G_PIN, g);
  writeChannel(B_PIN, b);
}

// 색상환 위치(0~1535)를 R, G, B 값으로 변환
void hueToRGB(int hue, int &r, int &g, int &b) {
  int section = hue / 256;   // 0~5: 색상환을 6구간으로 나눔
  int x = hue % 256;         // 구간 안에서의 위치 0~255

  switch (section) {
    case 0:  r = 255;     g = x;       b = 0;       break; // 빨강 → 노랑
    case 1:  r = 255 - x; g = 255;     b = 0;       break; // 노랑 → 초록
    case 2:  r = 0;       g = 255;     b = x;       break; // 초록 → 청록
    case 3:  r = 0;       g = 255 - x; b = 255;     break; // 청록 → 파랑
    case 4:  r = x;       g = 0;       b = 255;     break; // 파랑 → 자홍
    default: r = 255;     g = 0;       b = 255 - x; break; // 자홍 → 빨강
  }
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  int raw = analogRead(POT_PIN);
  int hue = map(raw, 0, 4095, 0, 1535);

  int r, g, b;
  hueToRGB(hue, r, g, b);
  setColor(r, g, b);

  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.printf("raw: %4d  hue: %4d  RGB(%3d, %3d, %3d)\n", raw, hue, r, g, b);
  }
}
```

### 색 섞기 원리
빛의 삼원색을 섞으면 다른 색이 된다.

| 조합 | 결과 |
|---|---|
| 빨강 + 초록 | 노랑 |
| 초록 + 파랑 | 청록 |
| 파랑 + 빨강 | 자홍 |
| 빨강 + 초록 + 파랑 | 흰색 |

### 색상환 변환
색상환을 6구간으로 나누고, 구간마다 한 색은 최대(255), 한 색은 0으로 두고 **나머지 한 색만 서서히 올리거나 내린다.** 256 × 6 = 1536이므로 hue 범위는 0~1535.

```
hue:   0      256     512     768     1024    1280    1535
색:   빨강 → 노랑 → 초록 → 청록 → 파랑 → 자홍 → 빨강
R:    255 ─── 255 ↘  0 ───── 0 ───── 0  ↗ 255 ─── 255
G:     0  ↗  255 ─── 255 ─── 255 ↘  0 ───── 0 ───── 0
B:     0 ───── 0 ───── 0  ↗ 255 ─── 255 ─── 255 ↘  0
```

### 새로 사용한 문법
| 문법 | 설명 |
|---|---|
| 사용자 정의 함수 | `setColor()`, `hueToRGB()`처럼 반복되는 동작을 이름 붙여 묶음 |
| `int &r` (참조) | 함수 안에서 바꾼 값이 함수 밖 변수에도 반영됨. 값 3개를 한 번에 돌려받기 위해 사용 |
| `switch` | 값에 따라 여러 경우로 분기 |
| `/`, `%` | 정수 나눗셈의 몫과 나머지. 구간 번호와 구간 안 위치 계산 |
| `Serial.printf` | 여러 값을 형식에 맞춰 한 줄로 출력. `%4d` = 정수를 4칸에 맞춤 |

---

## 핀 선택: ADC1과 ADC2

ESP32에는 아날로그 값을 읽는 ADC가 **두 개(ADC1, ADC2)** 있다. **ADC2는 Wi-Fi와 공유**하는 장치라서, Wi-Fi를 켜면 ADC2 핀으로는 `analogRead()`를 할 수 없다.

| ADC | 핀 | Wi-Fi 켰을 때 `analogRead()` |
|---|---|---|
| **ADC1** | 32, 33, 34, 35, 36(VP), 39(VN) | ✅ 정상 |
| **ADC2** | 0, 2, 4, 12, 13, 14, 15, 25, 26, 27 | ❌ 읽기 실패 |

### 왜 신경 쓰는가
- **에러가 나지 않는다.** 컴파일·업로드 모두 정상이고 값만 이상하게 나와서 원인을 찾기 어렵다.
- **나중에 터진다.** Wi-Fi 없이 테스트할 때는 정상이다가, Wi-Fi 기능을 붙이는 순간 문제가 생긴다. 그때 핀을 바꾸려면 회로, 코드, 문서를 모두 고쳐야 한다.
- 이 프로젝트는 최종적으로 **센서 값을 웹 대시보드에 표시**하므로, 아날로그 입력은 처음부터 **ADC1 핀(34)**에 연결했다.

### ADC2 핀을 못 쓰는 것은 아니다
| 용도 | ADC2 핀 |
|---|---|
| `digitalRead()` / `digitalWrite()` | ✅ |
| `analogWrite()` (PWM 출력) | ✅ |
| `analogRead()` + Wi-Fi 꺼짐 | ✅ |
| `analogRead()` + Wi-Fi 켜짐 | ❌ |

DHT11(4번), PIR(27번), 부저(25번)처럼 **디지털 신호**는 ADC2 핀에 연결해도 문제없다.

> GPIO34~39는 **입력 전용** 핀이라 출력(LED 등)에는 사용할 수 없다. 아날로그 센서 입력에 적합하다.

---

## 실험

- [x] 실습 A: 가변저항을 돌리면 0~4095 값과 전압이 변함
- [x] 실습 B: 가변저항으로 빨간색 밝기가 부드럽게 변함
- [x] 실습 C: 빨강 → 노랑 → 초록 → 청록 → 파랑 → 자홍 순서로 색이 변함
- [ ] `COMMON_ANODE`를 `true`로 바꿔 반대색이 나오는지 확인
- [ ] 저항을 COM 쪽 하나로 바꿔 밝기 변화 관찰
- [ ] 실물 보드에서 초록·파랑이 빨강보다 어둡게 보이는지 확인
- [ ] 실물 키트의 RGB LED가 공통 캐소드인지 공통 애노드인지 확인

---

## 배운 점
- ADC는 전압을 숫자로 바꾸고, ESP32는 12비트(0~4095) 해상도를 가진다.
- PWM은 켜짐/꺼짐의 시간 비율(듀티 사이클)로 아날로그처럼 보이는 출력을 만든다.
- 서로 다른 해상도의 입력과 출력은 `map()`으로 범위를 맞춘다.
- 핀은 기능만 보고 고르면 안 되고, **Wi-Fi와의 충돌, 입력 전용 여부** 같은 하드웨어 제약까지 확인해서 설계해야 한다.
- 반복되는 동작을 함수로 분리하면 코드가 읽기 쉽고, 공통 애노드/캐소드 같은 차이도 한 곳에서 처리할 수 있다.
