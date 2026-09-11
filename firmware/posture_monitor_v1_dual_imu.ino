/*
  Portable Smart Spine Posture Monitoring Device
  Starter Firmware - ESP32 + Dual MPU6050

  Wiring:
    MPU6050 #1 (Upper back) -> AD0 to GND -> I2C address 0x68
    MPU6050 #2 (Lower back) -> AD0 to 3.3V -> I2C address 0x69
    Both share SDA (GPIO21) and SCL (GPIO22) on ESP32
    Buzzer/vibration motor -> GPIO 4 (through transistor if motor)

  Library needed: Adafruit_MPU6050, Adafruit_Sensor, Wire
  Install via Arduino IDE Library Manager.
*/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpuUpper;
Adafruit_MPU6050 mpuLower;

#define MPU_UPPER_ADDR 0x68
#define MPU_LOWER_ADDR 0x69
#define ALERT_PIN 4

// Complementary filter state
float pitchUpper = 0, pitchLower = 0;
unsigned long lastTime = 0;

// Calibration + thresholds
float baselineAngle = 0;
const float DEVIATION_THRESHOLD = 15.0;   // degrees, tune during testing
const unsigned long DEBOUNCE_MS = 3000;   // must persist 3s before alert
unsigned long badPostureStart = 0;
bool alerting = false;

float computePitch(sensors_event_t &accel, sensors_event_t &gyro, float prevPitch, float dt) {
  // Accelerometer-based pitch estimate
  float accelPitch = atan2(accel.acceleration.y,
                            sqrt(accel.acceleration.x * accel.acceleration.x +
                                 accel.acceleration.z * accel.acceleration.z)) * 180.0 / PI;

  // Complementary filter: blend gyro integration with accel estimate
  float gyroRate = gyro.gyro.x * 180.0 / PI; // deg/s
  float pitch = 0.98 * (prevPitch + gyroRate * dt) + 0.02 * accelPitch;
  return pitch;
}

void setup() {
  Serial.begin(115200);
  pinMode(ALERT_PIN, OUTPUT);
  Wire.begin();

  if (!mpuUpper.begin(MPU_UPPER_ADDR)) {
    Serial.println("Upper MPU6050 not found!");
    while (1) delay(10);
  }
  if (!mpuLower.begin(MPU_LOWER_ADDR)) {
    Serial.println("Lower MPU6050 not found!");
    while (1) delay(10);
  }

  mpuUpper.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuUpper.setGyroRange(MPU6050_RANGE_500_DEG);
  mpuLower.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuLower.setGyroRange(MPU6050_RANGE_500_DEG);

  Serial.println("Sit in your natural, correct posture for calibration...");
  delay(3000);
  calibrateBaseline();
  Serial.print("Baseline spine angle set to: ");
  Serial.println(baselineAngle);

  lastTime = millis();
}

void calibrateBaseline() {
  sensors_event_t a1, g1, temp1, a2, g2, temp2;
  float sumAngle = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    mpuUpper.getEvent(&a1, &g1, &temp1);
    mpuLower.getEvent(&a2, &g2, &temp2);

    float pU = atan2(a1.acceleration.y,
                      sqrt(a1.acceleration.x * a1.acceleration.x + a1.acceleration.z * a1.acceleration.z)) * 180.0 / PI;
    float pL = atan2(a2.acceleration.y,
                      sqrt(a2.acceleration.x * a2.acceleration.x + a2.acceleration.z * a2.acceleration.z)) * 180.0 / PI;

    sumAngle += (pL - pU);
    delay(50);
  }
  baselineAngle = sumAngle / samples;
  pitchUpper = 0;
  pitchLower = 0;
}

void loop() {
  sensors_event_t accelU, gyroU, tempU;
  sensors_event_t accelL, gyroL, tempL;

  mpuUpper.getEvent(&accelU, &gyroU, &tempU);
  mpuLower.getEvent(&accelL, &gyroL, &tempL);

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  lastTime = now;

  pitchUpper = computePitch(accelU, gyroU, pitchUpper, dt);
  pitchLower = computePitch(accelL, gyroL, pitchLower, dt);

  float spineAngle = pitchLower - pitchUpper;
  float deviation = abs(spineAngle - baselineAngle);

  Serial.print("Spine angle: ");
  Serial.print(spineAngle);
  Serial.print(" | Deviation: ");
  Serial.println(deviation);

  if (deviation > DEVIATION_THRESHOLD) {
    if (badPostureStart == 0) {
      badPostureStart = now;
    } else if (now - badPostureStart > DEBOUNCE_MS && !alerting) {
      alerting = true;
      digitalWrite(ALERT_PIN, HIGH);
      Serial.println(">>> BAD POSTURE ALERT <<<");
    }
  } else {
    badPostureStart = 0;
    if (alerting) {
      alerting = false;
      digitalWrite(ALERT_PIN, LOW);
    }
  }

  delay(50); // ~20Hz sampling rate
}



