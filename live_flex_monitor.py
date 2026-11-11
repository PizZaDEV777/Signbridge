import serial
import serial.tools.list_ports
import time
import sys

def find_port():
    for p in serial.tools.list_ports.comports():
        if p.device == 'COM3' or 'serial' in (p.description or '').lower() or 'arduino' in (p.description or '').lower():
            return p.device
    return None

port = find_port()
if not port:
    print("[X] Arduino not detected. Please plug the USB cable into your laptop!")
    sys.exit(1)

print(f"[*] Connecting to Arduino on {port} at 115200 baud...")
try:
    s = serial.Serial(port, 115200, timeout=1.0)
except Exception as e:
    print(f"Error opening {port}: {e}")
    sys.exit(1)

# Toggle DTR for clean restart
s.setDTR(False)
time.sleep(0.1)
s.setDTR(True)

print("[*] Waiting for Arduino calibration (5 seconds)...")
time.sleep(5.0)

# Send 'd' to enable diagnostic stream
s.write(b'd')
time.sleep(0.3)

print("\n" + "="*70)
print(">>> LIVE FLEX SENSOR & GESTURE MONITOR <<<")
print(">>> Bend your fingers one by one to see the live bend percentages! <<<")
print("="*70)

t_start = time.time()
try:
    while time.time() - t_start < 25.0:
        line = s.readline().decode('latin-1', errors='ignore').strip()
        if line and any(k in line for k in ['[FLEX]', 'GESTURE', 'Track', 'Bent', 'T(']):
            print(f"  {line}")
except KeyboardInterrupt:
    pass

s.close()
print("\n[*] Monitor finished.")
