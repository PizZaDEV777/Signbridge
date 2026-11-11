with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Update refreshOLED to the universal page-by-page driver (supports both SSD1306 & SH1106)
old_refresh = '''void refreshOLED(uint8_t colOffset) {
  if (!displayAvailable) return;
  display.display();
}'''

new_refresh = '''void refreshOLED(uint8_t colOffset) {
  if (!displayAvailable) return;
  uint8_t *buf = display.getBuffer();
  if (!buf) {
    display.display();
    return;
  }

  for (uint8_t page = 0; page < 8; page++) {
    // 1. Set Page Address (0xB0..0xB7) and Column Address for SH1106 / SSD1306
    Wire.beginTransmission(0x3C);
    Wire.write((uint8_t)0x00); // Command byte
    Wire.write((uint8_t)(0xB0 | page));
    Wire.write((uint8_t)(0x00 | (colOffset & 0x0F)));        // Lower column
    Wire.write((uint8_t)(0x10 | ((colOffset >> 4) & 0x0F))); // Upper column
    if (Wire.endTransmission() != 0) return;

    // 2. Stream 128 bytes in 16-byte chunks (Wire buffer = 32 bytes)
    for (uint8_t chunk = 0; chunk < 128; chunk += 16) {
      Wire.beginTransmission(0x3C);
      Wire.write((uint8_t)0x40); // Data byte
      for (uint8_t b = 0; b < 16; b++) {
        Wire.write(buf[page * 128 + chunk + b]);
      }
      Wire.endTransmission();
    }
  }
}'''

assert old_refresh in code, "old_refresh not found"
code = code.replace(old_refresh, new_refresh)

# 2. In setup(), call sendOledRecoveryCommands() immediately and use refreshOLED()
old_init = '''  if (oledOk) {
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
    display.display();'''

new_init = '''  if (oledOk) {
    displayAvailable = true;
    display.setRotation(2);
    display.dim(false);
    sendOledRecoveryCommands(); // Force DC-DC charge pump ON for SH1106 & SSD1306
    Serial.println(F("CONNECTED READY [OK]"));

    // Quick white flash on boot so wearer knows display is powered!
    display.clearDisplay();
    display.fillScreen(SSD1306_WHITE);
    refreshOLED(oledColOffset);
    delay(200);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(14, 15);
    display.println(F("SIGNBRIDGE GLOVE"));
    display.setCursor(16, 32);
    display.println(F("OLED ACTIVE"));
    refreshOLED(oledColOffset);'''

assert old_init in code, "old_init not found"
code = code.replace(old_init, new_init)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Universal SSD1306/SH1106 driver patched successfully!")
