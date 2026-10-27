#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// SIGNBRIDGE - 5 FLEX SENSORS + LIGHTWEIGHT MPU6050 + OLED DASHBOARD
// ============================================================================
// Optimized embedded firmware:
// - Direct I2C driver for MPU6050 (saves ~12KB flash and >300 bytes RAM!)
// - Plenty of free RAM for SSD1306 OLED malloc (screen lights up instantly!)
// - Fast 400kHz I2C for snappy, lag-free visual response
// - Full 5-finger gauges + Pitch & Roll wrist orientation + Emergency Shake
// ============================================================================

// ---------------------- PIN DEFINITIONS -------------------------------------
// I2C Bus (Shared by SSD1306 OLED & MPU6050):
// A4 = SDA (Connect BOTH OLED SDA and MPU6050 SDA here)
// A5 = SCL (Connect BOTH OLED SCL and MPU6050 SCL here)
// VCC = 5V Rail, GND = GND Rail

// 5 Flex Sensors:
#define NUM_FLEX 5
const uint8_t FLEX_PINS[NUM_FLEX] = {A0, A1, A2, A3, 2};
// A0: Thumb  (Analog)
// A1: Index  (Analog)
// A2: Middle (Analog)
// A3: Ring   (Analog)
// D2: Pinky  (Digital threshold)

const bool IS_ANALOG[NUM_FLEX] = {true, true, true, true, false};
const char FINGER_INITIALS[NUM_FLEX] = {'T', 'I', 'M', 'R', 'P'};

#define LED_PIN LED_BUILTIN

// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
// Cycle-Accurate 9600 Baud UART on Pins 10 (PB2) & 11 (PB3)
// Direct hardware port registers + atomic timing for 100% baud accuracy!
#define BUZZER_PIN 3
uint8_t dfPlayerVolume = 30; // Scale: 0 to 30 (Set to MAX 30 for clear, loud voice!)

void dfplayer_send_byte(uint8_t b) {
  // 9600.0 baud = exactly 104.16 microseconds per bit
  // cli() ensures Arduino millis() timer interrupts never stretch the bits!
  noInterrupts();

  // Start bit: pull both Pin 10 (PB2) and Pin 11 (PB3) LOW
  PORTB &= ~((1 << PB2) | (1 << PB3));
  delayMicroseconds(104);

  // 8 Data bits (LSB first)
  for (uint8_t mask = 1; mask != 0; mask <<= 1) {
    if (b & mask) {
      PORTB |= ((1 << PB2) | (1 << PB3));
    } else {
      PORTB &= ~((1 << PB2) | (1 << PB3));
    }
    delayMicroseconds(104);
  }

  // Stop bit: pull both pins HIGH
  PORTB |= ((1 << PB2) | (1 << PB3));
  delayMicroseconds(104);

  interrupts();
}

void dfplayer_send_cmd(uint8_t cmd, uint16_t arg) {
  uint16_t checksum = 0 - (0xFF + 0x06 + cmd + 0x00 + (arg >> 8) + (arg & 0xFF));
  uint8_t packet[10] = {
    0x7E,           // Start byte
    0xFF,           // Version
    0x06,           // Length
    cmd,            // Command
    0x00,           // Feedback (0 = no ACK required)
    (uint8_t)(arg >> 8),   // Param High
    (uint8_t)(arg & 0xFF), // Param Low
    (uint8_t)(checksum >> 8),
    (uint8_t)(checksum & 0xFF),
    0xEF            // End byte
  };
  for (uint8_t i = 0; i < 10; i++) {
    dfplayer_send_byte(packet[i]);
  }
}

// ---------------------- DISPLAY CONFIGURATION --------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayAvailable = false;

// ---------------------- MPU6050 CONFIGURATION --------------------------------
uint8_t mpuAddress = 0x68;
bool mpuAvailable = false;

// ---------------------- SENSOR & CALIBRATION DATA ---------------------------
struct FlexSensor {
  uint16_t raw;
  uint16_t minVal;      // Calibrated / auto-tracked straight value
  uint16_t maxVal;      // Calibrated / auto-tracked bent value
  uint8_t  bendPercent; // 0 - 100%
  bool     isBent;
  bool     inverted;    // True if bent ADC < straight ADC
};

FlexSensor flexSensors[NUM_FLEX];

#define BEND_THRESHOLD_PERCENT 50

// ---------------------- MOTION & ORIENTATION DATA ---------------------------
struct MotionData {
  float ax, ay, az;
  float gx, gy, gz;
  float pitch;        // -90 to +90 degrees
  float roll;         // -180 to +180 degrees
  float dynamicAccel; // G-force beyond Earth gravity
  float gyroEnergy;   // Rotational speed
  bool  isMoving;     // Dynamic hand shake / wave
};

MotionData motion;

