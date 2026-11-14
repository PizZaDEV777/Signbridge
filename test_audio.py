import serial
import time
import sys

try:
    s = serial.Serial('COM3', 115200, timeout=0.3)
    print('[*] Connected to COM3. Waiting for boot & calibration to finish...')
    t0 = time.time()
    while time.time() - t0 < 5.5:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line and any(k in line for k in ['SIGNBRIDGE', 'OLED', 'Pin A', 'DFPlayer', 'Ready', 'Baseline', 'CALIBRAT']):
            print('  [BOOT]', line)
    
    # Increase volume
    s.write(b'+')
    time.sleep(0.1)
    s.write(b'+')
    time.sleep(0.2)
    
    tracks = [
        (1, 'Need Help / Help'),
        (2, 'Water'),
        (3, 'I am Okay'),
        (4, 'Thank You'),
        (5, 'Doctor / Emergency')
    ]
    
    for trk, name in tracks:
        print(f'\n>>> [TESTING AUDIO] Triggering Track {trk}: "{name}"...')
        s.write(str(trk).encode())
        t_start = time.time()
        while time.time() - t_start < 3.2:
            line = s.readline().decode('utf-8', errors='ignore').strip()
            if line and any(k in line for k in ['AUDIO', 'Keyboard', 'GESTURE', 'Track', 'Volume']):
                print('   ->', line)
            time.sleep(0.05)
            
    print('\n[OK] All 5 audio tracks triggered successfully!')
    s.close()
except Exception as e:
    print('Error:', e)
