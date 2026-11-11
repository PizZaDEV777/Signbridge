import os
import sys
import time
import subprocess
import serial
import serial.tools.list_ports

HEX_PATH = os.path.abspath('.pio/build/uno/firmware.hex')
AVRDUDE = os.path.expandvars(r'%USERPROFILE%\.platformio\packages\tool-avrdude\avrdude.exe')
CONF = os.path.expandvars(r'%USERPROFILE%\.platformio\packages\tool-avrdude\avrdude.conf')

def find_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.device == 'COM3':
            return p.device
    for p in ports:
        desc = (p.description or '').lower()
        if any(k in desc for k in ['arduino', 'ch340', 'serial', 'ftdi', 'usb']):
            return p.device
    return ports[0].device if ports else None

print("[*] Waiting for Arduino USB cable to be plugged into laptop (listening for 5 minutes)...")
port = None
for attempt in range(300):
    port = find_port()
    if port:
        print(f"[+] USB Port Detected: {port}!")
        break
    time.sleep(1)

if not port:
    print("[-] No USB serial port detected after 60 seconds.")
    sys.exit(1)

time.sleep(1.0)
print(f"[*] Uploading firmware to {port}...")

cmd = [
    AVRDUDE, '-C', CONF, '-p', 'm328p', '-c', 'arduino', '-P', port, '-b', '115200', '-D',
    '-U', f'flash:w:{HEX_PATH}:i'
]

flashed = False
for retries in range(3):
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if res.returncode == 0:
        print("[OK] FIRMWARE FLASHED SUCCESSFULLY!\n")
        flashed = True
        break
    else:
        print(f"[!] Upload attempt {retries+1} failed, retrying in 1s...")
        time.sleep(1.0)

if not flashed:
    print("[X] Could not flash firmware. Output:")
    print(res.stdout)
    sys.exit(1)

time.sleep(1.5)
print("[*] Connecting to Serial Monitor at 115200 baud to run live tests...")
try:
    with serial.Serial(port, 115200, timeout=1.0) as s:
        # Toggle DTR to trigger fresh reset
        s.setDTR(False)
        time.sleep(0.1)
        s.setDTR(True)
        time.sleep(0.5)

        t_end = time.time() + 15.0
        while time.time() < t_end:
            line = s.readline().decode('latin-1', errors='ignore').strip()
            if line:
                print(f"  [DEVICE]: {line}")
except Exception as e:
    print(f"Serial read error: {e}")

print("\n[*] Live diagnostic complete.")
