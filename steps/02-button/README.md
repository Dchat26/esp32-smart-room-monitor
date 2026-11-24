# 02. 버튼으로 LED 제어하기 (Button)

버튼 입력을 읽어 LED를 제어하고, 버튼 접점의 바운싱 문제를 `millis()` 기반 디바운싱으로 해결한 예제

- **시뮬레이션:** [Wokwi에서 실행하기](https://wokwi.com/projects/481571444371519489)
- **언어:** C++ (Arduino framework)

---

## 회로

| 부품 | 연결 |
|---|---|
| 푸시버튼 | 2번 쪽 핀(2.l) ↔ GPIO19 / 1번 쪽 핀(1.r) ↔ GND |
| LED + 220Ω | GPIO16 → LED(+ → −) → 220Ω → GND |

```
3.3V ─[내부 풀업 저항]─ GPIO19 ─ 버튼 ─ GND
GPIO16 ─ LED(+ → −) ─[220Ω]─ GND
```

> 푸시버튼의 다리 4개 중 **같은 번호(1.l↔1.r, 2.l↔2.r)는 내부에서 항상 연결**되어 있다.
> 반드시 **서로 다른 번호의 핀**에 연결해야 누를 때만 회로가 이어진다.

---

## 실습 A — 누르고 있는 동안만 켜기

```cpp
const int LED_PIN = 16;
const int BTN_PIN = 19;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);   // 내부 풀업 저항 사용
}

void loop() {
  bool pressed = (digitalRead(BTN_PIN) == LOW);  // 누르면 LOW
  digitalWrite(LED_PIN, pressed ? HIGH : LOW);
}
```

### INPUT_PULLUP과 Active Low
- 아무것도 연결되지 않은 입력 핀은 값이 HIGH와 LOW 사이를 제멋대로 오간다 (**Floating**).
- `INPUT_PULLUP`은 ESP32 내부 저항으로 핀을 3.3V 쪽으로 약하게 끌어올려 **기본값을 HIGH로 고정**한다.
- 버튼을 누르면 핀이 GND에 직접 연결되어 **LOW**가 된다.
- 이처럼 누르면 LOW가 되는 방식을 **Active Low**라고 한다. 외부 저항이 필요 없다.

| 버튼 | 핀 상태 | `digitalRead()` |
|---|---|---|
| 안 누름 | 내부 저항으로 3.3V에 연결 | `HIGH` |
| 누름 | GND에 직접 연결 | `LOW` |

---

## 실습 B — 토글 (엣지 검출)

한 번 누르면 켜고, 다시 누르면 끄기

```cpp
bool ledOn = false;
bool lastState = HIGH;
int pressCount = 0;

void loop() {
  bool current = digitalRead(BTN_PIN);

  // 직전 HIGH → 지금 LOW = 누른 순간 (하강 엣지)
  if (lastState == HIGH && current == LOW) {
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    pressCount++;
    Serial.print("press #");
    Serial.println(pressCount);
  }
  lastState = current;   // 반드시 if문 밖: 매 반복마다 직전 값 갱신
}
```

### 엣지 검출
"지금 눌려 있나?"가 아니라 **"방금 눌렸나?"**를 감지한다. 직전 값과 현재 값을 비교해 바뀌는 순간만 잡는다.

| 상황 | 직전(lastState) | 현재(current) | 토글 |
|---|---|---|---|
| 안 누름 | HIGH | HIGH | ❌ |
| **누른 순간** | HIGH | LOW | ✅ |
| 누르고 있음 | LOW | LOW | ❌ |
| 뗀 순간 | LOW | HIGH | ❌ (lastState가 HIGH로 돌아와 다음 누름 준비) |

- `HIGH → LOW`: 하강 엣지 (falling edge)
- `LOW → HIGH`: 상승 엣지 (rising edge)
- `lastState = current;`가 if문 **안**에 있으면, 한 번 누른 뒤 lastState가 LOW로 고정되어 **다시는 토글되지 않는다.**

### 관찰한 문제: 바운싱
한 번만 눌렀는데 `press #`가 2~3개씩 찍히고 LED가 반응하지 않는 것처럼 보이는 경우가 있었다.
→ [트러블슈팅](../../docs/troubleshooting.md#3-한-번-눌렀는데-여러-번-인식됨-바운싱) 참고

---

## 실습 C — 디바운싱 (최종 코드)

```cpp
const int LED_PIN = 16;
const int BTN_PIN = 19;
const unsigned long DEBOUNCE_MS = 50;

bool ledOn = false;
bool stableState = HIGH;          // 확정된 버튼 상태
bool lastReading = HIGH;          // 직전 반복에서 읽은 값
unsigned long lastChangeTime = 0; // 값이 마지막으로 바뀐 시각
int pressCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
}

void loop() {
  bool reading = digitalRead(BTN_PIN);           // ① 현재 값 읽기

  if (reading != lastReading) {                  // ② 값이 흔들리면
    lastChangeTime = millis();                   //    타이머 리셋
  }

  if (millis() - lastChangeTime > DEBOUNCE_MS    // ③ 50ms 넘게 유지되고
      && reading != stableState) {               //    확정 상태와 다르면
    stableState = reading;                       // ④ 확정 상태 갱신
    if (stableState == LOW) {                    // ⑤ '누름'일 때만
      ledOn = !ledOn;                            // ⑥ 토글
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      pressCount++;
      Serial.print("press #");
      Serial.println(pressCount);
    }
  }
  lastReading = reading;                         // ⑦ 직전 값 저장
}
```

### 동작 원리
| 단계 | 하는 일 |
|---|---|
| ① | 버튼 현재 값 읽기 |
| ② | 직전 값과 다르면 흔들린 것으로 보고 시각 기록 (타이머 리셋) |
| ③ | **50ms 동안 값이 유지**되고 **확정 상태와 다를 때만** 진짜 변화로 인정 |
| ④ | 확정 상태 갱신 → 같은 변화가 다시 처리되지 않음 |
| ⑤ | 뗄 때(LOW → HIGH)도 확정되지만 여기서 걸러져 LED는 그대로 |
| ⑥ | `!ledOn`으로 상태 반전, 조건 연산자로 HIGH/LOW 출력 |
| ⑦ | 이번 값을 다음 반복의 비교 대상으로 저장 |

### 시간 흐름 예시 (누를 때)
| 시간 | reading | 처리 | stableState | LED |
|---|---|---|---|---|
| 100ms | LOW | 누르기 시작 → 타이머 리셋 | HIGH | 꺼짐 |
| 101ms | HIGH | 바운싱 → 타이머 리셋 | HIGH | 꺼짐 |
| 102ms | LOW | 바운싱 → 타이머 리셋 | HIGH | 꺼짐 |
| 103~152ms | LOW | 안정, 대기 중 | HIGH | 꺼짐 |
| 153ms | LOW | **50ms 유지 → 확정, 토글** | LOW | **켜짐** |
| 154ms~ | LOW | 확정 상태와 같아 무시 | LOW | 켜짐 |

바운싱으로 생긴 흔들림은 타이머만 리셋할 뿐 LED에 영향을 주지 못한다. 대신 반응이 약 50ms 늦어지지만 사람이 느끼지 못하는 수준이다.

### 코드 구조 포인트
- **전역 변수:** `loop()` 밖에 선언해 반복이 끝나도 값이 유지된다. `reading`만 매 반복 새로 만들어진다.
- **`millis()`:** 보드가 켜진 뒤 지난 밀리초. `현재 시각 − 기록 시각`으로 경과 시간을 확인하므로 `delay()`처럼 멈추지 않는다.
- **`unsigned long`:** `millis()`는 약 49일 후 0으로 돌아가는데, 뺄셈으로 비교하면 그 순간에도 경과 시간이 올바르게 계산된다.

---

## 실험

- [x] 실습 A: 누르고 있는 동안만 LED 켜짐
- [x] 실습 B: 토글 동작 확인, **바운싱으로 한 번 누름이 여러 번 인식되는 현상 관찰**
- [x] 실습 C: 디바운싱 후 한 번 누를 때마다 `press #`가 하나씩 증가
- [x] 같은 번호 핀 배선 시 버튼을 안 눌러도 LED가 켜지는 문제 확인 및 수정
- [ ] 실물 보드에서 동일 회로 동작 확인

---

## 배운 점
- 입력 핀은 풀업/풀다운으로 기본값을 고정해야 안정적으로 읽을 수 있다.
- 버튼은 "상태"가 아니라 "상태가 바뀌는 순간(엣지)"을 감지해야 토글 같은 동작을 만들 수 있다.
- 기계식 버튼은 접점이 흔들리는 **바운싱**이 있어, 소프트웨어로 일정 시간 안정될 때까지 기다려야 한다.
- `delay()` 대신 `millis()`로 시간을 재면 기다리는 동안에도 다른 작업을 계속할 수 있다.
