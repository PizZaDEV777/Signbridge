with open('src/main.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = 'refreshOLED(oledColOffset);'
replacement = '''refreshOLED(oledColOffset);
  static unsigned long lastDbg = 0;
  if (millis() - lastDbg > 2000) {
    lastDbg = millis();
    Serial.println(F("[OLED] Frame drawn to 0x3C [OK]"));
  }'''

text = text.replace(target, replacement, 1)
with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
print('Added debug print!')