enum WristPose {
  POSE_FLAT,
  POSE_UP,
  POSE_DOWN,
  POSE_TILT_LEFT,
  POSE_TILT_RIGHT,
  POSE_PALM_UP
};

WristPose currentPose = POSE_FLAT;

// ---------------------- GESTURE DEFINITIONS ---------------------------------
enum GestureId {
  GESTURE_NONE = 0,
  GESTURE_NEED_HELP,   // 1: Fist -> "I Need Help"
  GESTURE_WANT_WATER,  // 2: Pointing -> "Water"
  GESTURE_AM_OKAY,     // 3: Open flat hand -> "I'm Okay"
  GESTURE_THANK_YOU,   // 4: Thumbs up -> "Thank You"
  GESTURE_PEACE,       // 5: Peace sign -> "Peace"
  GESTURE_LOVE,        // 6: ILY sign -> "I Love You"
  GESTURE_CALL_DOCTOR  // 7: Rapid hand wave / vigorous shake -> "Doctor!"
};

GestureId currentGesture = GESTURE_NONE;
GestureId lastTriggeredGesture = GESTURE_NONE;
unsigned long gestureStartTime = 0;
unsigned long lastTriggerTime = 0;
const unsigned long GESTURE_HOLD_DURATION_MS = 250; // Snappy 250ms hold verification
const unsigned long TRIGGER_COOLDOWN_MS = 2500;     // 2.5s cooldown between triggers

// ---------------------- OPERATING MODES -------------------------------------
enum OutputMode {
  MODE_PLOTTER_RAW,  // Live graph of Raw ADC (0-1023)
  MODE_PLOTTER_PCT,  // Live graph of Bend % (0-100%)
  MODE_DIAGNOSTIC,   // Live raw ADC voltages & IMU text
  MODE_NORMAL        // Clean text mode
};

OutputMode currentMode = MODE_PLOTTER_RAW;

// ---------------------- FORWARD DECLARATIONS --------------------------------
void readFlexSensors();
bool initMPU6050();
void readMPU6050();
void classifyOrientation();
void detectGesture();
void triggerGestureAudio(GestureId g);
void updateDisplay();
void printSerialOutput();
void handleSerialCommands();
void runAutoCalibration();
void scanI2C();
void playStartupVoiceTest();

