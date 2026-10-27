import os
import win32com.client

folder = r'c:\Users\m4713\OneDrive\Documents\Apps\Signbridge\audio_files'
os.makedirs(folder, exist_ok=True)

phrases = [
    ('0001.wav', 'I need help'),
    ('0002.wav', 'Please give me water'),
    ('0003.wav', 'I am okay'),
    ('0004.wav', 'Thank you'),
    ('0005.wav', 'Please call a doctor')
]

speaker = win32com.client.Dispatch('SAPI.SpVoice')

# Select Female voice (Zira) or Male (David)
voices = speaker.GetVoices()
for i in range(voices.Count):
    if 'zira' in voices.Item(i).GetDescription().lower():
        speaker.Voice = voices.Item(i)
        break

for filename, text in phrases:
    stream = win32com.client.Dispatch('SAPI.SpFileStream')
    filepath = os.path.join(folder, filename)
    stream.Open(filepath, 3, False)
    speaker.AudioOutputStream = stream
    speaker.Speak(text)
    stream.Close()
    print(f'Successfully created: {filename} -> "{text}"')
