const int POT_PIN = 34;
const int R_PIN = 16;
const int G_PIN = 17;
const int B_PIN = 18;

const bool COMMON_ANODE = false;   // 공통 애노드 RGB LED라면 true로 변경

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
