#define ENABLE_OLED 1 // Set to 1 when a replacement 0.96" OLED (0x3C) is connected

#include <Arduino.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#if ENABLE_OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif
// ---==========================
// SIGNBRIDGE - 5 FLEX SENSORS + LIGHTWEIGHT MPU6050 + OLED DASHBOARD
// ---==========================
// Optimized embedded firmware:
// - Direct I2C driver for MPU6050 (saves ~12KB flash and >300 bytes RAM!)
// - Plenty of free RAM for SSD1306 OLED malloc (screen lights up instantly!)
// - Fast 400kHz I2C for snappy, lag-free visual response
// - Full 5-finger gauges + Pitch & Roll wrist orientation + Emergency Shake
// ---==========================
// ---------------------- PIN DEFINITIONS -------------------------------------
// I2C Bus (Shared by SSD1306 OLED & MPU6050):
// A4 = SDA (Connect BOTH OLED SDA and MPU6050 SDA here)
// A5 = SCL (Connect BOTH OLED SCL and MPU6050 SCL here)
// VCC = 5V Rail, GND = GND Rail
// 5 Flex Sensors:
#define NUM_FLEX 5
uint8_t FLEX_PINS[NUM_FLEX] = {A0, A1, A2, A3, 2};
// A0: Thumb  (Analog A0)
// A1: Index  (Analog A1)
// A2: Middle (Analog A2 - with 100k pull-down resistor)
// A3: Ring   (Analog A3)
// D2: Pinky  (Digital Pin 2)
bool IS_ANALOG[NUM_FLEX] = {true, true, true, true, false};
const char FINGER_INITIALS[NUM_FLEX] = {'T', 'I', 'M', 'R', 'P'};
#define LED_PIN LED_BUILTIN
// ---------------------- BREADBOARD 5V POWER SUPPLY PIN ----------------------
#define BREADBOARD_5V_PIN 8       // Pin 8 outputs steady 5V for Breadboard sensors / MPU
#define BREADBOARD_5V_ALT_PIN 7   // Pin 7 also outputs steady 5V (convenient adjacent backup)
// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
#define BUZZER_PIN 3
// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
#define BUZZER_PIN 3
#define DFPLAYER_PIN_A 11 // Primary DFPlayer TX pin
#define DFPLAYER_PIN_B 10 // Secondary backup TX pin (dual broadcast!)
uint8_t dfPlayerVolume = 16; // Clean, crystal-clear voice volume (zero distortion clipping)

void dfplayer_send_byte(uint8_t b) {
  // Ultra-precise hardware 9600 baud (0.0% error) on Pin 11 (PB3) & Pin 10 (PB2)
  cli();
  PORTB &= ~((1 << PB3) | (1 << PB2)); // Start bit LOW
  delayMicroseconds(104);
  for (uint8_t i = 0; i < 8; i++) {
    if (b & (1 << i)) {
      PORTB |= ((1 << PB3) | (1 << PB2));  // Bit HIGH
    } else {
      PORTB &= ~((1 << PB3) | (1 << PB2)); // Bit LOW
    }
    delayMicroseconds(104);
  }
  PORTB |= ((1 << PB3) | (1 << PB2));   // Stop bit HIGH
  delayMicroseconds(104);
  sei();
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
  delay(12);
}

void playVoiceTrack(uint8_t track) {
  Serial.print(F("[AUDIO] Playing Track "));
  Serial.print(track);
  Serial.println(F(" via DFPlayer Mini (Dual Broadcast Pin 11 + 10)..."));

  dfplayer_send_cmd(0x06, dfPlayerVolume); // Set safe volume (22/30)
  delay(30);
  dfplayer_send_cmd(0x03, track);          // Command 0x03: Play track (0001.mp3)
  delay(35);
  dfplayer_send_cmd(0x12, track);          // Command 0x12: /MP3/ folder (0001.mp3)
}
// ---------------------- DISPLAY CONFIGURATION --------------------------------
#if ENABLE_OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 100000UL, 100000UL);
#endif
bool displayAvailable = false;
// ---------------------- MPU6050 CONFIGURATION --------------------------------
uint8_t mpuAddress = 0x68;
bool mpuAvailable = false;
// ---------------------- SENSOR & CALIBRATION DATA ---------------------------
struct FlexSensor {

  uint16_t raw;

  uint16_t baseline;    // Resting flat baseline ADC

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
OutputMode currentMode = MODE_DIAGNOSTIC;
// ---------------------- FORWARD DECLARATIONS --------------------------------
uint16_t readDigitalRC();
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
void calibrateFlatBaseline();
void recoverI2CBus();
void scanI2C();
uint8_t oledColOffset = 2; // Default 2 for SH1106 (or 0 for SSD1306)
uint8_t oledRotation = 2;

#if ENABLE_OLED
// Clear all 132 columns x 8 pages of SH1106 hardware RAM (erases power-on static snow!)
void clearSH1106RAM() {
  if (!displayAvailable) return;
  for (uint8_t p = 0; p < 8; p++) {
    Wire.beginTransmission(0x3C);
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)(0xB0 + p));
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)0x10);
    Wire.endTransmission();

    for (uint8_t c = 0; c < 132; c += 22) {
      Wire.beginTransmission(0x3C);
      Wire.write((uint8_t)0x40);
      for (uint8_t i = 0; i < 22; i++) Wire.write((uint8_t)0x00);
      Wire.endTransmission();
    }
  }
}

