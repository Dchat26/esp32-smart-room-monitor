#include "net.h"
#include "config.h"
#include "secrets.h"
#include "state.h"
#include "actuators.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ===== 토픽 (문자열 이어 붙이기) =====
#define T_STATE     TOPIC_BASE "/state"
#define T_EVENT     TOPIC_BASE "/event"
#define T_STATUS    TOPIC_BASE "/status"
#define T_CMD_ALL   TOPIC_BASE "/cmd/#"
#define T_CMD_LED   TOPIC_BASE "/cmd/led"
#define T_CMD_RELAY TOPIC_BASE "/cmd/relay"

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static unsigned long lastReconnect = 0;
static unsigned long lastPublish = 0;
static bool firstAttempt = true;

static const unsigned long RECONNECT_MIN = 5000;    // 처음 재시도 간격 5초
static const unsigned long RECONNECT_MAX = 60000;   // 최대 60초
static unsigned long reconnectDelay = RECONNECT_MIN;

// "ON" / "OFF" / "TOGGLE" 해석. 알 수 없는 명령이면 false
static bool parseOnOff(const String& cmd, bool current, bool& result) {
  if (cmd == "ON")     { result = true;     return true; }
  if (cmd == "OFF")    { result = false;    return true; }
  if (cmd == "TOGGLE") { result = !current; return true; }
  return false;
}

// 구독한 토픽에 메시지가 오면 PubSubClient가 이 함수를 호출 (콜백)
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
  netPublishState();   // 바뀐 상태를 바로 알려 줌
}

static bool tryConnectMqtt() {
  String clientId = "srm-" + WiFi.macAddress();   // 브로커에서 기기를 구분하는 고유 ID
  Serial.printf("MQTT connecting to %s ...\n", MQTT_HOST);

  // 연결 시도에 걸린 시간 측정
  unsigned long t0 = millis();
  // 마지막 유언(LWT): 연결이 비정상적으로 끊기면 브로커가 대신 "offline"을 발행
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
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);   // 연결을 시작만 하고 기다리지 않음
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);   // 기본 256바이트는 상태 JSON에 부족할 수 있음
  Serial.println("WiFi connecting...");
}

void netLoop() {
  unsigned long now = millis();

  // ----- Wi-Fi 상태 확인 -----
  bool wifi = (WiFi.status() == WL_CONNECTED);
  if (wifi && !state.wifiOk) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  }
  if (!wifi && state.wifiOk) Serial.println("WiFi lost");
  state.wifiOk = wifi;

  if (!wifi) {               // Wi-Fi가 없으면 MQTT도 불가. 재연결은 Wi-Fi 라이브러리가 자동으로 시도
    state.mqttOk = false;
    return;
  }

  // ----- MQTT 연결 유지 -----
    if (!mqtt.connected()) {
    state.mqttOk = false;
    if (firstAttempt || now - lastReconnect >= reconnectDelay) {
      firstAttempt = false;

      bool ok = tryConnectMqtt();
      lastReconnect = millis();          // 연결 시도가 "끝난" 시점부터 다시 잼

      if (ok) {
        reconnectDelay = RECONNECT_MIN;  // 성공하면 간격 초기화
      } else {
        reconnectDelay = min(reconnectDelay * 2, RECONNECT_MAX);   // 실패할수록 2배
        Serial.printf("Next MQTT retry in %lu s\n", reconnectDelay / 1000);
      }
    }
    return;
  }

  state.mqttOk = true;
  mqtt.loop();               // 받은 메시지 처리 + 연결 유지 신호

  // ----- 주기적 상태 발행 -----
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    netPublishState();
  }
}

void netPublishState() {
  if (!mqtt.connected()) return;

  // 센서 실패 시 JSON에 nan 대신 null을 넣음
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

  mqtt.publish(T_STATE, json, true);   // retained: 나중에 구독한 사람도 마지막 상태를 바로 받음
}

void netPublishEvent(const char* msg) {
  if (mqtt.connected()) mqtt.publish(T_EVENT, msg);
}
