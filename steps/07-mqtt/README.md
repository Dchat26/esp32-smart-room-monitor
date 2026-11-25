# 07. Wi-Fi + MQTT 원격 모니터링·제어 (MQTT)

ESP32를 Wi-Fi로 인터넷에 연결하고, **MQTT**로 센서 상태를 발행하며 브라우저(폰 포함)에서 LED와 릴레이를 원격 제어한다.
연결 실패 시 메인 루프가 멈추는 문제를 측정하고 **지수 백오프**로 개선했다.

- **시뮬레이션:** [Wokwi에서 실행하기](WOKWI_LINK)
- **언어:** C++ (Arduino framework)
- **라이브러리:** PubSubClient, DHT sensor library (Adafruit), Adafruit Unified Sensor, Adafruit SSD1306, Adafruit GFX Library
- **브로커:** HiveMQ 공개 브로커 (`broker.hivemq.com:1883`)

> ⚠️ 공개 브로커는 누구나 접근할 수 있다. 개인정보·비밀번호는 절대 발행하지 않는다.

---

## 왜 웹서버가 아니라 MQTT인가

Wokwi 무료 버전에서는 ESP32가 **바깥으로 나가는 연결**(외부 서버 접속)은 가능하지만, ESP32 안에 웹서버를 띄우고 **내 브라우저에서 들어오는 연결**은 유료 게이트웨이가 필요하다.

MQTT는 기기가 브로커로 먼저 연결하는 방식이라 이 제약이 없고, 실제 IoT에서도 많이 쓰는 이유가 같다.

| | ESP32 내장 웹서버 | MQTT |
|---|---|---|
| 연결 방향 | 외부 → ESP32 (들어옴) | ESP32 → 브로커 (나감) |
| 공유기 안의 기기에 외부 접속 | 포트포워딩 등 설정 필요 | 설정 불필요 |
| 상태 변화 전달 | 클라이언트가 주기적으로 요청 | 브로커가 즉시 전달 (push) |
| 메시지 크기 | HTTP 헤더 포함, 큼 | 작음 |

---

## MQTT 개념

```
                    ┌──────────────────┐
  ESP32  ─발행──→   │      브로커       │  ─전달──→  브라우저 (구독 중)
         ←전달──    │ broker.hivemq.com │  ←발행──   브라우저 (명령)
                    └──────────────────┘
```

| 용어 | 뜻 |
|---|---|
| 브로커 | 메시지를 받아 구독자에게 나눠 주는 서버 |
| 토픽 | 메시지의 주소(채널). `/`로 계층을 나눔 |
| 발행 (publish) | 토픽에 메시지 보내기 |
| 구독 (subscribe) | 토픽의 메시지 받아 보기 |
| `#` 와일드카드 | `a/b/#` 구독 시 `a/b/` 아래 모든 토픽 수신 |
| **retained** | 브로커가 토픽의 **마지막 메시지를 보관**했다가 새 구독자에게 즉시 전달 |
| **LWT (Last Will)** | 연결 시 브로커에 맡겨 두는 "유언". 비정상 종료 시 브로커가 대신 발행 |

보내는 쪽과 받는 쪽은 서로를 모르고, 같은 토픽만 쓰면 된다.

### 토픽 설계
공개 브로커이므로 다른 사용자와 겹치지 않게 고유 이름(`dchat26/srm`)으로 시작한다. `srm` = Smart Room Monitor.

| 토픽 | 방향 | 내용 | retained |
|---|---|---|---|
| `dchat26/srm/state` | ESP32 → | 센서·출력 상태 JSON (5초마다, 변경 시 즉시) | ✅ |
| `dchat26/srm/event` | ESP32 → | `motion:start`, `motion:end`, `object:N` | ❌ |
| `dchat26/srm/status` | ESP32 → | `online` / `offline`(LWT) | ✅ |
| `dchat26/srm/cmd/led` | → ESP32 | `ON` / `OFF` / `TOGGLE` | |
| `dchat26/srm/cmd/relay` | → ESP32 | `ON` / `OFF` / `TOGGLE` | |

- `state`, `status`는 **현재 상태**라 retained로 보내 새 구독자가 바로 알 수 있게 했다.
- `event`는 **지나간 사건**이라 retained로 보내면 새 구독자에게 오래된 사건이 전달되어 혼란을 준다.

