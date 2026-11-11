with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

target = '''  readFlexSensors();

  // readMPU6050(); // Temporarily bypassed to test OLED

  // classifyOrientation();

  detectGesture();'''

replacement = '''  readFlexSensors();

  // Read MPU6050 at silky 33Hz rate (prevents I2C bus congestion)
  static unsigned long lastMpuSample = 0;
  if (millis() - lastMpuSample >= 30) {
    lastMpuSample = millis();
    readMPU6050();
    classifyOrientation();
  }

  detectGesture();'''

assert target in code, "target not found"
code = code.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("MPU-6050 re-enabled with 33Hz rate limiter!")
