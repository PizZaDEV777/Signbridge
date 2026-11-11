import serial
import time
import sys

print("="*60)
print(">>> SIGNBRIDGE - FULL HARDWARE & SYSTEM HEALTH CHECK <<<")
print("="*60)

try:
    s = serial.Serial('COM3', 115200, timeout=1.0)
except Exception as e:
    print(f"Error opening COM3: {e}")
    sys.exit(1)

# Toggle DTR for clean reboot
s.setDTR(False)
time.sleep(0.1)
s.setDTR(True)

print("[*] Reading Boot & Sensor Calibration output (8 seconds)...")
t_end = time.time() + 9.0
boot_lines = []
while time.time() < t_end:
    line = s.readline().decode('latin-1', errors='ignore').strip()
    if line:
        boot_lines.append(line)
        print("  [BOOT]", line)

print("\n" + "="*60)
print("[*] Sending 'd' command to fetch live sensor & MPU telemetry...")
print("="*60)
s.write(b'd')
time.sleep(0.5)

sensor_lines = []
t_diag = time.time() + 4.0
while time.time() < t_diag:
    line = s.readline().decode('latin-1', errors='ignore').strip()
    if line:
        sensor_lines.append(line)
        print("  [SENSORS]", line)

print("\n" + "="*60)
print("[*] Testing Audio System: Sending command '1' to trigger Track 1...")
print("="*60)
s.write(b'1')
time.sleep(0.5)

audio_lines = []
t_audio = time.time() + 3.0
while time.time() < t_audio:
    line = s.readline().decode('latin-1', errors='ignore').strip()
    if line:
        audio_lines.append(line)
        print("  [AUDIO]", line)

s.close()
print("\n[OK] Diagnostic session complete.")
