with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# Change Wire clock to 50kHz for rock-solid stability with dual I2C devices on long wires
code = code.replace('Wire.setClock(100000);', 'Wire.setClock(50000);')

old_setup = '''  // 3. Initialize OLED Display (0x3C) with fast retry
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  Wire.begin();
  Wire.setClock(50000);

  bool oledOk = false;
  for (uint8_t a = 0; a < 3; a++) {
    oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (oledOk) break;
    delay(100);
  }
  if (oledOk) {
    displayAvailable = true;
    display.setRotation(2);
    display.dim(false);
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
    refreshOLED(oledColOffset);
  } else {
    displayAvailable = false;
    Serial.println(F("NOT DETECTED (Check 0x3C wiring)"));
  }

  // Scan I2C bus to report all detected devices
  scanI2C();'''

new_setup = '''  // 1. Scan I2C bus first to see what devices are detected
  scanI2C();

  // 2. Initialize OLED Display (0x3C)
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) {
    displayAvailable = true;
    display.setRotation(2);
    display.dim(false);
    Serial.println(F("CONNECTED READY [OK]"));

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

assert old_setup in code, "old_setup not found"
code = code.replace(old_setup, new_setup)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Setup reordered and I2C 50kHz applied!")