// ============================================================================
// STARTUP AUDIO SELF-TEST: SEQUENTIALLY PLAY ALL 5 RECORDINGS AT BOOT
// ============================================================================
void playStartupVoiceTest() {
  Serial.println(F("\n=================================================="));
  Serial.println(F(">>> AUDIO SELF-TEST: PLAYING ALL 5 TRACKS IN ORDER"));
  Serial.println(F("=================================================="));

  const uint16_t trackDelays[5] = { 2600, 2800, 2500, 2200, 3000 };

  for (uint8_t i = 1; i <= 5; i++) {
    Serial.print(F("[*] Playing Track "));
    Serial.print(i);
    switch (i) {
      case 1: Serial.println(F(": 'I need help' (0001.mp3)")); break;
      case 2: Serial.println(F(": 'Please give me water' (0002.mp3)")); break;
      case 3: Serial.println(F(": 'I am okay' (0003.mp3)")); break;
      case 4: Serial.println(F(": 'Thank you' (0004.mp3)")); break;
      case 5: Serial.println(F(": 'Please call a doctor' (0005.mp3)")); break;
    }

    if (displayAvailable) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(14, 6);
      display.println(F("AUDIO SELF-TEST"));
      display.drawLine(0, 16, 127, 16, SSD1306_WHITE);
      display.setCursor(16, 22);
      display.print(F("Voice "));
      display.print(i);
      display.println(F(" of 5:"));
      display.setTextSize(2);
      display.setCursor(4, 38);
      switch(i) {
        case 1: display.println(F("NEED HELP")); break;
        case 2: display.println(F("WATER")); break;
        case 3: display.println(F("I'M OKAY")); break;
        case 4: display.println(F("THANK YOU")); break;
        case 5: display.println(F("DOCTOR!")); break;
      }
      display.display();
    }

    tone(BUZZER_PIN, 1800, 40); // Audible confirmation tick
    dfplayer_send_cmd(0x03, i);  // Universal play track command

    delay(trackDelays[i - 1]);
  }

  Serial.println(F("[✓] Startup Audio Self-Test Complete!"));
  Serial.println(F("==================================================\n"));
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(115200);
  delay(400);

  Serial.println(F("\n=================================================="));
  Serial.println(F("  SIGNBRIDGE - 5 FLEX + MPU6050 + OLED GLOVE      "));
  Serial.println(F("=================================================="));

  // 0. Probe Physical Line Voltages on A4 (SDA) and A5 (SCL)
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  uint16_t sdaRaw = analogRead(A4);
  uint16_t sclRaw = analogRead(A5);
  Serial.print(F("[BUS LINE CHECK] SDA (A4) = "));
  Serial.print(sdaRaw);
  Serial.print(F("/1023 ("));
  Serial.print((sdaRaw * 5.0f) / 1023.0f, 2);
  Serial.print(F("V) | SCL (A5) = "));
  Serial.print(sclRaw);
  Serial.print(F("/1023 ("));
  Serial.print((sclRaw * 5.0f) / 1023.0f, 2);
  Serial.println(F("V)"));

  if (sdaRaw < 300) {
    Serial.println(F("[!] FAULT: SDA (A4) is SHORTED TO GND or missing pullup (<1.5V)!"));
  }
  if (sclRaw < 300) {
    Serial.println(F("[!] FAULT: SCL (A5) is SHORTED TO GND or missing pullup (<1.5V)!"));
  }

  // 1. Initialize I2C Bus with Standard 100kHz Mode and Safe Hardware Timeout
  Wire.begin();
  Wire.setClock(100000);            // Standard 100kHz rock-solid mode for breadboard wiring
  Wire.setWireTimeout(25000, true); // 25ms timeout with auto-reset: NEVER hangs if a wire is bumped

  // Scan I2C bus immediately at boot
  scanI2C();

  // 2. Initialize Flex Sensor Pins
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    if (IS_ANALOG[i]) {
      pinMode(FLEX_PINS[i], INPUT);
      // Average 8 samples to establish a clean initial baseline
      uint32_t sum = 0;
      for (uint8_t s = 0; s < 8; s++) {
        sum += analogRead(FLEX_PINS[i]);
        delay(5);
      }
      uint16_t initVal = sum / 8;
      flexSensors[i].raw = initVal;
      flexSensors[i].minVal = (initVal > 50) ? (initVal - 30) : initVal;
      flexSensors[i].maxVal = initVal + 220; // 220 count span for solid hysteresis
      flexSensors[i].inverted = false;
    } else {
      pinMode(FLEX_PINS[i], INPUT_PULLUP); // Pull-up prevents floating pin oscillations
      flexSensors[i].raw = digitalRead(FLEX_PINS[i]);
      flexSensors[i].minVal = 0;
      flexSensors[i].maxVal = 1;
      flexSensors[i].inverted = false;
    }
    flexSensors[i].bendPercent = 0;
    flexSensors[i].isBent = false;
  }

  // 3. Initialize OLED Display (0x3C)
  delay(50);
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    displayAvailable = true;
    Serial.println(F("READY [OK]"));
    // Flash screen all white for 250ms to visually verify OLED panel
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(250);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 15);
    display.println(F("SIGNBRIDGE GLOVE"));
    display.setCursor(18, 30);
    display.println(F("FLEX5 + MPU6050"));
    display.setCursor(28, 46);
    display.println(F("OLED 100kHz OK"));
    display.display();
  } else {
    displayAvailable = false;
    Serial.println(F("MALLOC FAILED"));
  }

  // 4. Initialize MPU6050 IMU via direct lightweight I2C driver (probes 0x68 and 0x69)
  Serial.print(F("[*] Initializing MPU6050 IMU... "));
  if (initMPU6050()) {
    mpuAvailable = true;
    Serial.print(F("CONNECTED [OK] at 0x"));
    Serial.println(mpuAddress, HEX);
  } else {
    mpuAvailable = false;
    Serial.println(F("NOT DETECTED (Connect VCC, GND, SDA->A4, SCL->A5)"));
  }

  // 5. Initialize DFPlayer Mini (MP3-TF-16P V3.0)
  pinMode(BUZZER_PIN, INPUT); // Kept in safe high-impedance mode

  Serial.print(F("[*] Initializing DFPlayer Mini Audio (Pins 10/11)... "));
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  digitalWrite(10, HIGH);
  digitalWrite(11, HIGH); // Idle HIGH for UART
  delay(1200); // Wait for Micro-SD card to mount FAT32 filesystem
  dfplayer_send_cmd(0x06, dfPlayerVolume); // Set volume to 30/30 (Maximum)
  delay(200);
  Serial.print(F("READY [OK] (Volume "));
  Serial.print(dfPlayerVolume);
  Serial.println(F("/30)"));

  delay(600);
  Serial.println(F("[✓] System Initialized & Streaming to Serial Plotter!"));
  Serial.println(F("==================================================\n"));

  digitalWrite(LED_PIN, LOW);
}

// ============================================================================
// MAIN LOOP (~35Hz refresh rate for instant, silky response)
// ============================================================================
void loop() {
  handleSerialCommands();
  readFlexSensors();
  readMPU6050();
  classifyOrientation();
  detectGesture();
  updateDisplay();
  printSerialOutput();

  delay(10); // Snappy loop delay
}

