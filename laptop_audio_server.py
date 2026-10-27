import os
import sys
import time
import serial
import serial.tools.list_ports
import win32com.client

AUDIO_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'audio_files')
BAUD = 115200

# Mapping of commands to MP3/WAV files
TRACK_MAP = {
    '1': os.path.join(AUDIO_DIR, '0001.mp3'),
    '2': os.path.join(AUDIO_DIR, '0002.mp3'),
    '3': os.path.join(AUDIO_DIR, '0003.mp3'),
    '4': os.path.join(AUDIO_DIR, '0004.mp3'),
    '5': os.path.join(AUDIO_DIR, '0005.mp3'),
    'NEED_HELP': os.path.join(AUDIO_DIR, '0001.mp3'),
    'WANT_WATER': os.path.join(AUDIO_DIR, '0002.mp3'),
    'AM_OKAY': os.path.join(AUDIO_DIR, '0003.mp3'),
    'THANK_YOU': os.path.join(AUDIO_DIR, '0004.mp3'),
    'CALL_DOCTOR': os.path.join(AUDIO_DIR, '0005.mp3')
}

GESTURE_DESCRIPTIONS = {
    '1': "I need help",
    '2': "Please give me water",
    '3': "I am okay",
    '4': "Thank you",
    '5': "Please call a doctor",
    'NEED_HELP': "I need help",
    'WANT_WATER': "Please give me water",
    'AM_OKAY': "I am okay",
    'THANK_YOU': "Thank you",
    'CALL_DOCTOR': "Please call a doctor",
    'PEACE': "Peace",
    'LOVE': "I love you"
}

# Windows Media Player Engine (built into Windows for playing high quality MP3/WAV)
wmp = win32com.client.Dispatch('WMPlayer.OCX')
wmp.settings.volume = 100

# Text-To-Speech engine as fallback
tts = win32com.client.Dispatch('SAPI.SpVoice')

def play_sound(key):
    key = key.strip().upper()
    phrase = GESTURE_DESCRIPTIONS.get(key, key.replace('_', ' ').title())

    if key in TRACK_MAP and os.path.exists(TRACK_MAP[key]):
        filepath = TRACK_MAP[key]
        print(f'\n>>> [LAPTOP AUDIO]: Playing \"{os.path.basename(filepath)}\" -> \"{phrase}\"')
        wmp.URL = filepath
        wmp.controls.play()
    else:
        # Fallback to SAPI Text-To-Speech
        print(f'\n>>> [TTS SPEAK]: \"{phrase}\"')
        tts.Speak(phrase)

def detect_port():
    """Auto-detects the Arduino serial port, or prompts if multiple are found."""
    if len(sys.argv) > 1 and sys.argv[1].upper().startswith('COM'):
        return sys.argv[1].upper()

    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None

    if len(ports) == 1:
        print(f"[*] Auto-detected port: {ports[0].device} ({ports[0].description})")
        return ports[0].device

    # Look for known Arduino/FTDI/CH340 identifiers
    for p in ports:
        desc = p.description.lower()
        if any(keyword in desc for keyword in ['arduino', 'ch340', 'ftdi', 'cp210', 'usb serial', 'usb-serial']):
            print(f"[*] Detected Arduino on: {p.device} ({p.description})")
            return p.device

    print("\nMultiple COM ports found:")
    for idx, p in enumerate(ports):
        print(f"  [{idx+1}] {p.device} - {p.description}")
    
    try:
        choice = input(f"Select port (1-{len(ports)}) or press Enter for {ports[0].device}: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(ports):
            return ports[int(choice)-1].device
    except Exception:
        pass

    return ports[0].device

def main():
    print('==================================================')
    print('  SIGNBRIDGE - Laptop Audio Bridge Running        ')
    print('==================================================')

    port = detect_port()
    if not port:
        print('[!] No serial devices found!')
        print('    1. Plug the Arduino into your laptop via USB.')
        print('    2. Re-run this script.')
        print('\n    Waiting for connection... (Press Ctrl+C to stop)')
        while not port:
            time.sleep(2)
            port = detect_port()

    print(f'[*] Connecting to Arduino on {port} at {BAUD} baud...')
    try:
        ser = serial.Serial(port, BAUD, timeout=1)
        time.sleep(2)  # Allow Arduino to reboot cleanly
        print(f'[✓] Connected to Arduino on {port}!')
        print('[*] Listening for gestures & motion triggers...\n')
    except Exception as e:
        print(f'[!] Could not open {port}: {e}')
        print('    Make sure the Arduino is plugged in and Serial Monitor is closed.')
        return

    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f'[ARDUINO]: {line}')
                
                # Check for triggers like "PLAY:1" or "GESTURE:NEED_HELP"
                if line.startswith('PLAY:'):
                    key = line.split('PLAY:')[1].strip()
                    play_sound(key)
                elif line.startswith('GESTURE:'):
                    key = line.split('GESTURE:')[1].strip()
                    play_sound(key)
        except KeyboardInterrupt:
            print('\nExiting Audio Bridge...')
            break
        except Exception as e:
            print(f'[!] Error reading serial: {e}')
            time.sleep(1)

if __name__ == '__main__':
    main()
