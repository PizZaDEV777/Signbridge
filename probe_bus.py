with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

target = '''void scanI2C() {
  Serial.println(F("--- I2C BUS SCAN ---"));
  int count = 0;'''

replacement = '''void scanI2C() {
  Serial.println(F("--- I2C BUS SCAN ---"));
  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  delay(10);
  Serial.print(F("  SDA(A4) Pin State: ")); Serial.println(digitalRead(A4) ? F("HIGH [OK]") : F("LOW [SHORT TO GND / BUS STUCK!]"));
  Serial.print(F("  SCL(A5) Pin State: ")); Serial.println(digitalRead(A5) ? F("HIGH [OK]") : F("LOW [SHORT TO GND / BUS STUCK!]"));
  Wire.begin();
  int count = 0;'''

assert target in code, "target not found"
code = code.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Bus pin probe added successfully!")
