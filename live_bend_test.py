import serial
import time
import re

try:
    s = serial.Serial('COM3', 115200, timeout=1)
    print('Waiting 5s for glove calibration...')
    time.sleep(5.0)
    s.reset_input_buffer()
    
    print('>>> TEST STARTED - BEND YOUR FINGERS NOW! (25 SECONDS) <<<')
    start = time.time()
    
    stats = {
        'T': {'base': None, 'min': 9999, 'max': 0, 'max_pct': 0},
        'I': {'base': None, 'min': 9999, 'max': 0, 'max_pct': 0},
        'M': {'base': None, 'min': 9999, 'max': 0, 'max_pct': 0},
        'R': {'base': None, 'min': 9999, 'max': 0, 'max_pct': 0},
        'P': {'base': None, 'min': 9999, 'max': 0, 'max_pct': 0},
    }
    
    pat = re.compile(r'([TIMRP])\([A-Z0-9]+\):(\d+)\((\d+)%\)')
    last_print = 0
    
    while time.time() - start < 25.0:
        line = s.readline().decode('latin-1', errors='ignore').strip()
        if '[FLEX]' in line:
            matches = pat.findall(line)
            any_active = False
            for f, val_s, pct_s in matches:
                v = int(val_s)
                p = int(pct_s)
                if stats[f]['base'] is None:
                    stats[f]['base'] = v
                if v < stats[f]['min']: stats[f]['min'] = v
                if v > stats[f]['max']: stats[f]['max'] = v
                if p > stats[f]['max_pct']: stats[f]['max_pct'] = p
                if p > 10: any_active = True
            
            now = time.time()
            if any_active or (now - last_print > 2.5):
                last_print = now
                remaining = int(25 - (now - start))
                print(f'[{remaining:02d}s left] {line}')
                
    s.close()
    print('\n==================== 25-SECOND BEND TEST SUMMARY ====================')
    for f in ['T', 'I', 'M', 'R', 'P']:
        d = stats[f]
        b = d['base']
        mn = d['min']
        mx = d['max']
        mp = d['max_pct']
        status = 'WORKING' if mp >= 20 else ('MARGINAL' if mp > 5 else 'NO DEFLECTION')
        print(f'Finger {f}: Baseline={b} | ADC Range=[{mn}, {mx}] | Max Bend={mp}% | Verdict={status}')
    print('=====================================================================')

except Exception as e:
    print('Serial error:', e)
