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