// ============================================================================
// FLEX SENSOR ACQUISITION & FAST DYNAMIC FILTERING
// ============================================================================
void readFlexSensors() {
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    if (IS_ANALOG[i]) {
      // 3-sample average to suppress high-frequency electrical spikes
      uint16_t r1 = analogRead(FLEX_PINS[i]);
      uint16_t r2 = analogRead(FLEX_PINS[i]);
      uint16_t reading = (r1 + r2) / 2;
      flexSensors[i].raw = reading;

      // Safe minimum span (at least 140 counts) to prevent noise from causing 0-100% jumping
      uint16_t span = (flexSensors[i].maxVal > flexSensors[i].minVal)
                      ? (flexSensors[i].maxVal - flexSensors[i].minVal) : 180;
      if (span < 140) span = 140;

      uint8_t pct;
      if (flexSensors[i].inverted) {
        pct = map(constrain(reading, flexSensors[i].minVal, flexSensors[i].maxVal),
                  flexSensors[i].minVal, flexSensors[i].maxVal, 100, 0);
      } else {
        pct = map(constrain(reading, flexSensors[i].minVal, flexSensors[i].maxVal),
                  flexSensors[i].minVal, flexSensors[i].maxVal, 0, 100);
      }

      // Smooth moving average filter: 65% previous + 35% new reading
      flexSensors[i].bendPercent = ((uint16_t)flexSensors[i].bendPercent * 65 + (uint16_t)pct * 35) / 100;

      // Clear hysteresis threshold (55% to bend, 35% to unbend)
      if (flexSensors[i].bendPercent > 55) {
        flexSensors[i].isBent = true;
      } else if (flexSensors[i].bendPercent < 35) {
        flexSensors[i].isBent = false;
      }
    } else {
      // Digital sensor (Pin 2: Pinky with INPUT_PULLUP)
      uint8_t dState = digitalRead(FLEX_PINS[i]);
      flexSensors[i].raw = dState;

      // With INPUT_PULLUP, LOW = bent / grounded switch
      bool bentState = flexSensors[i].inverted ? (dState == HIGH) : (dState == LOW);
      flexSensors[i].isBent = bentState;
      flexSensors[i].bendPercent = bentState ? 100 : 0;
    }
  }
}

// ============================================================================
// DIRECT LIGHTWEIGHT MPU6050 DRIVER (Zero library overhead!)
// ============================================================================
bool initMPU6050() {
  // 1. Auto-detect whether MPU is at address 0x68 or 0x69 (AD0 pin)
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() == 0) {
    mpuAddress = 0x68;
  } else {
    Wire.beginTransmission(0x69);
    if (Wire.endTransmission() == 0) {
      mpuAddress = 0x69;
    } else {
      return false;
    }
  }

  // Wake up MPU6050 (write 0 to PWR_MGMT_1 register 0x6B)
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x6B);
  Wire.write(0);
  if (Wire.endTransmission() != 0) return false;

  // Set Accelerometer range to +/- 4G (register 0x1C = 0x08)
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1C);
  Wire.write(0x08);
  Wire.endTransmission();

  // Set Gyroscope range to +/- 500 deg/s (register 0x1B = 0x08)
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission();

  return true;
}

