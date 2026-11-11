with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Replace refreshOLED implementation with direct display.display()
p1 = code.find('void refreshOLED(uint8_t colOffset) {')
p2 = code.find('// ============================================================================', p1)
if p1 != -1 and p2 != -1:
    new_fn = 'void refreshOLED(uint8_t colOffset) {\n  if (!displayAvailable) return;\n  display.display();\n}\n'
    code = code[:p1] + new_fn + code[p2:]
    print('[OK] refreshOLED replaced with display.display()')

# 2. In setup(), add white screen flash so user visually verifies the OLED hardware works
old_init = 'bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);'
idx = code.find(old_init)
if idx != -1:
    end_idx = code.find('displayAvailable = false;', idx)
    end_idx = code.find('}', end_idx) + 1
    new_block = '''bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) {
    displayAvailable = true;
    display.setRotation(2);
    display.dim(false);
    Serial.println(F("CONNECTED READY [OK]"));

    // Quick white flash on boot so wearer knows display is powered!
    display.clearDisplay();
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(250);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 15);
    display.println(F("SIGNBRIDGE GLOVE"));
    display.setCursor(16, 32);
    display.println(F("OLED ACTIVE"));
    display.display();
  } else {
    displayAvailable = false;
    Serial.println(F("NOT DETECTED (Check 0x3C wiring)"));
  }'''
    code = code[:idx] + new_block + code[end_idx:]
    print('[OK] setup() OLED initialization replaced with flash test.')

# 3. Disable the 800ms sendOledRecoveryCommands watchdog that was corrupting addressing mode
old_watchdog = '''  static unsigned long lastGlitchCheck = 0;
  if (millis() - lastGlitchCheck > 800) {
    lastGlitchCheck = millis();
    displayAvailable = true;
    sendOledRecoveryCommands();
  }'''

new_watchdog = '''  // Watchdog: keep display clean without corrupting I2C addressing mode
  static unsigned long lastGlitchCheck = 0;
  if (millis() - lastGlitchCheck > 1500) {
    lastGlitchCheck = millis();
    displayAvailable = true;
  }'''

if old_watchdog in code:
    code = code.replace(old_watchdog, new_watchdog)
    print('[OK] Disabled addressing mode corruption watchdog.')

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print('[OK] src/main.cpp patched successfully!')
