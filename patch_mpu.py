with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target1 = """  // 4. MPU6050 & Audio bypassed for dedicated Flex Sensors + OLED mode
  mpuAvailable = false;"""

replacement1 = """  // 4. Initialize MPU6050 IMU (0x68 / 0x69)
  Serial.print(F("[*] Initializing MPU6050 IMU... "));
  if (initMPU6050()) {
    mpuAvailable = true;
    Serial.print(F("CONNECTED READY [OK] at 0x"));
    Serial.println(mpuAddress, HEX);
  } else {
    mpuAvailable = false;
    Serial.println(F("NOT DETECTED (Auto-retry in loop)"));
  }"""

target2 = """  handleSerialCommands();

  readFlexSensors();

  detectGesture();"""

replacement2 = """  handleSerialCommands();

  readFlexSensors();

  readMPU6050();

  classifyOrientation();

  detectGesture();"""

target3 = """  // 3. Live Index Sensor Readout (Focus on Index Finger!)
  display.setCursor(0, 30);
  display.print(F("INDEX: "));
  display.print(flexSensors[1].bendPercent);
  display.print(F("% (ADC:"));
  display.print(flexSensors[1].raw);
  display.print(F(")"));

  display.drawLine(0, 39, 127, 39, SSD1306_WHITE);"""

replacement3 = """  // 3. Wrist Orientation (Pitch, Roll & Dynamic Motion)
  display.setCursor(0, 30);
  if (mpuAvailable) {
    display.print(F("P:"));
    if ((int)motion.pitch >= 0) display.print('+');
    display.print((int)motion.pitch);
    display.print((char)247);
    
    display.setCursor(64, 30);
    display.print(F("R:"));
    if ((int)motion.roll >= 0) display.print('+');
    display.print((int)motion.roll);
    display.print((char)247);
  } else {
    display.print(F("MPU: Initializing..."));
  }

  display.drawLine(0, 39, 127, 39, SSD1306_WHITE);"""

target4 = """    Serial.print(F("| IDX_DIFF:"));
    int16_t idxDiff = (int16_t)flexSensors[1].raw - (int16_t)flexSensors[1].baseline;
    Serial.print(idxDiff);"""

replacement4 = """    // (Index verified working)"""

assert target1 in text, "target1 not found"
assert target2 in text, "target2 not found"
assert target3 in text, "target3 not found"
assert target4 in text, "target4 not found"

text = text.replace(target1, replacement1)
text = text.replace(target2, replacement2)
text = text.replace(target3, replacement3)
text = text.replace(target4, replacement4)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Successfully updated src/main.cpp!")
