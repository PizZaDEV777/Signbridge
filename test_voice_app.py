import time
import sys

try:
    import pyttsx3
except ImportError:
    print("Error: pyttsx3 not installed. Run 'pip install pyttsx3'")
    sys.exit(1)

# Initialize offline Text-To-Speech engine
engine = pyttsx3.init()

# Configure speech rate and volume
engine.setProperty('rate', 160)     # Natural speaking speed
engine.setProperty('volume', 1.0)   # Max volume

def speak(phrase: str):
    """Speaks the given phrase out loud through PC speakers."""
    print(f"\n[SIGNBRIDGE VOICE]: \"{phrase}\"")
    engine.say(phrase)
    engine.runAndWait()

def test_interactive_mode():
    """Allows testing gestures and voice responses directly with your keyboard."""
    print("==================================================")
    print("      SIGNBRIDGE - Voice Companion App (Test)    ")
    print("==================================================")
    print("Simulating gestures from keyboard:")
    print("  [1] -> 'I need help'")
    print("  [2] -> 'Please give me water'")
    print("  [3] -> 'I am okay'")
    print("  [4] -> 'Thank you'")
    print("  [5] -> 'Please call a doctor'")
    print("  [q] -> Quit")
    print("==================================================\n")

    gesture_map = {
        '1': "I need help",
        '2': "Please give me water",
        '3': "I am okay",
        '4': "Thank you",
        '5': "Please call a doctor"
    }

    # Welcome announcement
    speak("Hello Kritaya, This is Signbridge speaking, hopefully you're doing a good, this is me leaving off, also tell me if my voice is good or not okay?")

    while True:
        try:
            choice = input("Enter gesture number (1-5, or 'q' to quit): ").strip().lower()
            if choice == 'q':
                print("Exiting...")
                break
            elif choice in gesture_map:
                speak(gesture_map[choice])
            else:
                print("Invalid input. Please enter 1, 2, 3, 4, 5, or q.")
        except KeyboardInterrupt:
            print("\nExiting...")
            break

if __name__ == "__main__":
    test_interactive_mode()
