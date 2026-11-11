import os
import sys
import time
import subprocess
import serial.tools.list_ports

HEX_PATH = os.path.abspath('.pio/build/uno/firmware.hex')
AVRDUDE = os.path.expandvars(r'%USERPROFILE%\.platformio\packages\tool-avrdude\avrdude.exe')
CONF = os.path.expandvars(r'%USERPROFILE%\.platformio\packages\tool-avrdude\avrdude.conf')

def find_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.device == 'COM3':
            return p.device
    for p in ports:
        desc = (p.description or '').lower()
        if 'arduino' in desc or 'ch340' in desc or 'serial' in desc or 'ftdi' in desc:
            return p.device
    return ports[0].device if ports else None

def upload():
    port = find_port()
    if not port:
        print('[X] Error: No Arduino USB connection detected.')
        return False

    cmd = [
        AVRDUDE, '-C', CONF, '-p', 'm328p', '-c', 'arduino', '-P', port, '-b', '115200', '-D',
        '-U', f'flash:w:{HEX_PATH}:i'
    ]
    print(f'[*] Uploading {HEX_PATH} to {port}...')
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=20)
        print(p.stdout)
        if p.returncode == 0:
            print('\n[OK] SUCCESS! FIRMWARE FLASHED!')
            return True
    except subprocess.TimeoutExpired:
        print('[X] Upload timed out!')
        return False
    except Exception as e:
        print(f'[X] Upload error: {e}')
        return False

    # Retry once if needed
    time.sleep(0.5)
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=20)
        print(p.stdout)
        return p.returncode == 0
    except Exception:
        return False

if __name__ == '__main__':
    upload()
