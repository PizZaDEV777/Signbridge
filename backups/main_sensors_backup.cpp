#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ============================================================================
// SIGNBRIDGE - GLOVE HARDWARE & MOTION DETECTION FIRMWARE
// ============================================================================
// Features:
// 1. Flex Sensor monitoring (4 or 5 fingers with auto-calibration)
// 2. MPU6050 6-DOF IMU (Pitch, Roll, dynamic acceleration, shake/motion detection)
// 3. SSD1306 OLED Live Dashboard (Visual finger bars, wrist angles, gesture display)
// 4. Multi-mode Serial Output:
//    - Realtime Gesture commands compatible with laptop_audio_server.py
//    - Arduino IDE Serial Plotter mode for live graphing during hardware testing
//    - Hardware Diagnostic mode for raw ADC / sensor inspection
// 5. Interactive Calibration (send 'c' over Serial)
// ============================================================================

// ---------------------- PIN DEFINITIONS -------------------------------------
// I2C Pins (Arduino Uno):
// A4 = SDA (Shared by MPU6050 & SSD1306)
// A5 = SCL (Shared by MPU6050 & SSD1306)

// Flex Sensor Analog Pins (Using remaining analog pins on Uno):
#define NUM_FLEX 4   // Change to 5 if using Arduino Nano or Uno with A6
const uint8_t FLEX_PINS[NUM_FLEX] = {A0, A1, A2, A3};
// Pin mapping:
// A0: Thumb
// A1: Index
// A2: Middle
// A3: Ring / Pinky

const char* FINGER_NAMES[NUM_FLEX] = {"THUMB", "INDEX", "MID", "RING"};

#define LED_PIN LED_BUILTIN

// ---------------------- DISPLAY CONFIGURATION --------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayAvailable = false;

// ---------------------- MPU6050 CONFIGURATION --------------------------------
Adafruit_MPU6050 mpu;
bool mpuAvailable = false;

// ---------------------- SENSOR & CALIBRATION DATA ---------------------------
struct FlexSensor {
  uint16_t raw;
  uint16_t minVal;      // Calibrated unbent (straight) value
  uint16_t maxVal;      // Calibrated fully bent value
  uint8_t  bendPercent; // 0 - 100%
  bool     isBent;
};

FlexSensor flexSensors[NUM_FLEX];

// Default initial calibration thresholds (will auto-adjust or calibrate via 'c')
// Typical voltage divider (5V -> Flex -> Analog Pin -> 10k/22k Resistor -> GND):
// Straight is usually around 300-500, Bent is around 600-850 (or vice versa).
const uint16_t DEFAULT_MIN_ADC = 350;
const uint16_t DEFAULT_MAX_ADC = 700;
#define BEND_THRESHOLD_PERCENT 50 // Above 50% bend = finger considered bent

// ---------------------- MOTION & ORIENTATION DATA ---------------------------
struct MotionData {
  float ax, ay, az;         // Acceleration (m/s^2)
  float gx, gy, gz;         // Gyroscope (rad/s or deg/s)
  float pitch;              // Degrees (-90 to +90)
  float roll;               // Degrees (-180 to +180)
  float totalAccel;         // Magnitude of acceleration vector
  float dynamicAccel;       // Acceleration magnitude minus 1G (9.8 m/s^2)
  float gyroEnergy;         // Magnitude of rotational velocity
  bool  isMoving;           // Dynamic movement detected (shake, swipe, jerk)
};

MotionData motion;

// Wrist Orientation States
enum WristPose {
  POSE_FLAT,       // Palm neutral / horizontal
  POSE_UP,         // Wrist tilted upward (hand up / stop sign)
  POSE_DOWN,       // Wrist tilted downward
  POSE_TILT_LEFT,  // Rolled to the left
  POSE_TILT_RIGHT, // Rolled to the right
  POSE_PALM_UP     // Palm facing ceiling
};

WristPose currentPose = POSE_FLAT;