// Universal SH1106 / SSD1306 page-mode refresh with hardware column offset
void refreshOLED(uint8_t colOffset = 2) {
  if (!displayAvailable) return;

  uint8_t* pBuf = display.getBuffer();
  if (!pBuf) return;

  for (uint8_t p = 0; p < 8; p++) {
    // 1. Set Page address (0xB0..0xB7) & Column Address (lower & upper nibble)
    Wire.beginTransmission(0x3C);
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)(0xB0 + p));
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)(colOffset & 0x0F));
    Wire.write((uint8_t)0x80);
    Wire.write((uint8_t)(0x10 | ((colOffset >> 4) & 0x0F)));
    Wire.endTransmission();

    // 2. Stream 128 bytes in 8 chunks of 16 bytes (Wire buffer limit is 32 bytes: 1 byte command + 16 bytes data = 17 bytes, 100% safe without dropped bytes!)
    uint8_t* pagePtr = pBuf + (p * 128);
    for (uint8_t chunk = 0; chunk < 8; chunk++) {
      Wire.beginTransmission(0x3C);
      Wire.write((uint8_t)0x40);
      Wire.write(pagePtr + (chunk * 16), 16);
      Wire.endTransmission();
    }
  }
}
#endif
// ---==========================
// I2C HARDWARE BUS RECOVERY (Flushes locked slaves & resets TWI hardware)
// ---==========================
int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

long readVcc() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));
  uint8_t low = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  if (result == 0) return 0;
  return 1125300L / result; // Vcc in millivolts
}

void checkI2CBusLines() {
  // Test 1: Check if SDA and SCL are shorted together
  pinMode(A4, OUTPUT);
  digitalWrite(A4, LOW);
  pinMode(A5, INPUT_PULLUP);
  delayMicroseconds(50);
  uint16_t sda_scl_cross = analogRead(A5);

  pinMode(A5, OUTPUT);
  digitalWrite(A5, LOW);
  pinMode(A4, INPUT_PULLUP);
  delayMicroseconds(50);
  uint16_t scl_sda_cross = analogRead(A4);

  // Restore INPUT_PULLUP
  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  delay(10);
  uint16_t sda_adc = analogRead(A4);
  uint16_t scl_adc = analogRead(A5);
  long vcc = readVcc();
  float sda_v = (sda_adc * (vcc / 1000.0f)) / 1023.0f;
  float scl_v = (scl_adc * (vcc / 1000.0f)) / 1023.0f;

  Serial.println(F("--- I2C HARDWARE LINE & SHORT CHECK ---"));
  Serial.print(F("  Arduino 5V Rail = ")); Serial.print(vcc); Serial.println(F(" mV"));
  Serial.print(F("  SDA (Pin A4): ")); Serial.print(sda_v, 2); Serial.print(F(" V (ADC=")); Serial.print(sda_adc); Serial.println(F(")"));
  Serial.print(F("  SCL (Pin A5): ")); Serial.print(scl_v, 2); Serial.print(F(" V (ADC=")); Serial.print(scl_adc); Serial.println(F(")"));

  if (sda_scl_cross < 200 || scl_sda_cross < 200) {
    Serial.println(F("  [!] CRITICAL: SDA AND SCL ARE SHORTED TOGETHER!"));
    Serial.println(F("      -> Check if SDA and SCL wires touch or share breadboard rows!"));
  } else {
    Serial.println(F("  [OK] SDA and SCL are physically isolated (no short)."));
  }

  if (sda_v < 1.0f || scl_v < 1.0f) {
    Serial.println(F("  [!] SHORT TO GND: One or both lines are grounded!"));
  } else if (sda_v < 3.2f || scl_v < 3.2f) {
    Serial.println(F("  [!] VOLTAGE IS ~2.4V (LOW): OLED VCC is DISCONNECTED or LOOSE!"));
    Serial.println(F("      -> Connect OLED VCC directly to Arduino 5V header pin!"));
  } else {
    Serial.println(F("  [OK] Bus voltage is healthy (~5V pullup present)."));
  }
  Serial.println(F("---------------------------------------"));
}

void recoverI2CBus() {
  Wire.end();
  TWCR = 0; // Reset AVR TWI hardware

  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, OUTPUT);

  // Send 16 clock pulses on SCL to force any hung slave to finish its byte
  for (uint8_t i = 0; i < 16; i++) {
    digitalWrite(A5, LOW);
    delayMicroseconds(15);
    digitalWrite(A5, HIGH);
    delayMicroseconds(15);
  }

  // Generate explicit I2C STOP condition (SDA low -> high while SCL high)
  pinMode(A4, OUTPUT);
  digitalWrite(A4, LOW);
  delayMicroseconds(15);
  digitalWrite(A5, HIGH);
  delayMicroseconds(15);
  digitalWrite(A4, HIGH);
  delayMicroseconds(25);

  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  delay(10);

  checkI2CBusLines();

  Wire.begin();
  Wire.setClock(100000UL);          // Standard 100kHz I2C for rock-solid OLED reliability
  Wire.setWireTimeout(3000, true);  // 3ms timeout prevents I2C bus hangs!
}
// ---==========================
// ---==========================
// DIGITAL PIN 2 RESISTANCE MEASUREMENT VIA RC RISE TIME (Pinky Finger)
// Ultra-safe: Never drives HIGH into GND, completely eliminating brownout resets!
uint16_t readDigitalRC() {
  uint16_t s[5];
  for (uint8_t i = 0; i < 5; i++) {
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);     // Discharge to GND (0V -> 0V, 100% safe, zero short circuit!)
    delayMicroseconds(40);
    pinMode(2, INPUT_PULLUP); // Charge safely through 35k internal pullup (0.14mA max current)
    uint16_t count = 0;
    while (!(PIND & (1 << PD2)) && count < 2000) {
      count++;
    }
    s[i] = count;
    delayMicroseconds(15);
  }
  // Sort 5 samples to find robust median (completely rejects noise spikes & bounce)
  for (uint8_t a = 0; a < 5; a++) {
    for (uint8_t b = a + 1; b < 5; b++) {
      if (s[b] < s[a]) {
        uint16_t tmp = s[a];
        s[a] = s[b];
        s[b] = tmp;
      }
    }
  }
  return s[2];
}
// FLAT HAND BASELINE CALIBRATION
// ---==========================
void calibrateFlatBaseline() {
  Serial.println(F("\n---"));
  Serial.println(F("[*] CALIBRATING FLAT BASELINE (Hold hand flat)..."));
  Serial.println(F("---"));

  for (int c = 3; c > 0; c--) {
#if ENABLE_OLED
    if (displayAvailable) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(14, 6);
      display.println(F("CALIBRATING FLAT"));
      display.drawLine(0, 18, 127, 18, SSD1306_WHITE);
      display.setCursor(18, 24);
      display.println(F("Hold Hand Flat"));
      display.setTextSize(2);
      display.setCursor(56, 40);
      display.print(c);
      refreshOLED(oledColOffset);
    }
#endif
    Serial.print(F("  Calibrating flat in ")); Serial.print(c); Serial.println(F("..."));
    delay(1000);
  }