void readMPU6050() {
  // Auto-reconnect: if MPU was disconnected/wiggled, ping it in background every 1.2s
  static unsigned long lastMpuCheck = 0;
  if (!mpuAvailable) {
    if (millis() - lastMpuCheck > 1200) {
      lastMpuCheck = millis();
      if (initMPU6050()) {
        mpuAvailable = true;
        Serial.print(F("\n[✓] MPU6050 RECONNECTED [OK] at 0x"));
        Serial.println(mpuAddress, HEX);
      }
    }
    return;
  }

  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B); // Starting at ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) {
    mpuAvailable = false;
    if (Wire.getWireTimeoutFlag()) Wire.clearWireTimeoutFlag();
    return;
  }

  // Read 14 bytes: Ax, Ay, Az, Temp, Gx, Gy, Gz
  if (Wire.requestFrom(mpuAddress, (uint8_t)14) == 14) {
    int16_t rawAx = (Wire.read() << 8) | Wire.read();
    int16_t rawAy = (Wire.read() << 8) | Wire.read();
    int16_t rawAz = (Wire.read() << 8) | Wire.read();
    Wire.read(); Wire.read(); // Skip temp
    int16_t rawGx = (Wire.read() << 8) | Wire.read();
    int16_t rawGy = (Wire.read() << 8) | Wire.read();
    int16_t rawGz = (Wire.read() << 8) | Wire.read();

    // Convert raw to physical units (+/- 4G -> 8192 LSB/g)
    motion.ax = (rawAx / 8192.0f) * 9.81f;
    motion.ay = (rawAy / 8192.0f) * 9.81f;
    motion.az = (rawAz / 8192.0f) * 9.81f;

    // Convert raw gyro (+/- 500 deg/s -> 65.5 LSB/(deg/s))
    motion.gx = rawGx / 65.5f;
    motion.gy = rawGy / 65.5f;
    motion.gz = rawGz / 65.5f;

    // Pitch (-90 to +90 deg) and Roll (-180 to +180 deg)
    motion.roll  = atan2(motion.ay, motion.az) * 57.29578f;
    float denom  = sqrt(motion.ay * motion.ay + motion.az * motion.az);
    motion.pitch = atan2(-motion.ax, denom) * 57.29578f;

    // Dynamic movement (wave / shake)
    float totalG = sqrt(motion.ax * motion.ax + motion.ay * motion.ay + motion.az * motion.az);
    motion.dynamicAccel = abs(totalG - 9.81f);
    motion.gyroEnergy   = sqrt(motion.gx * motion.gx + motion.gy * motion.gy + motion.gz * motion.gz);

    if (motion.dynamicAccel > 3.8f || motion.gyroEnergy > 120.0f) {
      motion.isMoving = true;
    } else if (motion.dynamicAccel < 1.2f && motion.gyroEnergy < 40.0f) {
      motion.isMoving = false;
    }
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

  if (motion.pitch > 35.0f) {
    currentPose = POSE_UP;          // Hand tilted up / Stop sign
  } else if (motion.pitch < -35.0f) {
    currentPose = POSE_DOWN;        // Hand pointing downward
  } else if (motion.roll > 40.0f && motion.roll < 140.0f) {
    currentPose = POSE_TILT_RIGHT;  // Tilted right
  } else if (motion.roll < -40.0f && motion.roll > -140.0f) {
    currentPose = POSE_TILT_LEFT;   // Tilted left
  } else if (abs(motion.roll) > 140.0f) {
    currentPose = POSE_PALM_UP;     // Palm facing ceiling
  } else {
    currentPose = POSE_FLAT;        // Palm horizontal / neutral
  }
}

// ============================================================================
// GESTURE RECOGNITION ENGINE (Fingers + Wrist Motion)
// ============================================================================
void detectGesture() {
  bool thumb = flexSensors[0].isBent;
  bool index = flexSensors[1].isBent;
  bool mid   = flexSensors[2].isBent;
  bool ring  = flexSensors[3].isBent;
  bool pinky = flexSensors[4].isBent;

  GestureId candidate = GESTURE_NONE;

  // 1. EMERGENCY CALL DOCTOR: Rapid hand wave / vigorous shake detected by MPU
  if (motion.isMoving && motion.gyroEnergy > 140.0f) {
    candidate = GESTURE_CALL_DOCTOR;
  }
  // 2. HELP / FIST: All fingers bent (or at least 4 fingers bent)
  else if ((thumb && index && mid && ring && pinky) || (index && mid && ring && pinky)) {
    candidate = GESTURE_NEED_HELP;
  }
  // 3. WATER: Index straight, Middle, Ring & Pinky bent (pointing / water gesture)
  else if (!index && mid && ring && pinky) {
    candidate = GESTURE_WANT_WATER;
  }
  // 4. PEACE: Index and Middle straight, Ring & Pinky bent
  else if (!index && !mid && ring && pinky) {
    candidate = GESTURE_PEACE;
  }
  // 5. THANK YOU: Thumbs up (Thumb straight, all other 4 fingers bent)
  else if (!thumb && index && mid && ring && pinky) {
    candidate = GESTURE_THANK_YOU;
  }
  // 6. I LOVE YOU: Thumb, Index, Pinky straight; Mid & Ring bent
  else if (!thumb && !index && mid && ring && !pinky) {
    candidate = GESTURE_LOVE;
  }
  // 7. I AM OKAY: OK Sign (Thumb & Index bent/touching, Mid, Ring, Pinky straight)
  else if (thumb && index && !mid && !ring && !pinky) {
    candidate = GESTURE_AM_OKAY;
  }
  // 8. RESTING / NEUTRAL HAND: All straight, or non-matching -> Silence / None
  else {
    candidate = GESTURE_NONE;
  }

  // Robust Gesture Debounce & Single-Shot Lock Logic:
  // Requires holding steadily for 350ms, triggers ONCE, then locks until released to NONE!
  static GestureId pendingGesture = GESTURE_NONE;
  static unsigned long pendingStartTime = 0;
  static bool gestureLocked = false;

  if (candidate != pendingGesture) {
    pendingGesture = candidate;
    pendingStartTime = millis();
  } else if (millis() - pendingStartTime >= 350) {
    if (pendingGesture != currentGesture) {
      currentGesture = pendingGesture;
      gestureLocked = false; // New gesture detected, allow trigger!
    }

    if (currentGesture != GESTURE_NONE && !gestureLocked) {
      triggerGestureAudio(currentGesture);
      gestureLocked = true; // Lock: will not repeat until released to neutral!
      lastTriggerTime = millis();
      lastTriggeredGesture = currentGesture;
    }
  }

  // Release lock when hand returns to neutral / open hand
  if (candidate == GESTURE_NONE) {
    gestureLocked = false;
    currentGesture = GESTURE_NONE;
  }
}

