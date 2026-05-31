# 🚗 ESP32 Real-Time Accident Detection System

A compact IoT-based safety system using **ESP32**, **MPU6050**, **ADXL345**, **NEO-6M GPS**, and **Piezo sensor** for real-time crash detection, automatic fuel cutoff, buzzer alert, and SMS notification via Twilio.

---

## ⚠️ Hardware Safety Fix (IMPORTANT — Issue #19)

### MOSFET Gate Protection

- The IRFZ44N **must NOT be connected directly to ESP32 GPIO**
- A **330Ω series resistor** is required between GPIO23 and MOSFET gate
- A **10kΩ pull-down resistor** is required between gate and GND

### Critical Electrical Limitation

- IRFZ44N is **not a logic-level MOSFET**
- At **3.3V (ESP32 output)**, it operates in the linear region → overheating risk
- Recommended replacement:
  - ✔ IRLZ44N (logic-level)
  - ✔ AO3400 (efficient low-power switching)

---

## 🔌 Hardware Setup (Vehicle Unit)

| Component | ESP32 Pin / Power | Purpose |
|---|---|---|
| MPU6050 | SDA → 21, SCL → 22 | Motion & acceleration sensing |
| NEO-6M GPS | TX → 16 (RX2), RX → 17 (TX2) | Location tracking |
| Piezo Sensor | Signal → 34 | Vibration / impact detection |
| Buzzer | GPIO 18 | Audio crash alert |
| MOSFET (Fuel Cutoff) | GPIO 23 → 330Ω → Gate | Simulated fuel cutoff |
| Pull-down resistor | Gate → 10kΩ → GND | Prevents floating gate |
| ESP32 | USB | Main controller |

---

## 🧠 Core Logic

1. MPU6050 samples motion at ~200 Hz
2. Gravity is removed → pure linear acceleration extracted
3. Piezo sensor confirms driver presence
4. Crash detection triggers when:
   - Acceleration > 2g
   - Jerk > 8g/s
   - Angular velocity > 300°/s
5. Severity levels:
   - **Minor:** ≥ 2g
   - **Moderate:** ≥ 3g
   - **Severe:** ≥ 5g
6. On crash: buzzer activates, fuel cutoff triggered, GPS location sent via SMS (Twilio)

---

## ⚙️ Firmware Architecture (Issue #19 Fix)

### Problem

Running IMU + GPS in a single loop causes GPS serial buffer overflow, missed IMU crash frames, and system latency under high sensor load.

### Solution: FreeRTOS Dual-Core Execution

| Core | Task |
|---|---|
| **Core 0** | GPS Task — reads and parses NEO-6M serial data |
| **Core 1** | IMU + Crash Detection — MPU6050 sampling (200 Hz), piezo monitoring, crash logic |

Implemented using `xTaskCreatePinnedToCore()` — see `version2/vehicle.ino`.

---

## 🔗 Version 2 Architecture (Multi-Device System)

| Unit | Sensors | Role |
|---|---|---|
| **Wearable** | ADXL345 | Detects rider body impact |
| **Vehicle** | MPU6050, GPS, Piezo | Detects crash + executes safety actions |

---

## 📡 Communication Protocol (Issue #19 Fix)

- **Protocol:** ESP-NOW (peer-to-peer, no router required, ~1ms latency)
- **Direction:** Wearable → Vehicle

**Workflow:**
1. Wearable detects impact via ADXL345
2. Sends impact packet over ESP-NOW
3. Vehicle ESP32 receives and cross-verifies with MPU6050 + Piezo
4. If confirmed → emergency system triggers

> ⚠️ Do **NOT** connect wearable and vehicle via I2C — units are physically separate and communicate wirelessly only.

---

## 🚗 Core Features

- Dual-layer crash detection (vehicle + wearable cross-validation)
- Physics-based crash modeling (acceleration + jerk + rotation)
- Gravity-compensated IMU processing at 200 Hz
- Real-time fuel cutoff simulation via MOSFET
- GPS-based emergency SMS alert (Twilio)
- Low-latency wireless communication via ESP-NOW

---

## 🛠️ Tech Stack

- ESP32 (Dual-core FreeRTOS)
- Embedded C++
- MPU6050, ADXL345, NEO-6M GPS, Piezo
- ESP-NOW communication protocol
- Twilio SMS API

---

## 🚀 How to Run

### Single Unit Mode (`code.ino`)

1. Wire components per the Hardware Setup table above — **include the 330Ω MOSFET gate resistor**
2. Install libraries: `MPU6050`, `TinyGPS++`, `TwilioESP`
3. Configure your Twilio credentials in the sketch
4. Upload `code.ino` to the ESP32
5. Open Serial Monitor at **115200 baud**

### Version 2 Mode — Dual Unit (`version2/`)

**Wearable Unit:**
1. Flash `version2/wearable.ino` onto the wearable ESP32
2. Power it on and note its MAC address

**Vehicle Unit:**
1. Flash `version2/vehicle.ino` onto the vehicle ESP32
2. Paste the wearable's MAC address into the ESP-NOW peer config in the sketch
3. Power the vehicle unit
4. Monitor output via Serial Monitor at **115200 baud**

---

## 🔮 Future Improvements

- Cloud dashboard for live tracking
- AI-based crash classification
- Mobile app integration (live alerts + GPS map)
- Battery-backed fail-safe system

---

## 🤝 Contributions

Pull requests and improvements are welcome. Focus areas: sensor calibration, communication reliability, power optimization, and safety enhancements.
