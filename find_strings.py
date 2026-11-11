with open('src/main.cpp', 'r') as f:
    text = f.read()

import re
f_strings = re.findall(r'F\("([^"]+)"\)', text)
print('Total F strings:', len(f_strings))
long_strings = [s for s in f_strings if len(s) > 25]
print(f'Long F strings (>25 chars): {len(long_strings)}')
total_bytes = sum(len(s) for s in long_strings)
print(f'Total bytes in long strings: {total_bytes}')
for s in long_strings[:15]:
    print(' ', repr(s))