### 상태 JSON 예시
```json
{"temp":24.0,"hum":40.0,"light":72,"motion":false,"obstacle":false,
 "objects":3,"led":true,"relay":false,"uptime":125}
```
센서 읽기 실패 시 `temp`, `hum`은 `nan` 대신 `null` (JSON에는 NaN이 없다).

---

## 회로

06 회로에 LED와 릴레이를 추가했다.

| 부품 | 핀 | 비고 |
|---|---|---|
| DHT | 4 | |
| OLED | 21 (SDA), 22 (SCL) | I2C |
| 조도 | 34 | ADC1 |
| PIR | 27 | |
| 적외선 (슬라이드 스위치) | 26 | |
| 버튼 | 19 | 페이지 전환 |
| **LED** | **16** → 220Ω → LED → GND | MQTT로 원격 제어 |
| **릴레이 IN** | **23** | VCC는 5V, MQTT로 원격 제어 |
| 릴레이 출력 LED | COM → 3V3, NO → 220Ω → LED → GND | 릴레이 동작 표시 |

### 릴레이
**전기 신호로 켜고 끄는 스위치.** ESP32의 작은 신호로 ESP32가 직접 감당할 수 없는 큰 전류나 다른 전원의 회로를 제어하며, 두 회로를 전기적으로 분리한다.

| 단자 | 릴레이 꺼짐 | 릴레이 켜짐 |
|---|---|---|
| COM (공통) | - | - |
| NO (Normally Open) | COM과 끊김 | COM과 **연결** |
| NC (Normally Closed) | COM과 **연결** | COM과 끊김 |

```
MQTT "ON" → GPIO23 HIGH → 릴레이 코일 동작 → COM–NO 연결 → 3V3 → 저항 → LED → GND
```

> 실물 5V 릴레이 모듈은 **IN이 LOW일 때 켜지는(Active Low)** 제품이 많다 → `RELAY_ACTIVE_LOW`로 대응.
> ESP32의 3.3V 신호로 5V 모듈이 동작하지 않는 경우가 있어 실물 확인 필요.
> 실습은 저전압만 사용. 220V는 감전·화재 위험.

---

## 파일 구조

```
07-mqtt/
├── 07-mqtt.ino          ← 수정: netBegin(), netLoop(), actuatorsBegin()
├── config.h             ← 수정: LED·릴레이 핀, MQTT 설정
├── secrets.example.h    ← 새 파일: Wi-Fi 정보 견본 (secrets.h는 Git에 올리지 않음)
├── state.h              ← 수정: LED·릴레이·연결 상태
├── debounce.h / .cpp    ← 06과 동일
├── sensors.h / .cpp     ← 수정: 이벤트를 MQTT로 발행
├── actuators.h / .cpp   ← 새 파일: LED, 릴레이 제어
├── net.h / .cpp         ← 새 파일: Wi-Fi + MQTT
└── screen.h / .cpp      ← 수정: 연결 상태 표시
```

### 실행 전 준비
`secrets.example.h`를 **`secrets.h`로 복사**한 뒤 Wi-Fi 정보를 입력한다.
```cpp
#pragma once
#define WIFI_SSID    "Wokwi-GUEST"   // 실물: 내 Wi-Fi 이름
#define WIFI_PASS    ""              // 실물: 내 Wi-Fi 비밀번호
#define WIFI_CHANNEL 6               // Wokwi는 6(빠른 연결), 실물은 0(자동)
```
실물에서는 Wi-Fi 비밀번호가 들어가므로 `secrets.h`는 `.gitignore`로 제외하고 **견본 파일만** 저장소에 올린다.

---

## 주요 코드

### config.h (추가분)
```cpp
// ===== 출력 =====
constexpr int  PIN_LED   = 16;
constexpr int  PIN_RELAY = 23;
constexpr bool RELAY_ACTIVE_LOW = false;   // 실물 모듈이 LOW에서 켜지면 true

// ===== MQTT =====
constexpr const char* MQTT_HOST = "broker.hivemq.com";
constexpr int MQTT_PORT = 1883;
#define TOPIC_BASE "dchat26/srm"

constexpr unsigned long PUBLISH_INTERVAL = 5000;   // 상태 발행 주기
```
`TOPIC_BASE`는 C의 **문자열 리터럴 이어 붙이기**를 쓰기 위해 `#define`으로 두었다. `TOPIC_BASE "/state"` → `"dchat26/srm/state"`.

### state.h (추가분)
```cpp
  bool  ledOn   = false;
  bool  relayOn = false;
  bool  wifiOk  = false;
  bool  mqttOk  = false;
```

