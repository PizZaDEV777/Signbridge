with open('src/main.cpp', 'r') as f:
    code = f.read()

# 1. Set volume to safe 23 to prevent brownout reset
code = code.replace('uint8_t dfPlayerVolume = 30;', 'uint8_t dfPlayerVolume = 23; // Safe 23/30 prevents 5V brownout!')

# 2. In setup, comment out startup boot chime so boot doesn't spike current before entering loop
old_boot_audio = '''  // Play boot welcome chime: Track 1 ("Help")
  Serial.println(F("[*] Startup Audio Test: Triggering Track 1..."));
  playVoiceTrack(1);'''

new_boot_audio = '''  // Startup Audio: Volume set to safe 23/30 (no brownout)
  Serial.println(F("[*] DFPlayer Ready at Volume 23/30 (No Brownout)"));'''

assert old_boot_audio in code, "old_boot_audio not found"
code = code.replace(old_boot_audio, new_boot_audio)

with open('src/main.cpp', 'w') as f:
    f.write(code)

print("Brownout protection applied!")
