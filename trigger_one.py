import serial
import time

try:
    s = serial.Serial('COM3', 115200, timeout=0.3)
    print('[*] Connected to COM3. Waiting for boot & calibration (7.5s)...')
    t0 = time.time()
    while time.time() - t0 < 7.5:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line and any(k in line for k in ['DFPlayer', 'READY', 'Baseline', 'CALIBRAT', 'Initialized']):
            print('  [BOOT]', line)
            
    print('\n>>> TRIGGERING TRACK 1 (Need Help) NOW! <<<')
    s.write(b'1')
    t0 = time.time()
    while time.time() - t0 < 3.5:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line and any(k in line for k in ['AUDIO', 'Track', 'Keyboard', 'GESTURE', 'FLEX']):
            print('   ->', line)
        time.sleep(0.05)
        
    print('\n>>> TRIGGERING TRACK 2 (Water) NOW! <<<')
    s.write(b'2')
    t0 = time.time()
    while time.time() - t0 < 3.5:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line and any(k in line for k in ['AUDIO', 'Track', 'Keyboard', 'GESTURE', 'FLEX']):
            print('   ->', line)
        time.sleep(0.05)
        
    s.close()
    print('\n[OK] Tracks 1 & 2 triggered!')
except Exception as e:
    print('Error:', e)
