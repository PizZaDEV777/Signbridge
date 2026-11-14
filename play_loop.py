import serial
import time

try:
    s = serial.Serial('COM3', 115200, timeout=0.3)
    print('[*] Connected to COM3. Waiting for boot (7s)...')
    t0 = time.time()
    while time.time() - t0 < 7.5:
        line = s.readline().decode('utf-8', errors='ignore').strip()
        if line and any(k in line for k in ['DFPlayer', 'READY', 'Baseline', 'CALIBRAT']):
            print('  [BOOT]', line)
            
    print('\n>>> STARTING AUDIO PLAYBACK LOOP (WATCH DFPLAYER LED NOW!) <<<')
    
    for cycle in range(1, 5):
        for trk in ['1', '2', '3']:
            print(f'\n[*] >>> TRIGGERING TRACK {trk} (Cycle {cycle})... <<<')
            s.write(trk.encode())
            t_start = time.time()
            while time.time() - t_start < 2.5:
                line = s.readline().decode('utf-8', errors='ignore').strip()
                if line and ('AUDIO' in line or 'Track' in line or 'Keyboard' in line):
                    print('   ', line)
                time.sleep(0.05)
                
    print('\n[*] Playback sequence complete.')
    s.close()
except Exception as e:
    print('Error:', e)