// ---------------------- GESTURE DEFINITIONS ---------------------------------
enum GestureId {
  GESTURE_NONE = 0,
  GESTURE_NEED_HELP,   // 1: Fist / Stop Hand Up -> "I need help"
  GESTURE_WANT_WATER,  // 2: Pointing forward / thumb to mouth -> "Please give me water"
  GESTURE_AM_OKAY,     // 3: Open flat hand -> "I am okay"
  GESTURE_THANK_YOU,   // 4: Thumbs up -> "Thank you"
  GESTURE_CALL_DOCTOR  // 5: Wave / rapid shake -> "Please call a doctor"
};

GestureId currentGesture = GESTURE_NONE;
GestureId lastTriggeredGesture = GESTURE_NONE;
unsigned long gestureStartTime = 0;
unsigned long lastTriggerTime = 0;
const unsigned long GESTURE_HOLD_DURATION_MS = 350; // Must hold gesture 350ms to confirm
const unsigned long TRIGGER_COOLDOWN_MS = 2500;      // Don't repeat same audio within 2.5s

// ---------------------- OPERATING MODES -------------------------------------
enum OutputMode {
  MODE_NORMAL,       // Readable status & audio commands
  MODE_PLOTTER,      // CSV format for Arduino IDE Serial Plotter
  MODE_DIAGNOSTIC    // Detailed raw hardware debug numbers
};

OutputMode currentMode = MODE_NORMAL;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void readFlexSensors();
void readMPU6050();
void classifyOrientation();
void detectGesture();
void triggerGestureAudio(GestureId g);
void updateDisplay();
void printSerialOutput();
void handleSerialCommands();
void runAutoCalibration();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n=================================================="));
  Serial.println(F("       SIGNBRIDGE - GLOVE HARDWARE TEST SUITE     "));
  Serial.println(F("=================================================="));
  Serial.println(F("[*] Initializing I2C Bus..."));
  Wire.begin();

  // Initialize Flex Sensors with default calibration
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    pinMode(FLEX_PINS[i], INPUT);
    flexSensors[i].minVal = DEFAULT_MIN_ADC;
    flexSensors[i].maxVal = DEFAULT_MAX_ADC;
    flexSensors[i].bendPercent = 0;
    flexSensors[i].isBent = false;
  }

  // 1. Initialize OLED Display (0x3C)
  Serial.print(F("[*] Checking OLED Display (0x3C)... "));
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    displayAvailable = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("SIGNBRIDGE GLOVE"));
    display.setCursor(10, 25);
    display.println(F("Hardware Init..."));
    display.display();
    Serial.println(F("CONNECTED [OK]"));
  } else {
    displayAvailable = false;
    Serial.println(F("NOT FOUND (Check I2C A4/A5 or Address)"));
  }

  // 2. Initialize MPU6050 (0x68)
  Serial.print(F("[*] Checking MPU6050 IMU (0x68)... "));
  if (mpu.begin()) {
    mpuAvailable = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println(F("CONNECTED [OK]"));
    if (displayAvailable) {
      display.setCursor(10, 40);
      display.println(F("MPU6050: READY"));
      display.display();
    }
  } else {
    mpuAvailable = false;
    Serial.println(F("FAILED! (Check SDA->A4, SCL->A5, VCC 5V/3.3V, GND)"));
    if (displayAvailable) {
      display.setCursor(10, 40);
      display.println(F("MPU6050: ERROR!"));
      display.display();
    }
  }

  delay(1200);

  Serial.println(F("\n[✓] System Initialized!"));
  Serial.println(F("Commands:"));
  Serial.println(F("  'c' -> Start Flex Sensor Calibration (Straight vs Fist)"));
  Serial.println(F("  'p' -> Toggle Arduino Serial Plotter Mode"));
  Serial.println(F("  'd' -> Toggle Raw Diagnostic Mode"));
  Serial.println(F("  '1'..'5' -> Simulate Gestures for Audio Bridge"));
  Serial.println(F("==================================================\n"));

  digitalWrite(LED_PIN, LOW);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  handleSerialCommands();
  readFlexSensors();
  readMPU6050();
  classifyOrientation();
  detectGesture();
  updateDisplay();
  printSerialOutput();

  delay(40); // ~25Hz refresh rate is smooth and responsive for gestures
}