### actuators.cpp
```cpp
#include "actuators.h"
#include "config.h"
#include "state.h"

void setLed(bool on) {
  state.ledOn = on;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
  Serial.printf("LED %s\n", on ? "ON" : "OFF");
}

void setRelay(bool on) {
  state.relayOn = on;
  bool level = RELAY_ACTIVE_LOW ? !on : on;   // Active Low면 신호를 반대로
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
  Serial.printf("Relay %s\n", on ? "ON" : "OFF");
}

void actuatorsBegin() {
  // 출력 모드로 바꾸기 전에 "꺼짐" 신호를 먼저 써서 부팅 순간 릴레이가 튀지 않게 함
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  setLed(false);
  setRelay(false);
}
```
**액추에이터:** 센서(입력)의 반대로 실제로 무언가를 움직이거나 켜는 부품 (LED, 릴레이, 부저, 모터).

### net.cpp (최종: 지수 백오프 적용)
```cpp
#include "net.h"
#include "config.h"
#include "secrets.h"
#include "state.h"
#include "actuators.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ===== 토픽 =====
#define T_STATE     TOPIC_BASE "/state"
#define T_EVENT     TOPIC_BASE "/event"
#define T_STATUS    TOPIC_BASE "/status"
#define T_CMD_ALL   TOPIC_BASE "/cmd/#"
#define T_CMD_LED   TOPIC_BASE "/cmd/led"
#define T_CMD_RELAY TOPIC_BASE "/cmd/relay"

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

// ===== 재연결 (지수 백오프) =====
static const unsigned long RECONNECT_MIN = 5000;    // 처음 재시도 간격
static const unsigned long RECONNECT_MAX = 60000;   // 최대 간격
static unsigned long reconnectDelay = RECONNECT_MIN;
static unsigned long lastReconnect = 0;
static bool firstAttempt = true;

static unsigned long lastPublish = 0;

// "ON" / "OFF" / "TOGGLE" 해석. 알 수 없는 명령이면 false
static bool parseOnOff(const String& cmd, bool current, bool& result) {
  if (cmd == "ON")     { result = true;     return true; }
  if (cmd == "OFF")    { result = false;    return true; }
  if (cmd == "TOGGLE") { result = !current; return true; }
  return false;
}

// 구독한 토픽에 메시지가 오면 라이브러리가 호출 (콜백)
static void onMessage(char* topic, byte* payload, unsigned int length) {
  String cmd;
  for (unsigned int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.trim();
  cmd.toUpperCase();
  Serial.printf("MQTT <- %s : %s\n", topic, cmd.c_str());

  bool on;
  if (strcmp(topic, T_CMD_LED) == 0) {
    if (!parseOnOff(cmd, state.ledOn, on)) { Serial.println("Unknown command"); return; }
    setLed(on);
  } else if (strcmp(topic, T_CMD_RELAY) == 0) {
    if (!parseOnOff(cmd, state.relayOn, on)) { Serial.println("Unknown command"); return; }
    setRelay(on);
  } else {
    return;
  }
  netPublishState();   // 바뀐 상태를 즉시 알림
}

static bool tryConnectMqtt() {
  String clientId = "srm-" + WiFi.macAddress();   // 브로커에서 기기를 구분하는 고유 ID
  Serial.printf("MQTT connecting to %s ...\n", MQTT_HOST);

  unsigned long t0 = millis();
  // LWT: 비정상 종료 시 브로커가 대신 "offline"을 retained로 발행
  bool ok = mqtt.connect(clientId.c_str(), T_STATUS, 0, true, "offline");
  Serial.printf("MQTT connect took %lu ms\n", millis() - t0);

  if (ok) {
    Serial.println("MQTT connected");
    mqtt.publish(T_STATUS, "online", true);
    mqtt.subscribe(T_CMD_ALL);
    netPublishState();
  } else {
    Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
  }
  return ok;
}

void netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);   // 시작만 하고 기다리지 않음 (논블로킹)
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);   // 기본 256바이트는 상태 JSON에 부족할 수 있음
  Serial.println("WiFi connecting...");
}

void netLoop() {
  unsigned long now = millis();

  // ----- Wi-Fi 상태 -----
  bool wifi = (WiFi.status() == WL_CONNECTED);
  if (wifi && !state.wifiOk) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  }
  if (!wifi && state.wifiOk) Serial.println("WiFi lost");
  state.wifiOk = wifi;

  if (!wifi) {               // Wi-Fi 재연결은 라이브러리가 자동으로 시도
    state.mqttOk = false;
    return;
  }

  // ----- MQTT 재연결 (지수 백오프) -----
  if (!mqtt.connected()) {
    state.mqttOk = false;
    if (firstAttempt || now - lastReconnect >= reconnectDelay) {
      firstAttempt = false;

      bool ok = tryConnectMqtt();
      lastReconnect = millis();          // 연결 시도가 "끝난" 시점부터 다시 잼

      if (ok) {
        reconnectDelay = RECONNECT_MIN;  // 성공하면 초기화
      } else {
        reconnectDelay = min(reconnectDelay * 2, RECONNECT_MAX);
        Serial.printf("Next MQTT retry in %lu s\n", reconnectDelay / 1000);
      }
    }
    return;
  }

  state.mqttOk = true;
  mqtt.loop();               // 수신 메시지 처리 + 연결 유지 신호

  // ----- 주기적 상태 발행 -----
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    netPublishState();
  }
}

void netPublishState() {
  if (!mqtt.connected()) return;

  char t[8], h[8];
  if (state.dhtOk) {
    snprintf(t, sizeof(t), "%.1f", state.temperature);
    snprintf(h, sizeof(h), "%.1f", state.humidity);
  } else {
    strcpy(t, "null");
    strcpy(h, "null");
  }

  char json[300];
  snprintf(json, sizeof(json),
    "{\"temp\":%s,\"hum\":%s,\"light\":%d,\"motion\":%s,\"obstacle\":%s,"
    "\"objects\":%d,\"led\":%s,\"relay\":%s,\"uptime\":%lu}",
    t, h, state.lightPct,
    state.motion ? "true" : "false",
    state.obstacle ? "true" : "false",
    state.objectCount,
    state.ledOn ? "true" : "false",
    state.relayOn ? "true" : "false",
    millis() / 1000);

  mqtt.publish(T_STATE, json, true);   // retained
}

void netPublishEvent(const char* msg) {
  if (mqtt.connected()) mqtt.publish(T_EVENT, msg);
}
```

