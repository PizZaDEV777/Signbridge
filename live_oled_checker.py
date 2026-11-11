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
print("[*] Waiting for Arduino to finish boot...")
time.sleep(6.0)
s.reset_input_buffer()

print("\n" + "="*50)
print(">>> LIVE OLED CONNECTION MONITOR RUNNING <<<")
print(">>> As you plug/adjust OLED wires, this will detect it immediately! <<<")
print("="*50)

found = False
t_start = time.time()
while time.time() - t_start < 45.0:
    # Send 'i' (scan I2C)
    s.write(b'i')
    time.sleep(0.3)
    
    # Read scan response
    lines = []
    t_wait = time.time()
    while time.time() - t_wait < 0.8:
        l = s.readline().decode('latin-1', errors='ignore').strip()
        if l and any(k in l for k in ['Found device', 'Total found', '0x3C', '0x3D']):
            lines.append(l)
    
    scan_text = " | ".join(lines)
    if "0x3C" in scan_text or "0x3D" in scan_text:
        print(f"\n[🎉 SUCCESS!] OLED DETECTED: {scan_text}")
        print("[*] Sending 'o' to initialize display...")
        s.write(b'o')
        time.sleep(0.5)
        while True:
            l = s.readline().decode('latin-1', errors='ignore').strip()
            if not l: break
            print("  ", l)
        found = True
        break
    else:
        print(f"[{time.strftime('%H:%M:%S')}] Scanning... Devices found: {scan_text if scan_text else 'Waiting...'}")
    
    time.sleep(1.2)

s.close()
if not found:
    print("\n[!] OLED not detected within 45s. Check VCC/GND/SDA/SCL wiring.")
