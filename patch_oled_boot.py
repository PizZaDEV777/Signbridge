with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = """  // 3. Initialize OLED Display (0x3C)
  delay(50);
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  Wire.begin();
  Wire.setClock(100000);

  bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);"""

replacement = """  // 3. Initialize OLED Display (0x3C) with fast retry
  Serial.print(F("[*] Initializing OLED Display (0x3C)... "));
  Wire.begin();
  Wire.setClock(100000);

  bool oledOk = false;
  for (uint8_t a = 0; a < 3; a++) {
    oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (oledOk) break;
    delay(100);
  }"""

assert target in text, "target not found"
text = text.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("OLED boot retry patched successfully!")
