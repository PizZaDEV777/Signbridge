import serial
import serial.tools.list_ports
import time
import sys

ports = [p.device for p in serial.tools.list_ports.comports()]
if not ports:
    print("NO COM PORT DETECTED!")
    sys.exit(1)

port = 'COM3' if 'COM3' in ports else ports[0]
s = serial.Serial(port, 115200, timeout=0.5)

print(f"[*] Connected to {port}. Waiting for Arduino boot & calibration...")
t0 = time.time()
while time.time() - t0 < 15.0:
    raw = s.readline()
    if raw:
        l = raw.decode('ascii', errors='ignore').strip()
        if any(k in l for k in ['Found device', 'Total found', 'Initializing OLED', 'Initializing MPU', 'Baseline =', 'Ready for Gestures']):
            print("  [BOOT]", l)
        if 'Ready for Gestures' in l:
            break

time.sleep(0.5)
s.reset_input_buffer()

def send_cmd(cmd, title, max_wait=1.5, stop_str=None):
    print(f"\n=== {title} ===")
    s.write(cmd)
    t_start = time.time()
    while time.time() - t_start < max_wait:
        raw = s.readline()
        if raw:
            l = raw.decode('ascii', errors='ignore').strip()
            if l and not l.startswith('[FLEX]'):
                print(" ", l)
                if stop_str and stop_str in l:
                    break

send_cmd(b'v', "1. I2C BUS VOLTAGE (v)", 1.0, "BUS VOLTAGE")
send_cmd(b'i', "2. I2C BUS SCAN (i)", 4.5, "--------------------")
send_cmd(b'w', "3. LIVE WIRE DIAGNOSTIC (w)", 1.5, "--------------------")
send_cmd(b'o', "4. OLED INIT TEST (o)", 1.5, "OLED")

print("\n=== 5. LIVE FLEX & MPU TELEMETRY (d) ===")
s.write(b'd')
time.sleep(0.3)
t_d = time.time()
samples = []
while time.time() - t_d < 2.0:
    raw = s.readline()
    if raw:
        l = raw.decode('ascii', errors='ignore').strip()
        if '[FLEX]' in l:
            samples.append(l)
for l in samples[-5:]:
    print(" ", l)

send_cmd(b'1', "6. AUDIO TEST (Track 1)", 1.5, "Playing Track")
s.close()
print("\n[DONE]")
