with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# Temporarily comment out readMPU6050 and classifyOrientation in loop to test I2C bus contention
target = '''  readFlexSensors();

  readMPU6050();

  classifyOrientation();

  detectGesture();'''

replacement = '''  readFlexSensors();

  // readMPU6050(); // Temporarily bypassed to test OLED

  // classifyOrientation();

  detectGesture();'''

assert target in code, "target not found"
code = code.replace(target, replacement)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Temporarily bypassed readMPU6050 in loop for OLED test!")
