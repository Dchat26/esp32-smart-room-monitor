#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ===== 핀 / 설정 =====
#define DHT_PIN   4
#define DHT_TYPE  DHT22    // 실물 DHT11이면 DHT11로 변경
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

  // 제목과 구분선
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

  // 온도, 습도 (큰 글씨)
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.printf("%.1f C", temperature);
  display.setCursor(0, 36);
  display.printf("%.1f %%", humidity);

  // 체감온도 (작은 글씨)
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
