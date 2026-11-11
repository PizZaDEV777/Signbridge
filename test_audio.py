import serial
import time

print("[*] Connecting to COM3 at 115200 baud...")
try:
    s = serial.Serial('COM3', 115200, timeout=1)
    print("[*] Waiting 5s for glove boot and calibration...")
    time.sleep(5.0)
    s.reset_input_buffer()
    
    print("\n" + "="*50)
    print(">>> SENDING AUDIO PLAY COMMAND: TRACK 1 (0001.mp3) <<<")
    print("="*50)
    
    # Send '1' over serial to trigger Track 1
    s.write(b'1')
    s.flush()
    
    start = time.time()
    while time.time() - start < 4.0:
        l = s.readline().decode('latin-1', errors='ignore').strip()
        if l and any(k in l for k in ['[AUDIO]', 'Track', 'Keyboard', 'GESTURE', 'DFPlayer']):
            print(f"  {l}")
            
    print("\n" + "="*50)
    print(">>> SENDING AUDIO PLAY COMMAND: TRACK 2 (0002.mp3) <<<")
    print("="*50)
    
    # Send '2' over serial to trigger Track 2
    s.write(b'2')
    s.flush()
    
    start = time.time()
    while time.time() - start < 4.0:
        l = s.readline().decode('latin-1', errors='ignore').strip()
        if l and any(k in l for k in ['[AUDIO]', 'Track', 'Keyboard', 'GESTURE', 'DFPlayer']):
            print(f"  {l}")

    s.close()
    print("\n[OK] Playback test commands sent successfully!")

except Exception as e:
    print(f"[X] Serial communication error: {e}")
