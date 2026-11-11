with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Swap FLEX_PINS so Index is A1 and Middle is A2
old_pins = """#define NUM_FLEX 5
uint8_t FLEX_PINS[NUM_FLEX] = {A0, A2, A1, A3, 2};
// A0: Thumb  (Analog A0)
// A2: Index  (Analog A2 - mapped to A2 where index sensor connects)
// A1: Middle (Analog A1 - mapped to A1)
// A3: Ring   (Analog A3)
// D2: Pinky  (Digital Pin 2)"""

new_pins = """#define NUM_FLEX 5
uint8_t FLEX_PINS[NUM_FLEX] = {A0, A1, A2, A3, 2};
// A0: Thumb  (Analog A0)
// A1: Index  (Analog A1)
// A2: Middle (Analog A2 - with 100k pull-down resistor)
// A3: Ring   (Analog A3)
// D2: Pinky  (Digital Pin 2)"""

assert old_pins in code, "old_pins not found"
code = code.replace(old_pins, new_pins, 1)

# 2. Update spanMax so Index (i == 1 on A1) has tailored sensitivity
old_span = """      // Tailored span: Middle (index 2) deflects ~25-35 counts; others deflect ~60-80 counts
      const uint16_t spanMax = (i == 2) ? 35 : 75;"""

new_span = """      // Tailored span: Index (index 1 on A1) deflects ~35-45 counts; others deflect ~75+ counts
      const uint16_t spanMax = (i == 1) ? 38 : 75;"""

assert old_span in code, "old_span not found"
code = code.replace(old_span, new_span, 1)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("SUCCESS: Index (A1) and Middle (A2) swapped and calibrated!")