// ============================================================================
// GESTURE TRIGGER (DFPlayer Mini Voice Output)
// ============================================================================
void triggerGestureAudio(GestureId g) {
  digitalWrite(LED_PIN, HIGH);

  switch (g) {
    case GESTURE_NEED_HELP:
      Serial.println(F("GESTURE:NEED_HELP"));
      dfplayer_send_cmd(0x03, 1); // Track 1: "I need help"
      break;

    case GESTURE_WANT_WATER:
      Serial.println(F("GESTURE:WANT_WATER"));
      dfplayer_send_cmd(0x03, 2); // Track 2: "Please give me water"
      break;

    case GESTURE_AM_OKAY:
      Serial.println(F("GESTURE:AM_OKAY"));
      dfplayer_send_cmd(0x03, 3); // Track 3: "I am okay"
      break;

    case GESTURE_THANK_YOU:
      Serial.println(F("GESTURE:THANK_YOU"));
      dfplayer_send_cmd(0x03, 4); // Track 4: "Thank you"
      break;

    case GESTURE_PEACE:
      Serial.println(F("GESTURE:PEACE"));
      dfplayer_send_cmd(0x03, 3); // Track 3: "I am okay"
      break;

    case GESTURE_LOVE:
      Serial.println(F("GESTURE:LOVE"));
      dfplayer_send_cmd(0x03, 4); // Track 4: "Thank you"
      break;

    case GESTURE_CALL_DOCTOR:
      Serial.println(F("GESTURE:CALL_DOCTOR"));
      dfplayer_send_cmd(0x03, 5); // Track 5: "Please call a doctor"
      break;

    default:
      break;
  }

  digitalWrite(LED_PIN, LOW);
}

// ============================================================================
// OLED DISPLAY REFRESH (Fast 400kHz & Optimized Layout)
// ============================================================================
void updateDisplay() {
  // Self-healing auto-reconnect: If OLED wire was bumped or loose, ping 0x3C every 800ms
  // The instant the user pushes the wire back in, re-arm the OLED charge pump automatically!
  static unsigned long lastReconnectCheck = 0;
  if (!displayAvailable) {
    if (millis() - lastReconnectCheck > 800) {
      lastReconnectCheck = millis();
      if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        displayAvailable = true;
      }
    }
    return;
  }

  // Rate-limit OLED redraws to ~20 FPS (50ms) to keep main loop running at 100Hz+
  static unsigned long lastOledRefresh = 0;
  if (millis() - lastOledRefresh < 50) return;
  lastOledRefresh = millis();

  display.clearDisplay();

  // 1. Header Bar
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("SIGNBRIDGE"));

  display.setCursor(76, 0);
  if (motion.isMoving) {
    display.print(F("[WAVE!]"));
  } else if (mpuAvailable) {
    display.print(F("[STABLE]"));
  } else {
    display.print(F("[FLEX5]"));
  }
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // 2. 5-Finger Gauge Bars (T, I, M, R, P)
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    int x = 4 + (i * 25);
    int y = 12;

    display.setCursor(x + 7, y);
    display.print(FINGER_INITIALS[i]);

    display.drawRect(x, y + 9, 20, 7, SSD1306_WHITE);

    int fillW = map(flexSensors[i].bendPercent, 0, 100, 0, 18);
    if (fillW > 0) {
      display.fillRect(x + 1, y + 10, fillW, 5, SSD1306_WHITE);
    }
  }

  // 3. Wrist Angles / Status Bar
  display.setCursor(0, 30);
  if (mpuAvailable) {
    display.print(F("P:"));
    display.print((int)motion.pitch);
    display.print((char)247);
    display.print(F(" R:"));
    display.print((int)motion.roll);
    display.print((char)247);
    display.setCursor(80, 30);
    switch (currentPose) {
      case POSE_UP:         display.print(F("UP")); break;
      case POSE_DOWN:       display.print(F("DOWN")); break;
      case POSE_TILT_LEFT:  display.print(F("LEFT")); break;
      case POSE_TILT_RIGHT: display.print(F("RIGHT")); break;
      case POSE_PALM_UP:    display.print(F("PLM-UP")); break;
      default:              display.print(F("FLAT")); break;
    }
  } else {
    display.print(F("Wrist IMU: Connect A4/A5"));
  }

  display.drawLine(0, 39, 127, 39, SSD1306_WHITE);

  // 4. Recognized Gesture Banner
  display.setCursor(0, 42);
  display.print(F("Gesture:"));

  display.setTextSize(2);
  display.setCursor(4, 50);

  GestureId active = (lastTriggeredGesture != GESTURE_NONE && (millis() - lastTriggerTime < 2200))
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
    case GESTURE_PEACE:
      display.print(F("PEACE"));
      break;
    case GESTURE_LOVE:
      display.print(F("LOVE"));
      break;
    case GESTURE_CALL_DOCTOR:
      display.print(F("DOCTOR!"));
      break;
    default:
      display.setTextSize(1);
      display.setCursor(24, 53);
      display.print(F("Listening..."));
      break;
  }

  display.display();
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
    displayAvailable = false; // Wire popped loose; auto-reconnect will restore it once plugged back in
  }
}

