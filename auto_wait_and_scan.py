import serial
import serial.tools.list_ports
import time
import sys

print("[*] Waiting for Arduino to be plugged in on USB...")
port = None
t_wait_plug = time.time()
while time.time() - t_wait_plug < 60.0:
    ports = [p.device for p in serial.tools.list_ports.comports() if 'COM' in p.device]
    if 'COM3' in ports:
        port = 'COM3'
        break
    elif ports:
        port = ports[0]
        break
    time.sleep(0.5)

if not port:
    print("[!] Timed out waiting for USB connection.")
    sys.exit(1)

print(f"[*] Arduino detected on {port}! Waiting 1.5s for USB enumeration...")
time.sleep(1.5)

try:
    s = serial.Serial(port, 115200, timeout=0.5)
except Exception as e:
    print(f"Error opening {port}: {e}")
    sys.exit(1)

print("[*] Capturing boot sequence...")
t0 = time.time()
while time.time() - t0 < 6.5:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l:
        print("  [BOOT]", l)

print("\n" + "="*50)
print(">>> RUNNING I2C SCAN COMMAND ('i') <<<")
print("="*50)
s.write(b'i')
time.sleep(0.5)

t_scan = time.time()
while time.time() - t_scan < 3.0:
    l = s.readline().decode('latin-1', errors='ignore').strip()
    if l and any(k in l for k in ['I2C', 'Found', 'Total', 'OLED', 'MPU', '0x']):
        print("  ", l)

print("\n" + "="*50)
print(">>> INITIALIZING OLED DISPLAY ('o') <<<")
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
