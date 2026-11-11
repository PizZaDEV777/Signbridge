import serial
import time
import sys

print("[*] Opening COM3 at 115200 baud...")
try:
    s = serial.Serial('COM3', 115200, timeout=0.5)
except Exception as e:
    print(f"Error opening COM3: {e}")
    sys.exit(1)

print("[*] Waiting for glove calibration to finish...")
t0 = time.time()
ready = False
while time.time() - t0 < 10.0:
    line = s.readline().decode('latin-1', errors='ignore').strip()
    if line:
        if "Ready for Gestures" in line or "DFPlayer Ready" in line:
            print("  [GLOVE READY]", line)
            ready = True
            break
        elif any(k in line for k in ["Calibrating", "Baseline", "SIGNBRIDGE", "RESET", "Found"]):
            print("  [BOOT]", line)

if not ready:
    print("[*] Wait 2 more seconds for stability...")
    time.sleep(2.0)

s.reset_input_buffer()
time.sleep(0.5)

# Boost volume to 26/30 for clear audio
print("\n" + "="*50)
print(">>> RAISING VOLUME (+ + +) TO 26/30 <<<")
print("="*50)
s.write(b'+')
time.sleep(0.1)
s.write(b'+')
time.sleep(0.1)
s.write(b'+')
time.sleep(0.2)

# Read responses
t_vol = time.time()
while time.time() - t_vol < 1.0:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l and "Volume" in l:
        print("  ", l)

time.sleep(0.5)

# Trigger Track 1 ("Help")
print("\n" + "="*50)
print(">>> TRIGGERING TRACK 1 ('Help') <<<")
print("="*50)
s.write(b'1')
s.flush()

t_track = time.time()
while time.time() - t_track < 4.0:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l and any(k in l for k in ['AUDIO', 'Track', 'RESET', 'Volume', 'DFPlayer']):
        print("  ", l)

time.sleep(1.0)

# Trigger Track 7 ("Love") - known to exist on SD card!
print("\n" + "="*50)
print(">>> TRIGGERING TRACK 7 ('Love') <<<")
print("="*50)
s.write(b'7')
s.flush()

t_track7 = time.time()
while time.time() - t_track7 < 5.0:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l and any(k in l for k in ['AUDIO', 'Track', 'RESET', 'Volume', 'DFPlayer']):
        print("  ", l)

print("\n[*] Waiting 3 seconds for playback to finish before closing...")
time.sleep(3.0)
s.close()
print("[OK] Test finished.")
