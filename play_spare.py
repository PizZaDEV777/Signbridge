import serial
import time
import sys

def test_track(track_num=1):
    print(f"[*] Opening COM4 at 115200 baud...")
    s = serial.Serial('COM4', 115200, timeout=1)
    time.sleep(1.0)
    
    print(f"\n>>> SENDING COMMAND TO PLAY TRACK {track_num} <<<")
    s.write(str(track_num).encode('ascii'))
    s.flush()
    
    start = time.time()
    while time.time() - start < 3.0:
        line = s.readline().decode('latin-1', errors='ignore').strip()
        if line:
            print("  [UNO]:", line)
            
    s.close()
    print("[OK] Done.")

if __name__ == '__main__':
    t = sys.argv[1] if len(sys.argv) > 1 else '1'
    test_track(t)
