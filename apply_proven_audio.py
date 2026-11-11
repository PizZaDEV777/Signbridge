with open('backups/main.cpp.glove_stable_phase3', 'r') as f:
    code = f.read()

# 1. Add SoftwareSerial.h include if not present
if '#include <SoftwareSerial.h>' not in code:
    code = '#include <SoftwareSerial.h>\n' + code

# 2. Replace audio section
old_audio = """// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
#define BUZZER_PIN 3
#define DFPLAYER_TX_PIN 11
uint8_t dfPlayerVolume = 26;

void dfplayer_send_byte(uint8_t b) {
  // Precise 9600 baud bit-bang: 104us per bit (Zero RAM buffer!)
  cli();
  digitalWrite(DFPLAYER_TX_PIN, LOW); // Start bit
  delayMicroseconds(103);
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(DFPLAYER_TX_PIN, (b & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(103);
  }
  digitalWrite(DFPLAYER_TX_PIN, HIGH); // Stop bit
  delayMicroseconds(103);
  sei();
}

void dfplayer_send_cmd(uint8_t cmd, uint16_t arg) {
  uint16_t checksum = 0 - (0xFF + 0x06 + cmd + 0x00 + (arg >> 8) + (arg & 0xFF));
  uint8_t packet[10] = {
    0x7E, 0xFF, 0x06, cmd, 0x00,
    (uint8_t)(arg >> 8), (uint8_t)(arg & 0xFF),
    (uint8_t)(checksum >> 8), (uint8_t)(checksum & 0xFF),
    0xEF
  };
  for (uint8_t i = 0; i < 10; i++) {
    dfplayer_send_byte(packet[i]);
  }
}

void playVoiceTrack(uint8_t track) {
  Serial.print(F("[AUDIO] Playing Track "));
  Serial.print(track);
  Serial.println(F(" via DFPlayer Mini (Pin 11 TX)..."));
  dfplayer_send_cmd(0x03, track); // Universal command 0x03: Play track (0001.mp3 - 0007.mp3)
}"""

new_audio = """// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
#define BUZZER_PIN 3
#define DFPLAYER_TX_PIN 11
#define DFPLAYER_RX_PIN 10
SoftwareSerial dfSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
uint8_t dfPlayerVolume = 30; // Maximum volume (30/30) for loud, clear voice output!

void dfplayer_send_cmd(uint8_t cmd, uint16_t arg) {
  uint16_t checksum = 0 - (0xFF + 0x06 + cmd + 0x00 + (arg >> 8) + (arg & 0xFF));
  uint8_t packet[10] = {
    0x7E,           // Start byte
    0xFF,           // Version
    0x06,           // Length
    cmd,            // Command
    0x00,           // Feedback (0 = no ACK required)
    (uint8_t)(arg >> 8),   // Param High
    (uint8_t)(arg & 0xFF), // Param Low
    (uint8_t)(checksum >> 8),
    (uint8_t)(checksum & 0xFF),
    0xEF            // End byte
  };

  for (uint8_t i = 0; i < 10; i++) {
    dfSerial.write(packet[i]);
    delayMicroseconds(250); // Inter-byte delay for MH2024K FIFO stability
  }
}

void playVoiceTrack(uint8_t track) {
  Serial.print(F("[AUDIO] Playing Track "));
  Serial.print(track);
  Serial.println(F(" via DFPlayer Mini (Pin 11 TX)..."));
  
  dfplayer_send_cmd(0x06, dfPlayerVolume); // Ensure volume is 30/30
  delay(30);
  dfplayer_send_cmd(0x03, track); // Universal command 0x03
  delay(40);
  dfplayer_send_cmd(0x12, track); // /MP3/ folder command 0x12
}"""

assert old_audio in code, 'old_audio block not found'
code = code.replace(old_audio, new_audio)

# 3. Clean up setup pins
old_setup_pin = """  pinMode(LED_PIN, OUTPUT);
  pinMode(DFPLAYER_TX_PIN, OUTPUT);
  digitalWrite(DFPLAYER_TX_PIN, HIGH); // UART idle state is HIGH"""

new_setup_pin = """  pinMode(LED_PIN, OUTPUT);"""

assert old_setup_pin in code, 'old_setup_pin not found'
code = code.replace(old_setup_pin, new_setup_pin)

# 4. Clean up audio init in setup
old_audio_init = """  // 6. Initialize DFPlayer Mini Audio System
  Serial.print(F("[*] Initializing DFPlayer Mini (Pin 11 TX)... Volume: "));
  Serial.print(dfPlayerVolume);
  Serial.println(F("/30"));
  dfplayer_send_cmd(0x06, dfPlayerVolume); // Command 0x06: Set volume
  delay(150);

  // Play boot welcome chime: Track 1 ("Help")
  Serial.println(F("[*] Startup Audio Test: Triggering Track 1..."));
  playVoiceTrack(1);"""

new_audio_init = """  // 6. Initialize DFPlayer Mini Audio System
  Serial.print(F("[*] Initializing DFPlayer Mini (Pin 11 TX)... Volume: "));
  Serial.print(dfPlayerVolume);
  Serial.println(F("/30"));
  dfSerial.begin(9600);
  delay(1500); // Allow DFPlayer to boot and mount SD card
  dfplayer_send_cmd(0x09, 0x02); // Select TF Micro-SD card as playback source
  delay(150);
  dfplayer_send_cmd(0x06, dfPlayerVolume); // Command 0x06: Set volume to 30/30
  delay(150);

  // Play boot welcome chime: Track 1 ("Help")
  Serial.println(F("[*] Startup Audio Test: Triggering Track 1..."));
  playVoiceTrack(1);"""

assert old_audio_init in code, 'old_audio_init not found'
code = code.replace(old_audio_init, new_audio_init)

with open('src/main.cpp', 'w') as f:
    f.write(code)

print('Patch applied successfully!')
