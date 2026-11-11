with open('src/main.cpp', 'r') as f:
    text = f.read()

# Replace 50-character decorative divider lines with shorter dividers
old_div = '=================================================='
new_div = '---'

print(f"Occurrences of divider: {text.count(old_div)}")
text = text.replace(old_div, new_div)

with open('src/main.cpp', 'w') as f:
    f.write(text)

import os, subprocess
pio = os.path.expandvars(r'%USERPROFILE%\.platformio\penv\Scripts\pio.exe')
res = subprocess.run([pio, 'run', '-e', 'uno'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
for line in res.stdout.splitlines():
    if 'RAM:' in line or 'Flash:' in line or 'SUCCESS' in line or 'FAILED' in line or 'error' in line.lower():
        print(line)
