import serial
import time
import sys

print("[*] Connecting to COM3 at 115200 baud for Live OLED Watcher...")
try:
    s = serial.Serial('COM3', 115200, timeout=0.5)
except Exception as e:
    print(f"Error opening COM3: {e}")
    sys.exit(1)

# Wait past boot
time.sleep(5.0)
s.reset_input_buffer()

print("\n" + "="*50)
print(">>> LIVE OLED SCAN RUNNING (60 SECONDS) <<<")
print(">>> Plug Yellow -> A4 and White -> A5 now! <<<")
print("="*50)

found = False
start_time = time.time()
while time.time() - start_time < 60.0:
    s.write(b'i')
    time.sleep(0.3)
    
    t_end = time.time() + 1.0
    devices = []
    while time.time() < t_end:
        line = s.readline().decode('latin-1', errors='ignore').strip()
        if "Found device at:" in line:
            devices.append(line.replace("Found device at: ", ""))
    
    dev_str = ", ".join(devices) if devices else "None"
    print(f"[{time.strftime('%H:%M:%S')}] I2C Scan: [ {dev_str} ]")
    
    if any("0x3C" in d or "0x3D" in d for d in devices):
        print("\n" + "*"*50)
        print(">>> [🎉 BINGO!] OLED DETECTED ON I2C BUS! <<<")
        print("*"*50)
        s.write(b'o') # Trigger OLED initialization!
        time.sleep(0.5)
        while True:
            l = s.readline().decode('latin-1', errors='ignore').strip()
            if not l: break
            print("  [OLED]:", l)
        found = True
        break
        
    time.sleep(1.2)

s.close()
if not found:
    print("\n[-] OLED still not detected.")