### net.cpp 핵심 개념
| 개념 | 설명 |
|---|---|
| 논블로킹 Wi-Fi | `WiFi.begin()`은 연결을 시작만 하고 넘어간다. 연결 중에도 센서·화면이 동작한다 |
| 콜백 | "메시지가 오면 이 함수를 호출해 줘"라고 미리 등록. `onMessage`는 `mqtt.loop()` 안에서 라이브러리가 호출한다 |
| `mqtt.loop()` | 수신 메시지 처리와 연결 유지 신호. 자주 호출해야 하므로 타이머 없이 매 `loop()`마다 호출 |
| `snprintf` | 버퍼 크기를 넘지 않게 제한하는 `printf`. 메모리가 작은 기기에서 안전하게 문자열을 만드는 방법 |
| 입력 검증 | 알 수 없는 명령은 무시하고 로그만 남긴다 |

### sensors.cpp (변경분)
움직임·물체 이벤트를 시리얼 출력과 함께 MQTT로도 발행한다.
```cpp
#include "net.h"
...
  if (pir.activated())   { Serial.println("Motion detected"); netPublishEvent("motion:start"); }
  if (pir.deactivated()) { Serial.println("Motion ended");    netPublishEvent("motion:end"); }
  ...
  if (ir.activated()) {
    state.objectCount++;
    char msg[24];
    snprintf(msg, sizeof(msg), "object:%d", state.objectCount);
    netPublishEvent(msg);
  }
```

### screen.cpp (변경분)
- 헤더 오른쪽에 연결 표시: `W` = Wi-Fi, `M` = MQTT
- SYSTEM 페이지를 5줄(줄 간격 10px)로 변경

```
SYSTEM          W M 3/3
───────────────────────
Uptime  00:01:06
WiFi    10.10.0.2
MQTT    connected
Output  LED -- RLY --
Heap    218/212 KB
```
Heap은 `현재 남은 메모리 / 부팅 후 최저값` (`ESP.getFreeHeap()` / `ESP.getMinFreeHeap()`).

### 07-mqtt.ino (변경분)
```cpp
#include "actuators.h"
#include "net.h"
...
void setup() {
  ...
  sensorsBegin();
  actuatorsBegin();
  ...
  screenShowBoot();
  netBegin();
}

void loop() {
  unsigned long now = millis();
  netLoop();          // 타이머 없이 매번 호출
  ...
}
```