// ============================================================================
// FLEX SENSORS READING & MAPPING
// ============================================================================
void readFlexSensors() {
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    // Read raw ADC value (0 - 1023)
    uint16_t reading = analogRead(FLEX_PINS[i]);
    flexSensors[i].raw = reading;

    // Constrain to calibrated min/max range
    uint16_t low = min(flexSensors[i].minVal, flexSensors[i].maxVal);
    uint16_t high = max(flexSensors[i].minVal, flexSensors[i].maxVal);

    if (reading < low) reading = low;
    if (reading > high) reading = high;

    // Calculate percentage bend (handles both normal and inverted resistor dividers)
    uint8_t pct;
    if (flexSensors[i].maxVal > flexSensors[i].minVal) {
      pct = map(reading, flexSensors[i].minVal, flexSensors[i].maxVal, 0, 100);
    } else {
      pct = map(reading, flexSensors[i].minVal, flexSensors[i].maxVal, 100, 0);
    }

    // Apply exponential smoothing filter for clean, jitter-free response
    flexSensors[i].bendPercent = (flexSensors[i].bendPercent * 3 + pct) / 4;

    // Determine binary state with hysteresis
    if (flexSensors[i].bendPercent > BEND_THRESHOLD_PERCENT + 5) {
      flexSensors[i].isBent = true;
    } else if (flexSensors[i].bendPercent < BEND_THRESHOLD_PERCENT - 5) {
      flexSensors[i].isBent = false;
    }
  }
}