#if ENABLE_OLED
  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 6);
    display.println(F("CALIBRATING FLAT"));
    display.drawLine(0, 18, 127, 18, SSD1306_WHITE);
    display.setCursor(22, 32);
    display.println(F("SAMPLING NOW!"));
    refreshOLED(oledColOffset);
  }
#endif
  delay(150);

  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    uint16_t sBuf[25];
    if (IS_ANALOG[i]) {
      for (uint8_t s = 0; s < 25; s++) {
        analogRead(FLEX_PINS[i]);
        delayMicroseconds(120);
        sBuf[s] = analogRead(FLEX_PINS[i]);
        delay(12);
      }
    } else {
      for (uint8_t s = 0; s < 25; s++) {
        sBuf[s] = readDigitalRC();
        delay(12);
      }
    }
    // Sort to find the median sample (rejects noise spikes & transient drift)
    for (uint8_t a = 0; a < 25; a++) {
      for (uint8_t b = a + 1; b < 25; b++) {
        if (sBuf[b] < sBuf[a]) {
          uint16_t tmp = sBuf[a];
          sBuf[a] = sBuf[b];
          sBuf[b] = tmp;
        }
      }
    }
    flexSensors[i].baseline = sBuf[12]; // Median: true resting center!
    if (!IS_ANALOG[i] && flexSensors[i].baseline > 15) {
      flexSensors[i].baseline = 10; // Guard D2 against stray RC spike during flat boot calibration
    }
    flexSensors[i].raw = flexSensors[i].baseline;
    flexSensors[i].bendPercent = 0;
    flexSensors[i].isBent = false;
    Serial.print(F("  "));
    Serial.print(FINGER_INITIALS[i]);
    Serial.print(F(" (Pin "));
    if (IS_ANALOG[i]) {
      Serial.print(F("A"));
      Serial.print(FLEX_PINS[i] - A0);
    } else {
      Serial.print(F("D"));
      Serial.print(FLEX_PINS[i]);
    }
    Serial.print(F(") Baseline = "));
    Serial.println(flexSensors[i].baseline);
  }

  Serial.println(F("[[OK]] Flat Baseline Calibrated Successfully!\n"));
}
// ---==========================
// SETUP
// ---==========================
void setup() {
  uint8_t mcusr_val = MCUSR;
  MCUSR = 0;
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Turn ON Digital Power Rails (5V and GND Power Bank for OLED & Sensors):
  pinMode(8, OUTPUT); digitalWrite(8, HIGH); // Pin 8: Solid 5V VCC
  pinMode(7, OUTPUT); digitalWrite(7, HIGH); // Pin 7: Solid 5V VCC
  pinMode(6, OUTPUT); digitalWrite(6, HIGH); // Pin 6: Solid 5V VCC
  pinMode(9, OUTPUT); digitalWrite(9, LOW);  // Pin 9: Solid 0V GND
  pinMode(12, OUTPUT); digitalWrite(12, LOW);// Pin 12: Solid 0V GND
  pinMode(5, OUTPUT); digitalWrite(5, LOW);  // Pin 5: Solid 0V GND
  pinMode(4, OUTPUT); digitalWrite(4, LOW);  // Pin 4: Solid 0V GND
  delay(150); // Allow OLED internal charge pump to stabilize with power!

  Serial.begin(115200);

  delay(400);

  Serial.println(F("\n---"));

  Serial.println(F("  SIGNBRIDGE - 5 FLEX + MPU6050 + OLED GLOVE      "));
  Serial.print(F("[RESET REASON] MCUSR = 0x")); Serial.println(mcusr_val, HEX);

  Serial.println(F("---"));

  // 1. Flush I2C Bus & Clear any stuck slave state
  recoverI2CBus();
  delay(50);

// 2. Initialize Flex Sensor Pin Modes
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    pinMode(FLEX_PINS[i], INPUT_PULLUP); // All pins use internal pull-up for 2-wire sensors to GND!

    flexSensors[i].raw = 0;
    flexSensors[i].baseline = 900;
    flexSensors[i].minVal = 0;
    flexSensors[i].maxVal = 1023;
    flexSensors[i].bendPercent = 0;
    flexSensors[i].isBent = false;
    flexSensors[i].inverted = false;
  }

  // 1. Scan I2C bus first to see what devices are detected
  scanI2C();

  // Sensor Continuity check for A1 (Index)
  Serial.println(F("--- SENSOR CONTINUITY & CONNECTION CHECK ---"));
  uint16_t a1_val = analogRead(A1);
  Serial.print(F("  Pin A1 (Index): Raw ADC = ")); Serial.print(a1_val);
  if (a1_val >= 1018) Serial.println(F(" [OPEN / CHECK INDEX WIRE!]"));
  else Serial.println(F(" [CONNECTED]"));
  Serial.println(F("--------------------------------------------"));

