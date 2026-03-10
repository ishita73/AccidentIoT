#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define IR_LED 3

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

const float IMPACT_G = 1.8;     // body impact threshold
const unsigned long COOLDOWN_MS = 5000;

unsigned long lastImpactMs = 0;
unsigned long lastDisplayMs = 0;

void sendIR()
{
  Serial.println("IR SIGNAL SENT");

  // send pulses so vehicle IR receiver counts toggles
  for (int i = 0; i < 24; i++) {
    digitalWrite(IR_LED, HIGH);
    delay(35);
    digitalWrite(IR_LED, LOW);
    delay(35);
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(IR_LED, OUTPUT);
  digitalWrite(IR_LED, LOW);

  Wire.begin();
  Wire.setClock(400000);

  delay(300);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (true);
  }

  if (!accel.begin()) {

    Serial.println("ADXL345 not detected");

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println("ADXL ERR");
    display.display();

    while (true);
  }

  accel.setRange(ADXL345_RANGE_16_G);

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 8);
  display.println("READY");

  display.setTextSize(1);
  display.setCursor(18, 38);
  display.println("ADXL + OLED OK");

  display.display();

  delay(1200);

  Serial.println("Wearable ready");
}

void loop()
{
  sensors_event_t event;
  accel.getEvent(&event);

  float ax = event.acceleration.x;
  float ay = event.acceleration.y;
  float az = event.acceleration.z;

  float mag = sqrt(ax * ax + ay * ay + az * az);

  float gVal = mag / 9.80665;

  Serial.print("X:");
  Serial.print(ax, 2);
  Serial.print(" Y:");
  Serial.print(ay, 2);
  Serial.print(" Z:");
  Serial.print(az, 2);
  Serial.print(" G:");
  Serial.println(gVal, 2);

  unsigned long now = millis();

  // IMPACT DETECTION
  if (gVal > IMPACT_G && (now - lastImpactMs > COOLDOWN_MS))
  {
    lastImpactMs = now;

    display.clearDisplay();

    display.setTextColor(SSD1306_BLACK);
    display.fillRect(0, 0, 128, 64, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(10, 8);
    display.println("IMPACT!");

    display.setTextSize(1);
    display.setCursor(20, 40);
    display.print("G = ");
    display.println(gVal, 2);

    display.setCursor(20, 54);
    display.println("Sending IR");

    display.display();

    Serial.println("BODY IMPACT DETECTED");

    sendIR();

    delay(800);
    return;
  }

  // NORMAL DISPLAY
  if (now - lastDisplayMs > 250)
  {
    lastDisplayMs = now;

    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.print("X: ");
    display.println(ax, 1);

    display.setCursor(0, 14);
    display.print("Y: ");
    display.println(ay, 1);

    display.setCursor(0, 28);
    display.print("Z: ");
    display.println(az, 1);

    display.setTextSize(2);

    display.setCursor(0, 44);
    display.print("G:");
    display.print(gVal, 2);

    display.display();
  }

  delay(60);
}