// ============================================================================
// MPU6050 MOTION & IMU READINGS
// ============================================================================
void readMPU6050() {
  if (!mpuAvailable) {
    motion.pitch = 0;
    motion.roll = 0;
    motion.dynamicAccel = 0;
    motion.isMoving = false;
    return;
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  motion.ax = a.acceleration.x;
  motion.ay = a.acceleration.y;
  motion.az = a.acceleration.z;

  motion.gx = g.gyro.x;
  motion.gy = g.gyro.y;
  motion.gz = g.gyro.z;

  // Calculate Pitch and Roll (in degrees) from accelerometer
  // Roll: Tilt left / right (-180 to +180 deg)
  motion.roll = atan2(motion.ay, motion.az) * 180.0 / PI;

  // Pitch: Tilt forward / backward (-90 to +90 deg)
  float denom = sqrt(motion.ay * motion.ay + motion.az * motion.az);
  motion.pitch = atan2(-motion.ax, denom) * 180.0 / PI;

  // Total acceleration magnitude
  motion.totalAccel = sqrt(motion.ax * motion.ax + motion.ay * motion.ay + motion.az * motion.az);

  // Dynamic acceleration (removes Earth gravity ~9.81 m/s^2)
  motion.dynamicAccel = abs(motion.totalAccel - 9.81);

  // Rotational energy from gyroscope (rad/s)
  motion.gyroEnergy = sqrt(motion.gx * motion.gx + motion.gy * motion.gy + motion.gz * motion.gz);

  // Motion detection threshold (shake, wave, or rapid wrist movement)
  if (motion.dynamicAccel > 3.8 || motion.gyroEnergy > 2.2) {
    motion.isMoving = true;
  } else if (motion.dynamicAccel < 1.2 && motion.gyroEnergy < 0.8) {
    motion.isMoving = false;
  }
}

// ============================================================================
// WRIST POSE CLASSIFICATION
// ============================================================================
void classifyOrientation() {
  if (!mpuAvailable) {
    currentPose = POSE_FLAT;
    return;
  }

  // Pitch-based tilts:
  if (motion.pitch > 35.0) {
    currentPose = POSE_UP;          // Hand pointing upward / stop
  } else if (motion.pitch < -35.0) {
    currentPose = POSE_DOWN;        // Hand pointing downward
  } 
  // Roll-based tilts:
  else if (motion.roll > 40.0 && motion.roll < 140.0) {
    currentPose = POSE_TILT_RIGHT;  // Tilted right
  } else if (motion.roll < -40.0 && motion.roll > -140.0) {
    currentPose = POSE_TILT_LEFT;   // Tilted left
  } else if (abs(motion.roll) > 140.0) {
    currentPose = POSE_PALM_UP;     // Palm facing up
  } else {
    currentPose = POSE_FLAT;        // Palm horizontal / neutral
  }
}

// ============================================================================
// GESTURE RECOGNITION ENGINE
// ============================================================================
void detectGesture() {
  bool thumb = flexSensors[0].isBent;
  bool index = flexSensors[1].isBent;
  bool mid   = flexSensors[2].isBent;
  bool ring  = flexSensors[3].isBent;

  GestureId candidate = GESTURE_NONE;

  // -------------------------------------------------------------
  // GESTURE 5: CALL DOCTOR / EMERGENCY
  // Dynamic motion: Hand waving or vigorous wrist shake
  // -------------------------------------------------------------
  if (motion.isMoving && motion.gyroEnergy > 2.5) {
    candidate = GESTURE_CALL_DOCTOR;
  }
  // -------------------------------------------------------------
  // GESTURE 1: I NEED HELP
  // Fist formed (all fingers bent) OR Hand raised Up in stop/fist
  // -------------------------------------------------------------
  else if (thumb && index && mid && ring) {
    candidate = GESTURE_NEED_HELP;
  }
  // -------------------------------------------------------------
  // GESTURE 2: PLEASE GIVE ME WATER
  // Index finger extended (straight), others bent (pointing forward)
  // -------------------------------------------------------------
  else if (!index && mid && ring) {
    candidate = GESTURE_WANT_WATER;
  }
  // -------------------------------------------------------------
  // GESTURE 4: THANK YOU
  // Thumbs up (Thumb straight, all other fingers bent)
  // -------------------------------------------------------------
  else if (!thumb && index && mid && ring) {
    candidate = GESTURE_THANK_YOU;
  }
  // -------------------------------------------------------------
  // GESTURE 3: I AM OKAY
  // Open hand flat (all fingers straight, wrist flat)
  // -------------------------------------------------------------
  else if (!thumb && !index && !mid && !ring && currentPose == POSE_FLAT) {
    candidate = GESTURE_AM_OKAY;
  }

  // Debouncing & Hold Time logic:
  // Candidate gesture must remain steady for GESTURE_HOLD_DURATION_MS
  if (candidate != GESTURE_NONE) {
    if (candidate != currentGesture) {
      currentGesture = candidate;
      gestureStartTime = millis();
    } else {
      if (millis() - gestureStartTime >= GESTURE_HOLD_DURATION_MS) {
        // Trigger if it's different from the last triggered gesture, or cooldown elapsed
        if (candidate != lastTriggeredGesture || (millis() - lastTriggerTime > TRIGGER_COOLDOWN_MS)) {
          triggerGestureAudio(candidate);
          lastTriggeredGesture = candidate;
          lastTriggerTime = millis();
        }
      }
    }
  } else {
    currentGesture = GESTURE_NONE;
  }
}

// ============================================================================
// AUDIO & GESTURE TRIGGER
// ============================================================================
void triggerGestureAudio(GestureId g) {
  digitalWrite(LED_PIN, HIGH);

  switch (g) {
    case GESTURE_NEED_HELP:
      Serial.println(F("GESTURE:NEED_HELP"));
      break;
    case GESTURE_WANT_WATER:
      Serial.println(F("GESTURE:WANT_WATER"));
      break;
    case GESTURE_AM_OKAY:
      Serial.println(F("GESTURE:AM_OKAY"));
      break;
    case GESTURE_THANK_YOU:
      Serial.println(F("GESTURE:THANK_YOU"));
      break;
    case GESTURE_CALL_DOCTOR:
      Serial.println(F("GESTURE:CALL_DOCTOR"));
      break;
    default:
      break;
  }

  digitalWrite(LED_PIN, LOW);
}

// ============================================================================
// OLED DISPLAY UPDATE
// ============================================================================
void updateDisplay() {
  if (!displayAvailable) return;

  display.clearDisplay();

  // Header Bar
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("SIGNBRIDGE"));

  display.setCursor(76, 0);
  if (motion.isMoving) {
    display.print(F("[MOTION]"));
  } else {
    display.print(F("[STABLE]"));
  }
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // Visual Finger Bend Bars
  // Draw 4 vertical or horizontal bars showing bend level
  display.setCursor(0, 13);
  display.print(F("F:"));
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    int x = 16 + (i * 28);
    int y = 13;
    // Box
    display.drawRect(x, y, 24, 7, SSD1306_WHITE);
    // Fill gauge according to bend %
    int fillW = map(flexSensors[i].bendPercent, 0, 100, 0, 22);
    if (fillW > 0) {
      display.fillRect(x + 1, y + 1, fillW, 5, SSD1306_WHITE);
    }
  }

  // Wrist Orientation & Angles
  display.setCursor(0, 24);
  display.print(F("P:"));
  display.print((int)motion.pitch);
  display.print((char)247); // Degree symbol
  display.print(F(" R:"));
  display.print((int)motion.roll);
  display.print((char)247);

  display.setCursor(76, 24);
  switch (currentPose) {
    case POSE_UP:         display.print(F("UP")); break;
    case POSE_DOWN:       display.print(F("DOWN")); break;
    case POSE_TILT_LEFT:  display.print(F("LEFT")); break;
    case POSE_TILT_RIGHT: display.print(F("RIGHT")); break;
    case POSE_PALM_UP:    display.print(F("PLM-UP")); break;
    default:              display.print(F("FLAT")); break;
  }

  display.drawLine(0, 34, 127, 34, SSD1306_WHITE);

  // Detected Gesture Banner (Prominent Text)
  display.setCursor(0, 38);
  display.print(F("Gesture:"));

  display.setTextSize(2);
  display.setCursor(4, 48);

  GestureId active = (lastTriggeredGesture != GESTURE_NONE && (millis() - lastTriggerTime < 2000)) 
                     ? lastTriggeredGesture : currentGesture;

  switch (active) {
    case GESTURE_NEED_HELP:
      display.print(F("HELP!"));
      break;
    case GESTURE_WANT_WATER:
      display.print(F("WATER"));
      break;
    case GESTURE_AM_OKAY:
      display.print(F("I'M OKAY"));
      break;
    case GESTURE_THANK_YOU:
      display.print(F("THANK YOU"));
      break;
    case GESTURE_CALL_DOCTOR:
      display.print(F("DOCTOR!"));
      break;
    default:
      display.setTextSize(1);
      display.setCursor(20, 52);
      display.print(F("Listening..."));
      break;
  }

  display.display();
}