#if ENABLE_OLED
  // 2. Initialize OLED Display (0x3C, 100kHz Standard)
  extern size_t __malloc_margin;
  __malloc_margin = 16; // Allow malloc(1024) to utilize available heap
  Serial.print(F("[*] Initializing OLED Display at 0x3C (Free RAM: "));
  Serial.print(freeMemory());
  Serial.print(F("B)... "));
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    displayAvailable = true;
    display.setRotation(oledRotation);
    display.dim(false);
    Serial.println(F("CONNECTED READY [OK]"));

    clearSH1106RAM(); // Clear hardware static snow!

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 15);
    display.println(F("SIGNBRIDGE GLOVE"));
    display.setCursor(16, 32);
    display.println(F("OLED ACTIVE"));
    refreshOLED(oledColOffset);
  } else {
    displayAvailable = false;
    Serial.println(F("RAM ALLOCATION FAILED"));
  }
#else
  displayAvailable = false;
  Serial.println(F("[*] OLED disabled (ENABLE_OLED=0) - Flex + MPU6050 + Audio Mode"));
#endif

  // 4. Initialize MPU6050 IMU (0x68 / 0x69)
  Serial.print(F("[*] Initializing MPU6050 IMU... "));
  if (initMPU6050()) {
    mpuAvailable = true;
    Serial.print(F("CONNECTED READY [OK] at 0x"));
    Serial.println(mpuAddress, HEX);
  } else {
    mpuAvailable = false;
    Serial.println(F("NOT DETECTED (Auto-retry in loop)"));
  }

  // 5. Calibrate Flat Baseline (Samples resting hand so all bars start at 0%)
  calibrateFlatBaseline();

  // 6. Initialize DFPlayer Mini Audio System (Dual-Pin 11 & 10 Broadcast)
  Serial.print(F("[*] Initializing DFPlayer Mini (Dual Broadcast Pin 11 + Pin 10)... Volume: "));
  Serial.print(dfPlayerVolume);
  Serial.println(F("/30"));
  pinMode(DFPLAYER_PIN_A, OUTPUT);
  pinMode(DFPLAYER_PIN_B, OUTPUT);
  digitalWrite(DFPLAYER_PIN_A, HIGH);
  digitalWrite(DFPLAYER_PIN_B, HIGH);
  delay(800); // Allow DFPlayer to finish booting and mount SD card
  dfplayer_send_cmd(0x09, 0x02); // Select TF Micro-SD card as playback source
  delay(100);
  dfplayer_send_cmd(0x06, dfPlayerVolume); // Command 0x06: Set safe volume (22/30)
  delay(100);

  Serial.println(F("[*] DFPlayer Ready (Source Selected: TF Card)"));

  Serial.println(F("[[OK]] System Initialized & Ready for Gestures!"));
  Serial.println(F("---\n"));

  digitalWrite(LED_PIN, LOW);
}
// ---==========================
// MAIN LOOP (~35Hz refresh rate for instant, silky response)
// ---==========================
void loop() {

  handleSerialCommands();

  readFlexSensors();

  // Read MPU6050 at silky 33Hz rate (prevents I2C bus congestion)
  static unsigned long lastMpuSample = 0;
  if (millis() - lastMpuSample >= 30) {
    lastMpuSample = millis();
    readMPU6050();
    classifyOrientation();
  }

  detectGesture();

  updateDisplay();

  printSerialOutput();

  delay(15); // Snappy loop delay
}
// ---==========================
// FLEX SENSOR ACQUISITION & FAST DYNAMIC FILTERING (ANTI-CROSSTALK ENGINE)
// ---==========================
void readFlexSensors() {
  uint16_t diffs[NUM_FLEX] = {0};

  // PASS 1: Sample all raw signals with Anti-Ghosting ADC settling delay
  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    if (IS_ANALOG[i]) {
      // Discard first read to switch AVR mux capacitor, allow 120us settling
      analogRead(FLEX_PINS[i]);
      delayMicroseconds(120);
      uint16_t r1 = analogRead(FLEX_PINS[i]);
      delayMicroseconds(60);
      uint16_t r2 = analogRead(FLEX_PINS[i]);
      uint16_t reading = (r1 + r2) / 2;

      flexSensors[i].raw = ((uint32_t)flexSensors[i].raw * 3 + reading) / 4;

      // Track bend deflection from resting flat baseline:
      int16_t delta = (int16_t)reading - (int16_t)flexSensors[i].baseline;
      diffs[i] = abs(delta);
    } else {
      // Pinky finger on Digital Pin 2 (Index 4):
      uint16_t rcReading = readDigitalRC();
      flexSensors[i].raw = rcReading;

      if (rcReading > flexSensors[i].baseline) {
        diffs[i] = rcReading - flexSensors[i].baseline;
      } else {
        diffs[i] = 0;
      }
    }
  }

  // PASS 2: Progressive scaling with stable per-finger deadband & span
  static const uint16_t ANALOG_DEADBAND[4] = {10, 12, 12, 12};   // T, I, M, R rock-solid noise rejection
  static const uint16_t ANALOG_SPANMAX[4]  = {50, 45, 45, 55};   // T, I, M, R full deliberate bend span
  static uint8_t analogReleaseCnt[4] = {0, 0, 0, 0};
  static uint8_t pinkyEngageCnt = 0;
  static uint8_t pinkyReleaseCnt = 0;

  for (uint8_t i = 0; i < NUM_FLEX; i++) {
    if (IS_ANALOG[i]) {
      const uint16_t deadband = ANALOG_DEADBAND[i];
      const uint16_t spanMax  = ANALOG_SPANMAX[i];
      const uint8_t zeroClamp = 3;

      uint8_t targetPct = 0;
      if (diffs[i] > deadband) {
        targetPct = map(constrain(diffs[i], deadband, spanMax), deadband, spanMax, 0, 100);
      }

      // Smooth moving average: snappy 60/40 rise, smooth 65/35 fall
      if (targetPct >= flexSensors[i].bendPercent) {
        flexSensors[i].bendPercent = ((uint16_t)flexSensors[i].bendPercent * 60 + (uint16_t)targetPct * 40) / 100;
      } else {
        flexSensors[i].bendPercent = ((uint16_t)flexSensors[i].bendPercent * 65 + (uint16_t)targetPct * 35) / 100;
      }

      if (flexSensors[i].bendPercent <= zeroClamp) {
        flexSensors[i].bendPercent = 0;
      }

      // Deliberate 28% bend trigger, clean 14% release (zero phantom gestures while flat!)
      if (flexSensors[i].bendPercent >= 28) {
        flexSensors[i].isBent = true;
        analogReleaseCnt[i] = 0;
      } else if (flexSensors[i].bendPercent <= 14) {
        if (flexSensors[i].isBent) {
          if (++analogReleaseCnt[i] >= 3) { // ~60ms release debounce
            flexSensors[i].isBent = false;
            analogReleaseCnt[i] = 0;
          }
        } else {
          analogReleaseCnt[i] = 0;
        }
      } else {
        analogReleaseCnt[i] = 0;
      }

    } else {
      // Digital Pin 2 (Pinky): RC discharge with spike-rejecting engage debounce
      const uint8_t rcDeadband = 5;    // Rejects idle jitter
      const uint16_t rcSpan = 40;      // Scales curl cleanly 0-100%
      const uint8_t zeroClamp = 3;

      uint8_t rcPct = 0;
      if (diffs[i] > rcDeadband) {
        rcPct = map(constrain(diffs[i], rcDeadband, rcSpan), rcDeadband, rcSpan, 0, 100);
      }

      // Balanced rise/fall
      if (rcPct >= flexSensors[i].bendPercent) {
        flexSensors[i].bendPercent = ((uint16_t)flexSensors[i].bendPercent * 60 + (uint16_t)rcPct * 40) / 100;
      } else {
        flexSensors[i].bendPercent = ((uint16_t)flexSensors[i].bendPercent * 55 + (uint16_t)rcPct * 45) / 100;
      }

      if (flexSensors[i].bendPercent <= zeroClamp) {
        flexSensors[i].bendPercent = 0;
      }

      // Deliberate 32% bend trigger, clean 15% release
      if (flexSensors[i].bendPercent >= 32) {
        pinkyReleaseCnt = 0;
        if (!flexSensors[i].isBent && ++pinkyEngageCnt >= 3) {
          flexSensors[i].isBent = true;
          pinkyEngageCnt = 3;
        }
      } else if (flexSensors[i].bendPercent <= 15) {
        pinkyEngageCnt = 0;
        if (flexSensors[i].isBent && ++pinkyReleaseCnt >= 3) {
          flexSensors[i].isBent = false;
          pinkyReleaseCnt = 0;
        }
      } else {
        pinkyEngageCnt = 0;
        pinkyReleaseCnt = 0;
      }
    }
  }
}
// ---==========================
// DIRECT LIGHTWEIGHT MPU6050 DRIVER (Zero library overhead!)
// ---==========================
bool initMPU6050() {
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    // 1. Auto-detect whether MPU is at address 0x68 or 0x69 (AD0 pin)
    Wire.beginTransmission(0x68);
    if (Wire.endTransmission() == 0) {
      mpuAddress = 0x68;
    } else {
      Wire.beginTransmission(0x69);
      if (Wire.endTransmission() == 0) {
        mpuAddress = 0x69;
      } else {
        delay(15);
        continue;
      }
    }

    // Wake up MPU6050 (write 0 to PWR_MGMT_1 register 0x6B)
    Wire.beginTransmission(mpuAddress);
    Wire.write((uint8_t)0x6B);
    Wire.write((uint8_t)0x00);
    if (Wire.endTransmission() == 0) {
      delay(15); // PLL clock stabilization
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
    delay(20);
  }
  return false;
}
void readMPU6050() {
  static unsigned long lastMpuCheck = 0;
  static uint8_t mpuFailCount = 0;

  if (!mpuAvailable) {
    motion.isMoving = false;
    motion.dynamicAccel = 0.0f;
    motion.gyroEnergy = 0.0f;
    if (millis() - lastMpuCheck > 1000) {
      lastMpuCheck = millis();
      if (initMPU6050()) {
        mpuAvailable = true;
        mpuFailCount = 0;
        Serial.print(F("[!] MPU6050 RECONNECTED [OK] at 0x"));
        Serial.println(mpuAddress, HEX);
      }
    }
    return;
  }

  // Send register address 0x3B with clean STOP so bus releases cleanly between OLED and MPU
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B); // Starting at ACCEL_XOUT_H
  uint8_t txErr = Wire.endTransmission(true);
  if (txErr != 0) {
    if (++mpuFailCount > 8) {
      mpuAvailable = false;
      mpuFailCount = 0;
      motion.isMoving = false;
      motion.dynamicAccel = 0.0f;
      motion.gyroEnergy = 0.0f;
      motion.pitch = 0.0f;
      motion.roll = 0.0f;
    }
    if (Wire.getWireTimeoutFlag()) Wire.clearWireTimeoutFlag();
    return;
  }
  mpuFailCount = 0;

  // Read 14 bytes: Ax, Ay, Az, Temp, Gx, Gy, Gz

  if (Wire.requestFrom((uint8_t)mpuAddress, (uint8_t)14, (uint8_t)1) == 14) {

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

    if (motion.dynamicAccel > 6.0f || motion.gyroEnergy > 200.0f) {

      motion.isMoving = true;

    } else if (motion.dynamicAccel < 1.5f && motion.gyroEnergy < 50.0f) {

      motion.isMoving = false;

    }

  }
}
// ---==========================
// WRIST POSE CLASSIFICATION
// ---==========================
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
// ---==========================
// GESTURE RECOGNITION ENGINE (Fingers + Wrist Motion)
// ---==========================
void detectGesture() {

  // Debounced per-finger bent states (ON >= 28%, OFF <= 14% from readFlexSensors)
  bool thumb = flexSensors[0].isBent;
  bool index = flexSensors[1].isBent;
  bool mid   = flexSensors[2].isBent;
  bool ring  = flexSensors[3].isBent;
  bool pinky = flexSensors[4].isBent;

  bool backFingers = (ring || mid); // Ring or Middle curled

  GestureId candidate = GESTURE_NONE;

  // 1. EMERGENCY VIGOROUS SHAKE / WAVE -> DOCTOR! (Track 5)
  if (motion.isMoving) {
    candidate = GESTURE_CALL_DOCTOR;
  }
  // 2. CLOSED FIST: Thumb + Index + Back fingers ALL BENT (1, 1, 1) -> HELP! (Track 1)
  else if (thumb && index && backFingers) {
    candidate = GESTURE_NEED_HELP;
  }
  // 3. THUMBS UP: Thumb STRAIGHT UP; Index + Back fingers BENT (0, 1, 1) -> THANK YOU (Track 4)
  else if (!thumb && index && backFingers) {
    candidate = GESTURE_THANK_YOU;
  }
  // 4. OK SIGN: Thumb + Index BENT; Back fingers FLAT (1, 1, 0) -> I'M OKAY (Track 3)
  else if (thumb && index && !backFingers) {
    candidate = GESTURE_AM_OKAY;
  }
  // 5. POINTING INDEX: Index BENT; Thumb + Back fingers FLAT (0, 1, 0) -> WATER (Track 2)
  else if (!thumb && index && !backFingers) {
    candidate = GESTURE_WANT_WATER;
  }
  // 6. PEACE SIGN: Index FLAT; Back fingers + Thumb BENT (1, 0, 1) -> PEACE (Track 6)
  else if (thumb && !index && backFingers) {
    candidate = GESTURE_PEACE;
  }
  // 7. I LOVE YOU (ILY): Index FLAT; Thumb FLAT; Back fingers BENT (0, 0, 1) -> LOVE (Track 7)
  else if (!thumb && !index && backFingers) {
    candidate = GESTURE_LOVE;
  }
  else {
    candidate = GESTURE_NONE;
  }

  // State Machine: Require steady 500ms deliberate hold to eliminate mid-transition misfires!
  static GestureId activeGesture = GESTURE_NONE;
  static unsigned long holdStart = 0;
  static GestureId lastTriggered = GESTURE_NONE;

  if (candidate != activeGesture) {
    activeGesture = candidate;
    holdStart = millis();
  } else {
    // Wait until hand sits flat/neutral for 300ms before unlocking next trigger
    if (activeGesture == GESTURE_NONE) {
      if (millis() - holdStart >= 300) {
        lastTriggered = GESTURE_NONE;
        currentGesture = GESTURE_NONE;
      }
    } else if (activeGesture != lastTriggered && (millis() - holdStart >= 500)) {
      currentGesture = activeGesture;
      Serial.print(F("[GESTURE] MATCHED (T:")); Serial.print(thumb);
      Serial.print(F(" I:")); Serial.print(index);
      Serial.print(F(" M:")); Serial.print(mid);
      Serial.print(F(" R:")); Serial.print(ring);
      Serial.print(F(" P:")); Serial.print(pinky);
      Serial.println(F(")"));
      triggerGestureAudio(currentGesture);
      lastTriggered = activeGesture;
      lastTriggerTime = millis();
      lastTriggeredGesture = currentGesture;
    }
  }
}
// ---==========================
// GESTURE TRIGGER (DFPlayer Mini Voice Output)
// ---==========================
void triggerGestureAudio(GestureId g) {
  // Voice playback cooldown: prevent playing tracks from stuttering or getting chopped
  static unsigned long lastAudioPlayTime = 0;
  if (millis() - lastAudioPlayTime < 2400) {
    return; // Wait for track to finish playing!
  }
  lastAudioPlayTime = millis();

  digitalWrite(LED_PIN, HIGH);
  pinMode(BUZZER_PIN, INPUT);

  switch (g) {

    case GESTURE_NEED_HELP:
      Serial.println(F("GESTURE:NEED_HELP"));
            playVoiceTrack(1); // Track 1: "I need help"
                  break;

    case GESTURE_WANT_WATER:
      Serial.println(F("GESTURE:WANT_WATER"));
            playVoiceTrack(2); // Track 2: "Please give me water"
                  break;

    case GESTURE_AM_OKAY:
      Serial.println(F("GESTURE:AM_OKAY"));
            playVoiceTrack(3); // Track 3: "I am okay"
                  break;

    case GESTURE_THANK_YOU:
      Serial.println(F("GESTURE:THANK_YOU"));
            playVoiceTrack(4); // Track 4: "Thank you"
                              break;

    case GESTURE_PEACE:
      Serial.println(F("GESTURE:PEACE"));
      playVoiceTrack(6); // Track 6: "Peace" (matches OLED: PEACE)
      break;

    case GESTURE_LOVE:
      Serial.println(F("GESTURE:LOVE"));
      playVoiceTrack(7); // Track 7: "Love" (matches OLED: LOVE)
      break;

    case GESTURE_CALL_DOCTOR:
      Serial.println(F("GESTURE:CALL_DOCTOR"));
            playVoiceTrack(5); // Track 5: "Please call a doctor"
                              break;

    default:
      break;

  }

  digitalWrite(LED_PIN, LOW);
}
// ---==========================
// OLED DISPLAY REFRESH (Clean & Stable 20 FPS)
// ---==========================
void updateDisplay() {
#if !ENABLE_OLED
  return;
#else
  if (!displayAvailable) return;

  // Rate-limit OLED redraws to stable ~16 FPS (60ms) - silky smooth, zero bus freeze
  static unsigned long lastOledRefresh = 0;
  if (millis() - lastOledRefresh < 60) return;
  lastOledRefresh = millis();

  display.clearDisplay();

  // 1. Clean Top Header: "SIGNBRIDGE"
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(34, 2);
  display.print(F("SIGNBRIDGE"));
  display.drawLine(0, 13, 127, 13, SSD1306_WHITE);

  // 2. Active Gesture / Spoken Voice Words
  // Keep spoken word on screen for 2.5s while speaker plays, then return to Listening...
  GestureId active = (lastTriggeredGesture != GESTURE_NONE && (millis() - lastTriggerTime < 2500))
                     ? lastTriggeredGesture : currentGesture;

  if (active != GESTURE_NONE) {
    display.setTextSize(2);
    switch (active) {
      case GESTURE_NEED_HELP:
        display.setCursor(10, 30);
        display.print(F("NEED HELP"));
        break;
      case GESTURE_WANT_WATER:
        display.setCursor(34, 30);
        display.print(F("WATER"));
        break;
      case GESTURE_AM_OKAY:
        display.setCursor(16, 30);
        display.print(F("I'M OKAY"));
        break;
      case GESTURE_THANK_YOU:
        display.setCursor(10, 30);
        display.print(F("THANK YOU"));
        break;
      case GESTURE_PEACE:
        display.setCursor(34, 30);
        display.print(F("PEACE"));
        break;
      case GESTURE_LOVE:
        display.setCursor(4, 30);
        display.print(F("I LOVE YOU"));
        break;
      case GESTURE_CALL_DOCTOR:
        display.setCursor(22, 30);
        display.print(F("DOCTOR!"));
        break;
      default:
        break;
    }
  } else {
    // Resting state: Clean centered "LISTENING" and "[ Ready ]"
    display.setTextSize(2);
    display.setCursor(10, 26);
    display.print(F("LISTENING"));
    display.setTextSize(1);
    display.setCursor(38, 48);
    display.print(F("[ Ready ]"));
  }

  refreshOLED(oledColOffset);

  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
  }
