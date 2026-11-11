with open('src/main.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Remove SoftwareSerial include
code = code.replace('#include <SoftwareSerial.h>\n', '')

# 2. Replace SoftwareSerial instance and send_cmd with zero-RAM bit-banged UART TX on Pin 11
old_audio = '''// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
#define BUZZER_PIN 3
uint8_t dfPlayerVolume = 26; // Safe 26/30 prevents 5V brownout and delivers loud, clear voice
SoftwareSerial dfSerial(10, 11); // RX = Pin 10 (from DFPlayer TX), TX = Pin 11 (to DFPlayer RX)

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
  dfSerial.write(packet, 10);
}'''

new_audio = '''// ---------------------- AUDIO & SPEECH HARDWARE ------------------------------
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
}'''

assert old_audio in code, "old_audio block not found"
code = code.replace(old_audio, new_audio)

# 3. In setup(), configure DFPLAYER_TX_PIN
old_setup_pin = 'pinMode(LED_PIN, OUTPUT);'
new_setup_pin = '''pinMode(LED_PIN, OUTPUT);
  pinMode(DFPLAYER_TX_PIN, OUTPUT);
  digitalWrite(DFPLAYER_TX_PIN, HIGH); // UART idle state is HIGH'''
code = code.replace(old_setup_pin, new_setup_pin, 1)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("Zero-RAM UART replacement applied successfully!")
