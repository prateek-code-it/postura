/*
  Portable Smart Spine Posture Monitoring Device (v2)
  Neckband Design: MPU6050 (head tilt) + Flex Sensor Tail (upper-spine curvature)
  Target: ESP32 (BLE built-in). Swap to nRF52840 board profile later if desired.

  Wiring:
    MPU6050        -> SDA: GPIO21, SCL: GPIO22, VCC: 3.3V, GND: GND
    Flex sensor    -> One leg to 3.3V, other leg to GPIO34 (ADC1_CH6)
                      AND to GND through a fixed resistor (voltage divider)
                      Check your flex sensor's datasheet for its unflexed/bent
                      resistance range and pick the divider resistor accordingly
                      (commonly 10k-47k works for most hobby flex sensors)
    Vibration motor -> GPIO 4 (through NPN transistor if motor draws >40mA)

  Library needed: Adafruit_MPU6050, Adafruit_Sensor, Wire
*/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

#define FLEX_PIN 34       // ADC pin for flex sensor
#define ALERT_PIN 4       // Vibration motor / buzzer

// ---- Calibration values (set during calibrateBaseline) ----
float baselinePitch = 0;      // neutral head tilt angle
int   baselineFlex   = 0;      // flex sensor reading when spine is straight
int   maxBendFlex     = 0;      // flex sensor reading at a deliberate max bend

// ---- Tunable thresholds ----
const float PITCH_DEVIATION_LIMIT = 12.0;   // degrees, forward head tilt tolerance
const float BEND_RATIO_LIMIT      = 0.35;   // 0.0 = straight, 1.0 = fully bent (calibrated)
const unsigned long DEBOUNCE_MS   = 3000;   // sustained bad posture before alert

float pitch = 0;
unsigned long lastTime = 0;
unsigned long badPostureStart = 0;
bool alerting = false;

float computePitch(sensors_event_t &accel, sensors_event_t &gyro, float prevPitch, float dt) {
  float accelPitch = atan2(accel.acceleration.y,
                            sqrt(accel.acceleration.x * accel.acceleration.x +
                                 accel.acceleration.z * accel.acceleration.z)) * 180.0 / PI;
  float gyroRate = gyro.gyro.x * 180.0 / PI;
  return 0.98 * (prevPitch + gyroRate * dt) + 0.02 * accelPitch;
}

void calibrateBaseline() {
  Serial.println("Sit/stand in your natural, correct posture...");
  delay(2000);

  sensors_event_t a, g, temp;
  float sumPitch = 0;
  long sumFlex = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    mpu.getEvent(&a, &g, &temp);
    float p = atan2(a.acceleration.y,
                     sqrt(a.acceleration.x * a.acceleration.x + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
    sumPitch += p;
    sumFlex += analogRead(FLEX_PIN);
    delay(50);
  }
  baselinePitch = sumPitch / samples;
  baselineFlex = sumFlex / samples;
  pitch = 0;

  Serial.println("Now gently bend/slouch forward as far as comfortable for 2 seconds (max-bend calibration)...");
  delay(2000);
  long sumMaxFlex = 0;
  for (int i = 0; i < samples; i++) {
    sumMaxFlex += analogRead(FLEX_PIN);
    delay(50);
  }
  maxBendFlex = sumMaxFlex / samples;

  Serial.print("Baseline pitch: "); Serial.println(baselinePitch);
  Serial.print("Baseline flex: "); Serial.println(baselineFlex);
  Serial.print("Max-bend flex: "); Serial.println(maxBendFlex);
}

void setup() {
  Serial.begin(115200);
  pinMode(ALERT_PIN, OUTPUT);
  analogReadResolution(12); // ESP32 ADC: 0-4095

  Wire.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);

  calibrateBaseline();
  lastTime = millis();
}

void loop() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  lastTime = now;

  pitch = computePitch(accel, gyro, pitch, dt);
  float pitchDeviation = abs(pitch - baselinePitch);

  int flexRaw = analogRead(FLEX_PIN);
  float bendRatio = 0;
  if (maxBendFlex != baselineFlex) {
    bendRatio = float(flexRaw - baselineFlex) / float(maxBendFlex - baselineFlex);
    bendRatio = constrain(bendRatio, 0.0, 1.5); // allow slight overshoot beyond calibrated max
  }

  // ---- Combined posture classification ----
  String status;
  bool badPosture = false;

  if (pitchDeviation > PITCH_DEVIATION_LIMIT || bendRatio > BEND_RATIO_LIMIT) {
    badPosture = true;
    status = (pitchDeviation > PITCH_DEVIATION_LIMIT * 1.5 || bendRatio > BEND_RATIO_LIMIT * 1.5)
              ? "POOR" : "MODERATE";
  } else {
    status = "GOOD";
  }

  Serial.print("Pitch dev: "); Serial.print(pitchDeviation);
  Serial.print(" | Bend ratio: "); Serial.print(bendRatio);
  Serial.print(" | Status: "); Serial.println(status);

  if (badPosture) {
    if (badPostureStart == 0) {
      badPostureStart = now;
    } else if (now - badPostureStart > DEBOUNCE_MS && !alerting) {
      alerting = true;
      digitalWrite(ALERT_PIN, HIGH);
      Serial.println(">>> POSTURE ALERT <<<");
    }
  } else {
    badPostureStart = 0;
    if (alerting) {
      alerting = false;
      digitalWrite(ALERT_PIN, LOW);
    }
  }

  delay(50); // ~20Hz sampling
}