// ============================================================================
// SERIAL OUTPUT & MODES
// ============================================================================
void printSerialOutput() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 100) return; // Limit Serial output to 10Hz
  lastPrint = millis();

  if (currentMode == MODE_PLOTTER) {
    // Format specifically for Arduino IDE Serial Plotter:
    // "F1:val F2:val F3:val F4:val Pitch:val Roll:val Motion:val"
    Serial.print(F("Thumb:"));  Serial.print(flexSensors[0].bendPercent); Serial.print(F(","));
    Serial.print(F("Index:"));  Serial.print(flexSensors[1].bendPercent); Serial.print(F(","));
    Serial.print(F("Middle:")); Serial.print(flexSensors[2].bendPercent); Serial.print(F(","));
    Serial.print(F("Ring:"));   Serial.print(flexSensors[3].bendPercent); Serial.print(F(","));
    Serial.print(F("Pitch:"));  Serial.print((int)motion.pitch);          Serial.print(F(","));
    Serial.print(F("Roll:"));   Serial.print((int)motion.roll);           Serial.print(F(","));
    Serial.print(F("Motion:")); Serial.println(motion.isMoving ? 100 : 0);
  } 
  else if (currentMode == MODE_DIAGNOSTIC) {
    // Hardware Diagnostic format:
    Serial.print(F("[RAW ADC] "));
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      Serial.print(FINGER_NAMES[i]);
      Serial.print(F(":"));
      Serial.print(flexSensors[i].raw);
      Serial.print(F(" ("));
      Serial.print(flexSensors[i].bendPercent);
      Serial.print(F("%) "));
    }
    Serial.print(F("| P:"));
    Serial.print((int)motion.pitch);
    Serial.print(F(" R:"));
    Serial.print((int)motion.roll);
    Serial.print(F(" DynG:"));
    Serial.print(motion.dynamicAccel, 1);
    Serial.print(F(" Mov:"));
    Serial.println(motion.isMoving ? F("YES") : F("NO"));
  }
}

