with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

old_rc = '''    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    delayMicroseconds(25);
    digitalWrite(2, LOW); // Turn off pull-up before switching to input
    pinMode(2, INPUT);    // High-Z: discharges strictly through flex sensor to GND
    uint16_t count = 0;
    while ((PIND & (1 << PD2)) && count < 1000) {
      count++;
    }'''

new_rc = '''    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);
    delayMicroseconds(50);
    pinMode(2, INPUT);     // Switch to high-impedance input
    digitalWrite(2, LOW);  // Disable internal pullup without driving pin low
    uint16_t count = 0;
    while ((PIND & (1 << PD2)) && count < 2000) {
      count++;
    }'''

assert old_rc in code, "old_rc not found"
code = code.replace(old_rc, new_rc)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("RC discharge timing fixed for Pinky on D2!")
