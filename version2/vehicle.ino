#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPSPlus.h>
#include <BluetoothSerial.h>
#include "driver/adc.h"

// ---------------- PIN CONFIG ----------------
const int FUEL_GATE_PIN = 23;
const int BUZZER_PIN = 18;
const int IR_PIN = 35;

const int I2C_SDA = 21;
const int I2C_SCL = 22;

const int GPS_RX = 16;
const int GPS_TX = 17;
const uint32_t GPS_BAUD = 9600;

const int PIEZO_ADC_PIN = 34;

// ---------------- OBJECTS ----------------
HardwareSerial gpsSerial(2);
BluetoothSerial SerialBT;

MPU6050 mpu;
TinyGPSPlus gps;

// ---------------- CONSTANTS ----------------
const float ACC_SENS = 16384.0f;
const float GYRO_SENS = 131.0f;

const float GRAVITY_ALPHA = 0.98f;
const float A_SMOOTH_ALPHA = 0.85f;

const float SAMPLE_HZ = 200.0f;
const uint32_t SAMPLE_US = 1000000 / SAMPLE_HZ;

const uint32_t IMPACT_WINDOW_MS = 150;
const uint32_t VALIDATION_WINDOW_MS = 2000;
const uint32_t LATCH_HOLDOFF_MS = 10000;

// Severity thresholds
const float A_MINOR_G = 2.5f, J_MINOR = 5.0f, W_MINOR = 250.0f;
const float A_MOD_G   = 4.0f, J_MOD   = 8.0f, W_MOD   = 300.0f;
const float A_SEV_G   = 6.0f, J_SEV   = 15.0f, W_SEV   = 500.0f;

// Seat detection
const float SEAT_ENERGY_THRESH = 12.0f;
const int SEAT_SAMPLES = 40;

// IR detection
const int IR_CHANGE_THRESHOLD = 80;
const int IR_PULSE_MIN = 4;

// ---------------- STATE ----------------
float gX = 0, gY = 0, gZ = 1;
float prevA = 0;

float axBias = 0, ayBias = 0, azBias = 0;

bool seatOccupied = true;

int irPulseCount = 0;
bool waitingWearable = false;
uint32_t wearableStart = 0;
uint32_t lastTrigger = 0;

uint32_t lastSample = 0;
uint32_t lastDbg = 0;

// impact window
struct {
  float a_peak;
  float j_peak;
  float w_peak;
  uint32_t start_ms;
} win;

// seat energy
float seatHP = 0;
float seatSum = 0;
int seatCount = 0;

// ---------------- HELPERS ----------------
float lpf(float prev, float x, float a) {
  return a * prev + (1 - a) * x;
}

void setFuel(bool on) {
  digitalWrite(FUEL_GATE_PIN, on ? HIGH : LOW);
}

void buzz(int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

String classify(float a, float j, float w) {
  if (a >= A_SEV_G || j >= J_SEV || w >= W_SEV) return "SEVERE";
  if (a >= A_MOD_G || j >= J_MOD || w >= W_MOD) return "MODERATE";
  if (a >= A_MINOR_G || j >= J_MINOR || w >= W_MINOR) return "MINOR";
  return "NONE";
}

double getLat() {
  return gps.location.isValid() ? gps.location.lat() : 12.843583;
}

double getLng() {
  return gps.location.isValid() ? gps.location.lng() : 80.156194;
}

void sendBT(String s) {
  SerialBT.println(s);
  Serial.println(s);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Accident_BT");

  pinMode(FUEL_GATE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);

  setFuel(true);

  Wire.begin(I2C_SDA, I2C_SCL);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 FAIL");
    while (1);
  }

  // GPS
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  // calibration
  long ax=0, ay=0, az=0;

  for (int i = 0; i < 200; i++) {
    int16_t x,y,z,gx,gy,gz;
    mpu.getMotion6(&x,&y,&z,&gx,&gy,&gz);
    ax += x; ay += y; az += z;
    delay(5);
  }

  axBias = ax / 200.0;
  ayBias = ay / 200.0;
  azBias = az / 200.0;

  win = {0,0,0,millis()};

  lastSample = micros();

  Serial.println("System Ready");
}

// ---------------- LOOP ----------------
void loop() {

  uint32_t nowUs = micros();
  uint32_t nowMs = millis();

  // GPS always read (non-blocking)
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // fixed sampling rate
  if (nowUs - lastSample < SAMPLE_US) return;
  lastSample += SAMPLE_US;

  float dt = SAMPLE_US / 1e6;

  // ---------------- MPU ----------------
  int16_t axr, ayr, azr, gxr, gyr, gzr;
  mpu.getMotion6(&axr,&ayr,&azr,&gxr,&gyr,&gzr);

  float ax = (axr - axBias) / ACC_SENS;
  float ay = (ayr - ayBias) / ACC_SENS;
  float az = (azr - azBias) / ACC_SENS;

  float gx = gxr / GYRO_SENS;
  float gy = gyr / GYRO_SENS;
  float gz = gzr / GYRO_SENS;

  float w = sqrt(gx*gx + gy*gy + gz*gz);

  gX = lpf(gX, ax, GRAVITY_ALPHA);
  gY = lpf(gY, ay, GRAVITY_ALPHA);
  gZ = lpf(gZ, az, GRAVITY_ALPHA);

  float lin = sqrt(
    pow(ax - gX,2) +
    pow(ay - gY,2) +
    pow(az - gZ,2)
  );

  float a = lin;
  float aSmooth = lpf(prevA, a, A_SMOOTH_ALPHA);

  float jerk = (aSmooth - prevA) / dt;
  prevA = aSmooth;

  // ---------------- IMPACT WINDOW ----------------
  if (aSmooth > win.a_peak) win.a_peak = aSmooth;
  if (fabs(jerk) > win.j_peak) win.j_peak = fabs(jerk);
  if (w > win.w_peak) win.w_peak = w;

  if (nowMs - win.start_ms > IMPACT_WINDOW_MS) {

    String sev = classify(win.a_peak, win.j_peak, win.w_peak);

    if (sev != "NONE" &&
        seatOccupied &&
        !waitingWearable &&
        (nowMs - lastTrigger > LATCH_HOLDOFF_MS)) {

      Serial.println("Impact detected");
      waitingWearable = true;
      wearableStart = nowMs;
      irPulseCount = 0;
    }

    win = {0,0,0,nowMs};
  }

  // ---------------- IR wearable validation ----------------
  static int lastIR = 0;
  int ir = analogRead(IR_PIN);

  if (waitingWearable && abs(ir - lastIR) > IR_CHANGE_THRESHOLD) {
    irPulseCount++;
  }

  lastIR = ir;

  if (waitingWearable && (nowMs - wearableStart > VALIDATION_WINDOW_MS)) {

    if (irPulseCount >= IR_PULSE_MIN) {

      lastTrigger = nowMs;
      setFuel(false);
      buzz(1000);

      String msg = "ACCIDENT,CONFIRMED," +
                   String(getLat(),6) + "," +
                   String(getLng(),6);

      sendBT(msg);
    }
    else {
      Serial.println("False trigger ignored");
    }

    waitingWearable = false;
  }

  // ---------------- DEBUG ----------------
  if (nowMs - lastDbg > 500) {
    lastDbg = nowMs;

    Serial.printf("a=%.2f jerk=%.1f w=%.0f IR=%d seat=%d\n",
      aSmooth, jerk, w, ir, seatOccupied);
  }
}