// ============================================================================
// INTERACTIVE SERIAL COMMANDS
// ============================================================================
void handleSerialCommands() {
  if (!Serial.available()) return;
  char cmd = Serial.read();

  if (cmd == 'c' || cmd == 'C') {
    runAutoCalibration();
  } else if (cmd == 'p' || cmd == 'P') {
    if (currentMode == MODE_PLOTTER) {
      currentMode = MODE_NORMAL;
      Serial.println(F("[*] Switched to NORMAL mode"));
    } else {
      currentMode = MODE_PLOTTER;
      Serial.println(F("[*] Switched to SERIAL PLOTTER mode (Open Tools > Serial Plotter)"));
    }
  } else if (cmd == 'd' || cmd == 'D') {
    if (currentMode == MODE_DIAGNOSTIC) {
      currentMode = MODE_NORMAL;
      Serial.println(F("[*] Switched to NORMAL mode"));
    } else {
      currentMode = MODE_DIAGNOSTIC;
      Serial.println(F("[*] Switched to RAW HARDWARE DIAGNOSTIC mode"));
    }
  } else if (cmd >= '1' && cmd <= '5') {
    triggerGestureAudio((GestureId)(cmd - '0'));
  }
}

// ============================================================================
// AUTOMATIC FLEX SENSOR CALIBRATION ROUTINE
// ============================================================================
void runAutoCalibration() {
  Serial.println(F("\n=================================================="));
  Serial.println(F("      FLEX SENSOR AUTO-CALIBRATION PROCEDURE      "));
  Serial.println(F("=================================================="));

  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 5);
    display.println(F("CALIBRATION MODE"));
    display.println(F("-----------------"));
    display.println(F("Step 1:"));
    display.println(F("HOLD HAND OPEN"));
    display.println(F("& STRAIGHT..."));
    display.display();
  }

  Serial.println(F("[STEP 1/2]: Keep all fingers completely FLAT & STRAIGHT."));
  Serial.println(F("Sampling in 3 seconds..."));
  for (int i = 3; i > 0; i--) {
    Serial.print(i); Serial.print(F("... "));
    delay(1000);
  }
  Serial.println(F("SAMPLING STRAIGHT POSITIONS!"));

  uint32_t straightSum[NUM_FLEX] = {0};
  const int SAMPLES = 40;
  for (int s = 0; s < SAMPLES; s++) {
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      straightSum[i] += analogRead(FLEX_PINS[i]);
    }
    delay(25);
  }

  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    flexSensors[i].minVal = straightSum[i] / SAMPLES;
    Serial.print(F("  "));
    Serial.print(FINGER_NAMES[i]);
    Serial.print(F(" Flat ADC = "));
    Serial.println(flexSensors[i].minVal);
  }

  if (displayAvailable) {
    display.clearDisplay();
    display.setCursor(0, 5);
    display.println(F("CALIBRATION MODE"));
    display.println(F("-----------------"));
    display.println(F("Step 2:"));
    display.println(F("CLOSE INTO A"));
    display.println(F("TIGHT FIST..."));
    display.display();
  }

  Serial.println(F("\n[STEP 2/2]: Close your hand into a TIGHT FIST (Bending all fingers)."));
  Serial.println(F("Sampling in 3 seconds..."));
  for (int i = 3; i > 0; i--) {
    Serial.print(i); Serial.print(F("... "));
    delay(1000);
  }
  Serial.println(F("SAMPLING BENT POSITIONS!"));

  uint32_t bentSum[NUM_FLEX] = {0};
  for (int s = 0; s < SAMPLES; s++) {
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      bentSum[i] += analogRead(FLEX_PINS[i]);
    }
    delay(25);
  }

  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    flexSensors[i].maxVal = bentSum[i] / SAMPLES;
    Serial.print(F("  "));
    Serial.print(FINGER_NAMES[i]);
    Serial.print(F(" Bent ADC = "));
    Serial.println(flexSensors[i].maxVal);
  }

  Serial.println(F("\n[✓] Calibration Complete! Dynamic ranges saved."));
  Serial.println(F("==================================================\n"));

  if (displayAvailable) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println(F("CALIBRATION DONE!"));
    display.println(F("Ready to detect!"));
    display.display();
    delay(1500);
  }
}