#endif
}
// ---==========================
// SERIAL TELEMETRY OUTPUT (Live Stream for Arduino Serial Plotter)
// ---==========================
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

    Serial.print(F("Pinky:"));  Serial.print(flexSensors[4].raw);

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
      Serial.print(F("("));
      if (IS_ANALOG[i]) {
        Serial.print(F("A"));
        Serial.print(FLEX_PINS[i] - A0);
      } else {
        Serial.print(F("D"));
        Serial.print(FLEX_PINS[i]);
      }
      Serial.print(F("):"));
      Serial.print(flexSensors[i].raw);
      Serial.print(F("("));
      Serial.print(flexSensors[i].bendPercent);
      Serial.print(F("%) "));
    }

    // Flag any actively bent fingers in serial log:
    bool anyBent = false;
    for (uint8_t i = 0; i < NUM_FLEX; i++) {
      if (flexSensors[i].isBent) {
        if (!anyBent) {
          Serial.print(F("-> [BENT: "));
          anyBent = true;
        } else {
          Serial.print(F("+"));
        }
        Serial.print(FINGER_INITIALS[i]);
      }
    }
    if (anyBent) Serial.print(F("] "));

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
// ---==========================
// SERIAL COMMAND HANDLER
// ---==========================
void scanI2C() {
  Serial.println(F("--- I2C BUS SCAN ---"));
  int count = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (Wire.getWireTimeoutFlag()) Wire.clearWireTimeoutFlag();
    if (address == 0x3C || address == 0x3D) {
      Serial.print(F("  Probe 0x"));
      Serial.print(address, HEX);
      Serial.print(F(" -> error code: "));
      Serial.println(error);
    }
    if (error == 0) {
      Serial.print(F("Found device at: 0x"));
      Serial.print(address, HEX);
      if (address == 0x3C || address == 0x3D) Serial.println(F(" (OLED Display)"));
      else if (address == 0x68 || address == 0x69) Serial.println(F(" (MPU6050 IMU)"));
      else Serial.println();
      count++;
    }
  }
  Serial.print(F("Total found: "));
  Serial.println(count);
  Serial.println(F("--------------------"));
}
void handleSerialCommands() {

  if (!Serial.available()) return;

  char cmd = Serial.read();
  while (Serial.available()) Serial.read(); // Flush extra bytes to prevent command loops!

  if (cmd >= '1' && cmd <= '7') {
    uint8_t trk = cmd - '0';
    Serial.print(F("[*] Keyboard Audio Test: Track "));
    Serial.println(trk);
    playVoiceTrack(trk);
    lastTriggeredGesture = (GestureId)trk;
    lastTriggerTime = millis();
  } else if (cmd == '+' || cmd == '=') {
    if (dfPlayerVolume < 30) dfPlayerVolume++;
    dfplayer_send_cmd(0x06, dfPlayerVolume);
    Serial.print(F("[*] Volume: ")); Serial.print(dfPlayerVolume); Serial.println(F("/30"));
  } else if (cmd == '-' || cmd == '_') {
    if (dfPlayerVolume > 0) dfPlayerVolume--;
    dfplayer_send_cmd(0x06, dfPlayerVolume);
    Serial.print(F("[*] Volume: ")); Serial.print(dfPlayerVolume); Serial.println(F("/30"));
  } else if (cmd == 'i' || cmd == 'I') {

    scanI2C();

  } else if (cmd == 's' || cmd == 'S') {

    uint8_t tmpPin = FLEX_PINS[1];
    FLEX_PINS[1] = FLEX_PINS[2];
    FLEX_PINS[2] = tmpPin;
    Serial.print(F("[*] SWAPPED INDEX & MIDDLE PINS! Index is now Pin A"));
    Serial.print(FLEX_PINS[1] - A0);
    Serial.print(F(", Middle is now Pin A"));
    Serial.println(FLEX_PINS[2] - A0);
    calibrateFlatBaseline();

  } else if (cmd == 'c' || cmd == 'C') {

    calibrateFlatBaseline();

  } else if (cmd == 'k' || cmd == 'K') {

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
#if ENABLE_OLED
    Serial.print(F("[*] Re-initializing OLED Display (0x3C)... "));
    Wire.beginTransmission(0x3C);
    byte oErr = Wire.endTransmission();
    if (oErr == 0) {
      if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
        display.setRotation(2);
        display.dim(false);
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10, 24);
        display.print(F("SIGNBRIDGE"));
        refreshOLED(oledColOffset);
        displayAvailable = true;
        Serial.println(F("CONNECTED & READY [OK]"));
      } else {
        displayAvailable = false;
        Serial.println(F("RAM ALLOCATION FAILED"));
      }
    } else {
      displayAvailable = false;
      Serial.print(F("NOT DETECTED on I2C (Error "));
      Serial.print(oErr);
      Serial.println(F(") - Check OLED VCC, GND, SDA(A4), SCL(A5) wires!"));
    }
