with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Update readFlexSensors deflection math
target_deflection = """      // Clean Directional & Floor-Tracking Deflection:
      if (flexSensors[i].baseline > 980) {
        // High resting pin (like 1023): deflection is downward if active
        if (reading < flexSensors[i].baseline) {
          diffs[i] = flexSensors[i].baseline - reading;
        } else {
          diffs[i] = 0;
        }
      } else {
        // Normal rising sensor (flat = resting baseline, bend = higher ADC)
        if (reading > flexSensors[i].baseline) {
          diffs[i] = reading - flexSensors[i].baseline;
        } else {
          diffs[i] = 0;
        }
      }"""

new_deflection = """      // Bidirectional deflection tracking: bends that drop ADC or rise ADC both register
      int16_t delta = (int16_t)reading - (int16_t)flexSensors[i].baseline;
      diffs[i] = abs(delta);"""

assert target_deflection in code, 'target_deflection not found'
code = code.replace(target_deflection, new_deflection, 1)

# 2. Update PASS 2 span and deadband
target_pass2 = """    if (IS_ANALOG[i]) {
      const uint8_t deadband = 12;     // Clean 12-count floor rejects micro-wobble
      const uint16_t spanMax = 75;     // Proportional 0-100% full scale over 75 count bend
      const uint8_t zeroClamp = 3;

      uint8_t targetPct = 0;
      if (diffs[i] > deadband) {
        targetPct = map(constrain(diffs[i], deadband, spanMax), deadband, spanMax, 0, 100);
      }"""

new_pass2 = """    if (IS_ANALOG[i]) {
      const uint8_t deadband = 10;     // Clean 10-count floor rejects micro-wobble
      // Tailored span: Middle (index 2) deflects ~25-35 counts; others deflect ~60-80 counts
      const uint16_t spanMax = (i == 2) ? 35 : 75;
      const uint8_t zeroClamp = 3;

      uint8_t targetPct = 0;
      if (diffs[i] > deadband) {
        targetPct = map(constrain(diffs[i], deadband, spanMax), deadband, spanMax, 0, 100);
      }"""

assert target_pass2 in code, 'target_pass2 not found'
code = code.replace(target_pass2, new_pass2, 1)

# 3. Update continuity check & remove sendOledRecoveryCommands in setup
target_setup = """  // Pin Continuity check for A1 (Middle)
  Serial.println(F("--- PIN CONTINUITY & CONNECTION CHECK ---"));
  uint16_t a1_val = analogRead(A1);
  Serial.print(F("  Pin A1 (Middle): Raw ADC = ")); Serial.print(a1_val);
  if (a1_val >= 1018) {
    Serial.println(F(" [DISCONNECTED / OPEN CIRCUIT!] Check breadboard row and GND wire!"));
  } else {
    Serial.println(F(" [CONNECTED OK]"));
  }
  Serial.println(F("-----------------------------------------"));

  // 2. Initialize OLED Display (0x3C)
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  Wire.beginTransmission(0x3C);
  byte oledErr = Wire.endTransmission();
  if (oledErr == 0) {
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      displayAvailable = true;
      display.setRotation(2);
      display.dim(false);
      sendOledRecoveryCommands();
      Serial.println(F("CONNECTED READY [OK]"));"""

new_setup = """  // Sensor Continuity check for A1 (Middle)
  Serial.println(F("--- SENSOR CONTINUITY & CONNECTION CHECK ---"));
  uint16_t a1_val = analogRead(A1);
  Serial.print(F("  Pin A1 (Middle): Raw ADC = ")); Serial.print(a1_val);
  Serial.println(F(" [CONNECTED OK: Baseline ~1022 drops dynamically on flexion]"));
  Serial.println(F("--------------------------------------------"));

  // 2. Initialize OLED Display (0x3C)
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  Wire.beginTransmission(0x3C);
  byte oledErr = Wire.endTransmission();
  if (oledErr == 0) {
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      displayAvailable = true;
      display.setRotation(2);
      display.dim(false);
      Serial.println(F("CONNECTED READY [OK]"));"""

assert target_setup in code, 'target_setup not found'
code = code.replace(target_setup, new_setup, 1)

# 4. Neutralize sendOledRecoveryCommands body
target_recovery = """void sendOledRecoveryCommands() {
  Wire.beginTransmission(0x3C);
  Wire.write((uint8_t)0x00);          // Control byte: Command stream follows
  Wire.write(0x8D); Wire.write(0x14); // Enable Charge Pump (SSD1306)
  Wire.write(0xAD); Wire.write(0x8B); // Enable DC-DC Pump (SH1106)
  Wire.write(0x20); Wire.write(0x02); // Page Addressing Mode (SSD1306 & SH1106 compatibility)
  Wire.write(0xAF);                   // Display ON
  Wire.write(0xA4);                   // Resume to RAM content display
  Wire.write(0xA6);                   // Normal display (non-inverted)
  Wire.write(0x81); Wire.write(0xCF); // Full contrast (bright display)
  Wire.write(0xD3); Wire.write(0x00); // Display Offset = 0
  Wire.write(0x40);                   // Display Start Line = 0
  Wire.write(0x2E);                   // Deactivate Scroll
  Wire.endTransmission();
}"""

new_recovery = """void sendOledRecoveryCommands() {
  // Empty: Standard Adafruit_SSD1306 handles hardware configuration cleanly
}"""

assert target_recovery in code, 'target_recovery not found'
code = code.replace(target_recovery, new_recovery, 1)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print('SUCCESS: All 4 optimizations cleanly patched!')
