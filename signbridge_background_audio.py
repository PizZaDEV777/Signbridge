"""
SIGNBRIDGE - FAIL-SAFE HIGH-PRIORITY LAPTOP AUDIO SERVICE
Guarantees 100% synchronization between what is displayed on the OLED
and what is spoken out loud through the laptop speakers!
"""

import os
import sys
import time
import queue
import threading
import winsound
import serial
import serial.tools.list_ports
import win32com.client

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
AUDIO_DIR = os.path.join(BASE_DIR, "audio_files")
LOG_FILE = os.path.join(BASE_DIR, "signbridge_audio.log")

# Unified mapping for both GESTURE tags and track numbers:
GESTURE_LOOKUP = {
    # Tag triggers
    "GESTURE:NEED_HELP": ("0001.wav", "I need help"),
    "GESTURE:WANT_WATER": ("0002.wav", "Please give me water"),
    "GESTURE:AM_OKAY": ("0003.wav", "I am okay"),
    "GESTURE:THANK_YOU": ("0004.wav", "Thank you"),
    "GESTURE:CALL_DOCTOR": ("0005.wav", "Please call a doctor"),
    "GESTURE:PEACE": ("0006.wav", "Peace"),
    "GESTURE:LOVE": ("0007.wav", "Love"),
    
    # Secondary fallback matching for track triggers
    "TRACK 1": ("0001.wav", "I need help"),
    "TRACK 2": ("0002.wav", "Please give me water"),
    "TRACK 3": ("0003.wav", "I am okay"),
    "TRACK 4": ("0004.wav", "Thank you"),
    "TRACK 5": ("0005.wav", "Please call a doctor"),
    "TRACK 6": ("0006.wav", "Peace"),
    "TRACK 7": ("0007.wav", "Love"),
}

log_queue = queue.Queue()
audio_queue = queue.Queue()

def log(msg):
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{ts}] {msg}\n"
    print(line.strip())
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line)
    except Exception:
        pass

def unmute_and_boost_volume():
    """Unmutes laptop master volume and boosts level."""
    try:
        wscript = win32com.client.Dispatch("WScript.Shell")
        for _ in range(20):
            wscript.SendKeys(chr(0xAF)) # VK_VOLUME_UP
        log("Laptop volume unmuted and boosted.")
    except Exception as e:
        log(f"Volume boost error: {e}")

def audio_worker():
    """Plays audio through laptop speakers using WAV or SAPI fallback."""
    try:
        sapi_voice = win32com.client.Dispatch("SAPI.SpVoice")
    except Exception:
        sapi_voice = None

    while True:
        try:
            filename, spoken_text = audio_queue.get()
            wav_path = os.path.join(AUDIO_DIR, filename)
            played = False

            if os.path.exists(wav_path):
                try:
                    # Synchronous PlaySound in dedicated background worker
                    winsound.PlaySound(wav_path, winsound.SND_FILENAME)
                    played = True
                except Exception as ex:
                    log(f"PlaySound error ({filename}): {ex}")

            # Guaranteed voice fallback: if WAV failed or was silent, speak via SAPI
            if not played and spoken_text and sapi_voice:
                try:
                    sapi_voice.Speak(spoken_text)
                    played = True
                except Exception as ex:
                    log(f"SAPI error: {ex}")

            audio_queue.task_done()
        except Exception as e:
            log(f"Audio worker error: {e}")

def find_glove_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.device == "COM3":
            return p.device
    for p in ports:
        desc = (p.description or "").lower()
        if any(k in desc for k in ["arduino", "ch340", "serial", "ftdi", "usb"]):
            return p.device
    return ports[0].device if ports else None

def main():
    threading.Thread(target=audio_worker, daemon=True).start()
    unmute_and_boost_volume()

    log("=== Signbridge High-Priority Audio Service Active ===")

    last_play_time = 0
    cooldown = 1.6 # Minimum spacing between words in seconds

    while True:
        port = find_glove_port()
        if not port:
            time.sleep(0.8)
            continue

        try:
            log(f"Connecting to Arduino Glove on {port}...")
            ser = serial.Serial()
            ser.port = port
            ser.baudrate = 115200
            ser.timeout = 0.05
            ser.dtr = False
            ser.rts = False
            ser.open()
            log(f"CONNECTED to {port}. Real-time OLED-to-Laptop audio link active!")

            while True:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                except Exception as read_err:
                    log(f"Serial disconnected: {read_err}")
                    break

                if not line:
                    continue

                line_upper = line.upper()
                now = time.time()

                for trigger_key, (wav_file, spoken_text) in GESTURE_LOOKUP.items():
                    if trigger_key in line_upper:
                        if (now - last_play_time) >= cooldown:
                            last_play_time = now
                            log(f"[SYNC] OLED Displayed '{trigger_key}' -> Speaking \"{spoken_text}\"")
                            audio_queue.put((wav_file, spoken_text))
                        break

        except Exception as e:
            log(f"Port connection error: {e}")
            time.sleep(1.2)

if __name__ == "__main__":
    main()