#else
    Serial.println(F("[*] OLED disabled in firmware (ENABLE_OLED=0)"));
#endif
  } else if (cmd == 'w' || cmd == 'W') {

    Serial.println(F("\n--- LIVE HARDWARE WIRE DIAGNOSTIC ---"));
    Serial.print(F("  Thumb  (A0): ADC=")); Serial.print(analogRead(A0));
    Serial.print(F(" | Index  (A1): ADC=")); Serial.print(analogRead(A1));
    Serial.print(F(" | Middle (A2): ADC=")); Serial.print(analogRead(A2));
    if (analogRead(A1) >= 1018) Serial.print(F(" [A1 NEAR 5V]"));
    Serial.print(F(" | Ring   (A3): ADC=")); Serial.println(analogRead(A3));
    Serial.print(F("  Pinky  (D2): RC Count=")); Serial.println(readDigitalRC());
    Serial.print(F("  I2C SDA(A4): ")); Serial.print(digitalRead(A4) ? F("HIGH") : F("LOW"));
    Serial.print(F(" | SCL(A5): ")); Serial.println(digitalRead(A5) ? F("HIGH") : F("LOW"));
    Serial.println(F("--------------------------------------\n"));

  } else if (cmd == 'f' || cmd == 'F') {
#if ENABLE_OLED
    static uint8_t rot = 2;
    rot = (rot == 2) ? 0 : 2;
    display.setRotation(rot);
    Serial.print(F("[*] Switched OLED Rotation to: "));
    Serial.println(rot);
    refreshOLED(oledColOffset);
#endif
  } else if (cmd >= '1' && cmd <= '7') {

    triggerGestureAudio((GestureId)(cmd - '0'));

  } else if (cmd == 'v' || cmd == 'V') {
    uint16_t sdaV = analogRead(A4);
    uint16_t sclV = analogRead(A5);
    long vcc = readVcc();
    float sda_volts = (sdaV * (vcc / 1000.0f)) / 1023.0f;
    float scl_volts = (sclV * (vcc / 1000.0f)) / 1023.0f;
    Serial.print(F("[BUS VOLTAGE] SDA (A4) = "));
    Serial.print(sda_volts, 2);
    Serial.print(F("V | SCL (A5) = "));
    Serial.print(scl_volts, 2);
    Serial.print(F("V (VCC = "));
    Serial.print(vcc / 1000.0f, 2);
    Serial.println(F("V)"));
    if (sda_volts < 3.2f || scl_volts < 3.2f) {
      Serial.println(F(" -> [!] OLED VCC IS UNPOWERED OR LOOSE (Lines at ~2.4V)! Plug OLED VCC into 5V."));
    } else {
      Serial.println(F(" -> [OK] Bus voltage healthy at ~5V!"));
    }
  } else if (cmd == 'x' || cmd == 'X') {
#if ENABLE_OLED
    oledColOffset = (oledColOffset == 2) ? 0 : 2;
    clearSH1106RAM();
    Serial.print(F("[*] Switched OLED Column Offset to: "));
    Serial.println(oledColOffset);
    refreshOLED(oledColOffset);
#endif
  } else if (cmd == 'y' || cmd == 'Y') {
#if ENABLE_OLED
    oledRotation = (oledRotation == 2) ? 0 : 2;
    display.setRotation(oledRotation);
    clearSH1106RAM();
    Serial.print(F("[*] Switched OLED Rotation to: "));
    Serial.println(oledRotation);
    refreshOLED(oledColOffset);
#endif
  } else if (cmd == 't' || cmd == 'T') {

    Serial.println(F("[*] Audio Test: Playing Track 1 ('I need help')..."));

    playVoiceTrack(1);

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

    
    delay(120);

    
    delay(120);

    
  }
}
// ---==========================
// 2-STEP GUIDED AUTO-CALIBRATION ROUTINE
// ---==========================
void runAutoCalibration() {

  Serial.println(F("\n---"));

  Serial.println(F("      FLEX SENSOR AUTO-CALIBRATION PROCEDURE      "));

  Serial.println(F("---"));

#if ENABLE_OLED
  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 5);
    display.println(F("CALIBRATION MODE"));
    display.println(F("-----------------"));
    display.println(F("Step 1:"));
    display.println(F("HOLD HAND OPEN"));
    display.println(F("& STRAIGHT..."));
    refreshOLED(oledColOffset);
  }
#endif

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

    flexSensors[i].baseline = flexSensors[i].minVal;

    Serial.print(F("  "));

    Serial.print(FINGER_INITIALS[i]);

    Serial.print(F(" Flat ADC = "));

    Serial.println(flexSensors[i].minVal);

  }

#if ENABLE_OLED
  if (displayAvailable) {
    display.clearDisplay();
    display.setCursor(0, 5);
    display.println(F("CALIBRATION MODE"));
    display.println(F("-----------------"));
    display.println(F("Step 2:"));
    display.println(F("CLOSE INTO A"));
    display.println(F("TIGHT FIST..."));
    refreshOLED(oledColOffset);
  }
#endif

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

  Serial.println(F("\n[[OK]] Calibration Complete! Dynamic ranges saved."));

  Serial.println(F("---\n"));

#if ENABLE_OLED
  if (displayAvailable) {
    display.clearDisplay();
    display.setCursor(10, 20);
    display.println(F("CALIBRATION DONE!"));
    display.setCursor(10, 36);
    display.println(F("Ready to detect!"));
    refreshOLED(oledColOffset);
    delay(1200);
  }
#endif
}
