with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Remove call to sendOledRecoveryCommands() in setup
code = code.replace('sendOledRecoveryCommands(); // Force DC-DC charge pump ON for SH1106 & SSD1306\n', '')
code = code.replace('sendOledRecoveryCommands();\n', '')

# 2. Use display.display() directly in refreshOLED (this is what worked when user verified OLED was working)
old_refresh = '''void refreshOLED(uint8_t colOffset) {
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

new_refresh = '''void refreshOLED(uint8_t colOffset) {
  if (!displayAvailable) return;
  display.display();
}'''

assert old_refresh in code, "old_refresh not found"
code = code.replace(old_refresh, new_refresh)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Corruption commands removed, clean display.display() restored!")