---

## 브라우저에서 확인·제어

1. Wokwi 시뮬레이션 실행 → 시리얼에 `WiFi connected` → `MQTT connected`, OLED에 `W M` 표시
2. [HiveMQ MQTT 브라우저 클라이언트](https://www.hivemq.com/demos/websocket-client/) → **Connect**
3. `dchat26/srm/#` 구독 → `status: online`, 5초마다 `state` JSON 수신
4. 발행: Topic `dchat26/srm/cmd/led`, Message `ON` → LED 켜짐, `state` 즉시 갱신
5. `OFF`, `TOGGLE`, `dchat26/srm/cmd/relay`도 동일

폰 브라우저에서도 같은 방법으로 제어할 수 있다.

---

## 실험

### 기본 동작
- [x] Wi-Fi·MQTT 연결, OLED `W M` 표시
- [x] 브라우저에서 상태·이벤트 수신
- [x] 브라우저에서 LED·릴레이 원격 제어
- [x] **retained:** 구독을 끊었다가 다시 구독하면 다음 발행을 기다리지 않고 마지막 `state`와 `status`를 즉시 수신. 시뮬레이션을 정지한 상태에서도 마지막 `state` 수신
- [ ] **LWT:** 시뮬레이션 정지 후 약 20초 뒤 `status: offline` 수신 (연결 유지 신호가 끊긴 뒤 브로커가 판단하는 시간이 필요)
- [ ] 알 수 없는 명령(`BLINK`) → 무시되고 `Unknown command` 출력

### 메모리 측정
| | 남은 메모리 (Free Heap) |
|---|---|
| 06 (Wi-Fi 없음) | 316 KB |
| 07 (Wi-Fi + MQTT) | 218 KB |
| **차이** | **약 98 KB (약 31%)** |

- 대부분은 MQTT가 아니라 **Wi-Fi 드라이버와 TCP/IP 통신 처리**가 사용한다 (MQTT 버퍼는 512바이트).
- 명령을 연속으로 발행하면 약 2KB 줄었다가 유지·회복된다. 측정 시 `218/212 KB`(현재/최저)로, 일시적으로 더 쓰고 반납하는 **정상 변동**으로 판단.

| | 정상 변동 | 메모리 누수 |
|---|---|---|
| 모양 | 줄었다가 일정 수준에서 멈추거나 회복 | 보낼수록 계속 줄어듦 |
| 결과 | 장시간 동작해도 문제없음 | 결국 메모리 부족으로 멈춤 |

- [ ] 명령 30~50회 연속 발행 후 1분 대기 → 현재값 회복 여부로 누수 최종 확인

### MQTT 연결 실패 시 블로킹 측정
`mqtt.connect()` 전후로 `millis()`를 측정했다.

| 상황 | `MQTT_HOST` | 연결 시도 시간 | 원인 |
|---|---|---|---|
| 정상 | `broker.hivemq.com` | 약 1,115ms | 성공해도 이만큼 멈춤 (Wokwi 공개 게이트웨이 경유) |
| A. 없는 주소 | `no-such-broker.invalid` | 3,165~3,329ms | 주소 찾기(DNS) 대기 후 실패 |
| B. 응답 없는 주소 | `10.255.255.1` | 3,004~3,005ms | 연결 대기 시간(약 3초) 초과로 실패 |

- 체감: **약 2초 동작 → 3초 멈춤** 반복. 멈춘 동안 화면·버튼·센서가 모두 정지.
- 재연결 간격 5초를 **시도 시작 시점**부터 재서, 3초 블로킹 후 2초만 동작 → 브로커 장애 시 **시간의 약 60%를 멈춤**.

**개선: 지수 백오프 + 시도 종료 시점부터 간격 측정**
```
재시도 간격: 5초 → 10초 → 20초 → 40초 → 60초 → 60초 ...
멈춤 비율:   3/8(38%) → 3/13(23%) → 3/23(13%) → 3/43(7%) → 3/63(약 5%)
```
- [x] 개선 후 `Next MQTT retry in 10 s, 20 s ...` 출력과 멈춤 빈도 감소 확인

→ [트러블슈팅 6번](../../docs/troubleshooting.md#6-mqtt-연결-실패-시-메인-루프가-3초씩-멈춤)

### 더 나은 해결 (향후)
| 방법 | 효과 | 난이도 |
|---|---|---|
| 지수 백오프 (적용) | 멈추는 빈도 감소 | 쉬움 |
| 연결 대기 시간 단축 | 한 번 멈추는 길이 감소 | 보통 |
| 네트워크를 별도 FreeRTOS 태스크로 분리 | 메인 루프가 아예 멈추지 않음. ESP32는 코어 2개 | 어려움 |

---

## 개발 환경 참고: 빌드 시간과 시뮬레이션 속도

### 빌드 시간 약 1분 30초
| 원인 | 설명 |
|---|---|
| Wi-Fi 코드 | `WiFi.h`를 쓰면 Wi-Fi 드라이버와 TCP/IP 처리 등 큰 코드가 함께 컴파일된다 |
| 라이브러리 수 | DHT, Unified Sensor, SSD1306, GFX, PubSubClient 5개 |
| 서버 빌드 | Wokwi는 서버에서 빌드하므로 사용자가 많으면 대기 시간이 더해진다 |

- 정상 범위. 실물에서 Arduino IDE로 빌드해도 처음에는 1~2분 걸릴 수 있다.
- 로컬 빌드는 두 번째부터 캐시를 재사용해 빨라지며, PlatformIO는 바뀐 파일만 다시 컴파일한다.

### 시뮬레이션 속도 55~70%
- 이 값은 **시뮬레이션이 실제 시간 대비 얼마나 빠르게 도는지**를 나타낸다.
- 06(Wi-Fi 없음)은 약 100%였고, 07은 Wi-Fi 흉내 내기 부담으로 55~70%로 떨어졌다. 부품 수, OLED 갱신, 컴퓨터 성능, 열린 탭 수도 영향을 준다.
- **실물 ESP32에는 영향 없음.** 실물은 항상 240MHz로 동작한다.

### 측정값 해석 시 주의
시뮬레이션 속 `millis()`는 **시뮬레이션 시간**이다. 속도가 60%면 시뮬레이션 1초가 실제로는 약 1.7초 걸린다.

| | 시뮬레이션 측정값 | 60%일 때 실제 체감 |
|---|---|---|
| MQTT 연결 실패 블로킹 | 3,000ms | 약 5초 |
| 버튼 디바운싱 | 50ms | 약 83ms |

- Wokwi에서 느낀 반응 지연이나 버튼 씹힘의 일부는 시뮬레이션 속도 때문일 수 있다.
- Wokwi 측정값은 **실물에서 다시 측정해 비교**해야 정확하다.

**시뮬레이션 속도를 올리는 방법:** 다른 탭(특히 영상) 닫기, 노트북은 전원 연결·고성능 모드, Wi-Fi가 필요 없는 측정은 Wi-Fi 없는 버전에서 하기.

---

## 실물에서 할 것
- [ ] `secrets.h`에 집 Wi-Fi 정보 입력, `WIFI_CHANNEL`은 0
- [ ] 릴레이 모듈의 Active Low 여부 확인 → `RELAY_ACTIVE_LOW`
- [ ] 3.3V 신호로 5V 릴레이 모듈이 동작하는지 확인
- [ ] 실물에서 MQTT 연결 시간 측정 (Wokwi 1.1초와 비교)
- [ ] 연결 실패 블로킹 시간, 버튼 반응을 실물에서 다시 측정해 Wokwi 결과와 비교
- [ ] 실물 빌드 시간 확인 (첫 빌드 vs 두 번째 빌드)

---

## 배운 점
- IoT 기기는 기기가 먼저 브로커로 연결하는 MQTT 방식으로, 공유기 안에서도 별도 설정 없이 원격 제어할 수 있다.
- 토픽 설계 시 상태(retained)와 사건(non-retained)을 구분해야 새 구독자가 올바른 정보를 받는다.
- LWT로 기기가 갑자기 꺼져도 다른 쪽이 상태를 알 수 있다.
- Wi-Fi는 약 98KB의 메모리를 사용한다. 기능을 추가할 때마다 남은 메모리와 누수 여부를 확인해야 한다.
- 네트워크 함수는 블로킹일 수 있다. 측정으로 멈춤 시간을 확인하고, 지수 백오프로 장애 시 영향을 줄였다.
- Wi-Fi 비밀번호 같은 정보는 별도 파일로 분리하고 견본만 공유한다.
- 시뮬레이터의 시간은 실제 시간과 다를 수 있으므로, 타이밍 측정값은 실물에서 다시 검증해야 한다.