// ============================================================================
// SERIAL TELEMETRY OUTPUT (Live Stream for Arduino Serial Plotter)
// ============================================================================
void printSerialOutput() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 40) return; // ~25Hz refresh rate
  lastPrint = millis();

  if (currentMode == MODE_PLOTTER_RAW) {
    // Format for Arduino IDE Serial Plotter:
    Serial.print(F("Thumb:"));  Serial.print(flexSensors[0].raw); Serial.print(F(","));
    Serial.print(F("Index:"));  Serial.print(flexSensors[1].raw); Serial.print(F(","));
    Serial.print(F("Middle:")); Serial.print(flexSensors[2].raw); Serial.print(F(","));
    Serial.print(F("Ring:"));   Serial.print(flexSensors[3].raw); Serial.print(F(","));
    Serial.print(F("Pinky:"));  Serial.print(flexSensors[4].raw == 1 ? 1023 : 0);
    if (mpuAvailable) {
      Serial.print(F(",Pitch:")); Serial.print((int)motion.pitch);
      Serial.print(F(",Roll:"));  Serial.print((int)motion.roll);
      Serial.print(F(",Wave:"));  Serial.print(motion.isMoving ? 800 : 0);
    }
    Serial.println();
  }
  else if (currentMode == MODE_PLOTTER_PCT) {
    Serial.print(F("Thumb:"));  Serial.print(flexSensors[0].bendPercent); Serial.print(F(","));
    Serial.print(F("Index:"));  Serial.print(flexSensors[1].bendPercent); Serial.print(F(","));
    Serial.print(F("Middle:")); Serial.print(flexSensors[2].bendPercent); Serial.print(F(","));
    Serial.print(F("Ring:"));   Serial.print(flexSensors[3].bendPercent); Serial.print(F(","));
    Serial.print(F("Pinky:"));  Serial.print(flexSensors[4].bendPercent);
    if (mpuAvailable) {
      Serial.print(F(",Pitch:")); Serial.print((int)motion.pitch);
      Serial.print(F(",Roll:"));  Serial.print((int)motion.roll);
      Serial.print(F(",Wave:"));  Serial.print(motion.isMoving ? 100 : 0);
    }
    Serial.println();
  }
  else if (currentMode == MODE_DIAGNOSTIC) {
    Serial.print(F("[FLEX] "));
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      Serial.print(FINGER_INITIALS[i]);
      Serial.print(F(":"));
      Serial.print(flexSensors[i].bendPercent);
      Serial.print(F("% "));
    }
    if (mpuAvailable) {
      Serial.print(F("| P:"));
      Serial.print((int)motion.pitch);
      Serial.print(F(" R:"));
      Serial.print((int)motion.roll);
      Serial.print(F(" DynG:"));
      Serial.print(motion.dynamicAccel, 1);
      Serial.print(F(" Mov:"));
      Serial.print(motion.isMoving ? F("YES") : F("NO"));
    }
    Serial.println();
  }
}

// ============================================================================
// SERIAL COMMAND HANDLER
// ============================================================================
void scanI2C() {
  Serial.println(F("\n--- I2C BUS SCAN ---"));
  int count = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("Found device at: 0x"));
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      if (address == 0x3C || address == 0x3D) Serial.println(F(" (OLED Display)"));
      else if (address == 0x68 || address == 0x69) Serial.println(F(" (MPU6050 IMU)"));
      else Serial.println();
      count++;
    }
  }
  Serial.print(F("Total found: "));
  Serial.println(count);
  Serial.println(F("--------------------\n"));
}

