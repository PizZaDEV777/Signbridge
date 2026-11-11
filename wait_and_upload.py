import time
import serial.tools.list_ports
import upload

print("[*] Waiting for Arduino to connect via USB (plug in anytime)...")
start = time.time()
uploaded = False

while time.time() - start < 35.0:
    port = upload.find_port()
    if port:
        print(f"[!] Detected Arduino on {port}! Starting upload in 1.5s...")
        time.sleep(1.5)
        if upload.upload():
            uploaded = True
            break
        else:
            time.sleep(1.0)
    time.sleep(0.8)

if not uploaded:
    print("[X] Timed out waiting for USB connection.")