/*
  Portable Smart Spine Posture Monitoring Device
  Starter Firmware - ESP32 + Dual MPU6050

  Wiring:
    MPU6050 #1 (Upper back) -> AD0 to GND -> I2C address 0x68
    MPU6050 #2 (Lower back) -> AD0 to 3.3V -> I2C address 0x69
    Both share SDA (GPIO21) and SCL (GPIO22) on ESP32
    Buzzer/vibration motor -> GPIO 4 (through transistor if motor)

  Library needed: Adafruit_MPU6050, Adafruit_Sensor, Wire
  Install via Arduino IDE Library Manager.


#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpuUpper;
Adafruit_MPU6050 mpuLower;

#define MPU_UPPER_ADDR 0x68
#define MPU_LOWER_ADDR 0x69
#define ALERT_PIN 4

// Complementary filter state
float pitchUpper = 0, pitchLower = 0;
unsigned long lastTime = 0;

// Calibration + thresholds
float baselineAngle = 0;
const float DEVIATION_THRESHOLD = 15.0;   // degrees, tune during testing
const unsigned long DEBOUNCE_MS = 3000;   // must persist 3s before alert
unsigned long badPostureStart = 0;
bool alerting = false;

float computePitch(sensors_event_t &accel, sensors_event_t &gyro, float prevPitch, float dt) {
  // Accelerometer-based pitch estimate
  float accelPitch = atan2(accel.acceleration.y,
                            sqrt(accel.acceleration.x * accel.acceleration.x +
                                 accel.acceleration.z * accel.acceleration.z)) * 180.0 / PI;

  // Complementary filter: blend gyro integration with accel estimate
  float gyroRate = gyro.gyro.x * 180.0 / PI; // deg/s
  float pitch = 0.98 * (prevPitch + gyroRate * dt) + 0.02 * accelPitch;
  return pitch;
}

void setup() {
  Serial.begin(115200);
  pinMode(ALERT_PIN, OUTPUT);
  Wire.begin();

  if (!mpuUpper.begin(MPU_UPPER_ADDR)) {
    Serial.println("Upper MPU6050 not found!");
    while (1) delay(10);
  }
  if (!mpuLower.begin(MPU_LOWER_ADDR)) {
    Serial.println("Lower MPU6050 not found!");
    while (1) delay(10);
  }

  mpuUpper.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuUpper.setGyroRange(MPU6050_RANGE_500_DEG);
  mpuLower.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpuLower.setGyroRange(MPU6050_RANGE_500_DEG);

  Serial.println("Sit in your natural, correct posture for calibration...");
  delay(3000);
  calibrateBaseline();
  Serial.print("Baseline spine angle set to: ");
  Serial.println(baselineAngle);

  lastTime = millis();
}

void calibrateBaseline() {
  sensors_event_t a1, g1, temp1, a2, g2, temp2;
  float sumAngle = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    mpuUpper.getEvent(&a1, &g1, &temp1);
    mpuLower.getEvent(&a2, &g2, &temp2);

    float pU = atan2(a1.acceleration.y,
                      sqrt(a1.acceleration.x * a1.acceleration.x + a1.acceleration.z * a1.acceleration.z)) * 180.0 / PI;
    float pL = atan2(a2.acceleration.y,
                      sqrt(a2.acceleration.x * a2.acceleration.x + a2.acceleration.z * a2.acceleration.z)) * 180.0 / PI;

    sumAngle += (pL - pU);
    delay(50);
  }
  baselineAngle = sumAngle / samples;
  pitchUpper = 0;
  pitchLower = 0;
}

void loop() {
  sensors_event_t accelU, gyroU, tempU;
  sensors_event_t accelL, gyroL, tempL;

  mpuUpper.getEvent(&accelU, &gyroU, &tempU);
  mpuLower.getEvent(&accelL, &gyroL, &tempL);

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  lastTime = now;

  pitchUpper = computePitch(accelU, gyroU, pitchUpper, dt);
  pitchLower = computePitch(accelL, gyroL, pitchLower, dt);

  float spineAngle = pitchLower - pitchUpper;
  float deviation = abs(spineAngle - baselineAngle);

  Serial.print("Spine angle: ");
  Serial.print(spineAngle);
  Serial.print(" | Deviation: ");
  Serial.println(deviation);

  if (deviation > DEVIATION_THRESHOLD) {
    if (badPostureStart == 0) {
      badPostureStart = now;
    } else if (now - badPostureStart > DEBOUNCE_MS && !alerting) {
      alerting = true;
      digitalWrite(ALERT_PIN, HIGH);
      Serial.println(">>> BAD POSTURE ALERT <<<");
    }
  } else {
    badPostureStart = 0;
    if (alerting) {
      alerting = false;
      digitalWrite(ALERT_PIN, LOW);
    }
  }

  delay(50); // ~20Hz sampling rate
} */ 