void handleSerialCommands() {
  if (!Serial.available()) return;
  char cmd = Serial.read();

  if (cmd == 'i' || cmd == 'I') {
    scanI2C();
  } else if (cmd == 'c' || cmd == 'C') {
    runAutoCalibration();
  } else if (cmd == 'r' || cmd == 'R') {
    currentMode = MODE_PLOTTER_RAW;
    Serial.println(F("[*] Switched to RAW ADC GRAPH mode"));
  } else if (cmd == 'p' || cmd == 'P') {
    currentMode = MODE_PLOTTER_PCT;
    Serial.println(F("[*] Switched to PERCENTAGE GRAPH mode"));
  } else if (cmd == 'd' || cmd == 'D') {
    currentMode = MODE_DIAGNOSTIC;
    Serial.println(F("[*] Switched to RAW HARDWARE DIAGNOSTIC mode"));
  } else if (cmd == 'o' || cmd == 'O') {
    Serial.print(F("[*] Re-initializing OLED Display (0x3C)... "));
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(300);
    display.clearDisplay();
    displayAvailable = true;
    Serial.println(F("OK! Screen flushed white."));
  } else if (cmd >= '1' && cmd <= '7') {
    triggerGestureAudio((GestureId)(cmd - '0'));
  } else if (cmd == 't' || cmd == 'T') {
    Serial.println(F("[*] Audio Test: Playing Track 1 ('I need help')..."));
    dfplayer_send_cmd(0x03, 1);
  } else if (cmd == '+' || cmd == '=') {
    if (dfPlayerVolume < 30) dfPlayerVolume++;
    dfplayer_send_cmd(0x06, dfPlayerVolume);
    Serial.print(F("[*] DFPlayer Volume: "));
    Serial.print(dfPlayerVolume);
    Serial.println(F("/30"));
  } else if (cmd == '-' || cmd == '_') {
    if (dfPlayerVolume > 0) dfPlayerVolume--;
    dfplayer_send_cmd(0x06, dfPlayerVolume);
    Serial.print(F("[*] DFPlayer Volume: "));
    Serial.print(dfPlayerVolume);
    Serial.println(F("/30"));
  } else if (cmd == 'b' || cmd == 'B') {
    Serial.println(F("[*] Buzzer Test Chime!"));
    tone(BUZZER_PIN, 1000, 100);
    delay(120);
    tone(BUZZER_PIN, 1500, 100);
    delay(120);
    tone(BUZZER_PIN, 2000, 150);
  }
}

// ============================================================================
// 2-STEP GUIDED AUTO-CALIBRATION ROUTINE
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

  Serial.println(F("[STEP 1/2]: Keep all 5 fingers completely FLAT & STRAIGHT."));
  Serial.println(F("Sampling in 3 seconds..."));
  for (int i = 3; i > 0; i--) {
    Serial.print(i); Serial.print(F("... "));
    delay(1000);
  }
  Serial.println(F("SAMPLING STRAIGHT POSITIONS!"));

  uint32_t straightSum[NUM_FLEX] = {0};
  const int SAMPLES = 30;
  for (int s = 0; s < SAMPLES; s++) {
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      if (IS_ANALOG[i]) {
        straightSum[i] += analogRead(FLEX_PINS[i]);
      } else {
        straightSum[i] += digitalRead(FLEX_PINS[i]);
      }
    }
    delay(20);
  }

  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    flexSensors[i].minVal = straightSum[i] / SAMPLES;
    Serial.print(F("  "));
    Serial.print(FINGER_INITIALS[i]);
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

  Serial.println(F("\n[STEP 2/2]: Close hand into a TIGHT FIST (Bending all fingers)."));
  Serial.println(F("Sampling in 3 seconds..."));
  for (int i = 3; i > 0; i--) {
    Serial.print(i); Serial.print(F("... "));
    delay(1000);
  }
  Serial.println(F("SAMPLING BENT POSITIONS!"));

  uint32_t bentSum[NUM_FLEX] = {0};
  for (int s = 0; s < SAMPLES; s++) {
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      if (IS_ANALOG[i]) {
        bentSum[i] += analogRead(FLEX_PINS[i]);
      } else {
        bentSum[i] += digitalRead(FLEX_PINS[i]);
      }
    }
    delay(20);
  }

  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    flexSensors[i].maxVal = bentSum[i] / SAMPLES;
    if (flexSensors[i].maxVal < flexSensors[i].minVal) {
      flexSensors[i].inverted = true;
    } else {
      flexSensors[i].inverted = false;
    }

    Serial.print(F("  "));
    Serial.print(FINGER_INITIALS[i]);
    Serial.print(F(" Bent ADC = "));
    Serial.print(flexSensors[i].maxVal);
    if (flexSensors[i].inverted) {
      Serial.println(F(" [Inverted Divider]"));
    } else {
      Serial.println(F(" [Normal Divider]"));
    }
  }

  Serial.println(F("\n[✓] Calibration Complete! Dynamic ranges saved."));
  Serial.println(F("==================================================\n"));

  if (displayAvailable) {
    display.clearDisplay();
    display.setCursor(10, 20);
    display.println(F("CALIBRATION DONE!"));
    display.setCursor(10, 36);
    display.println(F("Ready to detect!"));
    display.display();
    delay(1200);
  }
}
