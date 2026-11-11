import serial
import time

print("[*] Connecting to COM3 at 115200 baud...")
try:
    s = serial.Serial('COM3', 115200, timeout=1)
    print("[*] Waiting 4.5s for glove boot...")
    time.sleep(4.5)
    s.reset_input_buffer()
    
    # Send '+' 4 times to set volume to MAX 30/30
    print("[*] Setting Volume to MAX (30/30)...")
    for _ in range(4):
        s.write(b'+')
        time.sleep(0.05)
    s.flush()
    time.sleep(0.5)
    
    print("\n" + "="*50)
    print(">>> TRIGGERING TRACK 1 NOW! (WATCH DFPLAYER LED) <<<")
    print("="*50)
    s.write(b'1')
    s.flush()
    
    start = time.time()
    while time.time() - start < 4.0:
        l = s.readline().decode('latin-1', errors='ignore').strip()
        if l and any(k in l for k in ['[AUDIO]', 'Track', 'Volume']):
            print(f"  {l}")
            
    print("\n" + "="*50)
    print(">>> TRIGGERING TRACK 2 NOW! (WATCH DFPLAYER LED) <<<")
    print("="*50)
    s.write(b'2')
    s.flush()
    
    start = time.time()
    while time.time() - start < 4.0:
        l = s.readline().decode('latin-1', errors='ignore').strip()
        if l and any(k in l for k in ['[AUDIO]', 'Track', 'Volume']):
            print(f"  {l}")

    s.close()
    print("\n[OK] Play commands finished!")

except Exception as e:
    print(f"[X] Serial error: {e}")
