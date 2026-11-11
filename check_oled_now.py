import serial
import time
import sys

print("[*] Connecting to COM3 at 115200 baud...")
try:
    s = serial.Serial('COM3', 115200, timeout=0.5)
except Exception as e:
    print(f"Error opening COM3: {e}")
    sys.exit(1)

print("[*] Reading boot output for 7 seconds...")
t0 = time.time()
while time.time() - t0 < 7.0:
    line = s.readline().decode('latin-1', errors='ignore').strip()
    if line:
        print("  [BOOT]", line)

print("\n" + "="*50)
print(">>> RUNNING I2C SCAN (Command 'i') <<<")
print("="*50)
s.write(b'i')
time.sleep(0.5)

t_scan = time.time()
while time.time() - t_scan < 3.0:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l and any(k in l for k in ['I2C', 'Found', 'Total', 'OLED', 'MPU', '0x']):
        print("  ", l)

print("\n" + "="*50)
print(">>> INITIALIZING OLED DISPLAY (Command 'o') <<<")
print("="*50)
s.write(b'o')
time.sleep(0.5)

t_o = time.time()
while time.time() - t_o < 2.0:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l:
        print("  [OLED]:", l)

s.close()
print("\n[OK] Scan finished.")
