"""
SIGNBRIDGE - SIMULTANEOUS DUAL AUDIO (LAPTOP + GLOVE SPEAKER)
Both the laptop speakers and the glove's DFPlayer Mini speaker play simultaneously
whenever any gesture is performed!
"""

import os
import sys
import time
import threading
import winsound
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
AUDIO_DIR = os.path.join(BASE_DIR, "audio_files")

GESTURE_MAP = {
    "GESTURE:NEED_HELP": {
        "text": "HELP!",
        "desc": "Closed Fist ✊",
        "audio": os.path.join(AUDIO_DIR, "0001.wav"),
        "color": "#FF3344",
        "speech": "Help!"
    },
    "GESTURE:WANT_WATER": {
        "text": "WATER",
        "desc": "Pointing Index ☝️",
        "audio": os.path.join(AUDIO_DIR, "0002.wav"),
        "color": "#00D4FF",
        "speech": "Water."
    },
    "GESTURE:AM_OKAY": {
        "text": "I'M OKAY",
        "desc": "OK Sign 👌",
        "audio": os.path.join(AUDIO_DIR, "0003.wav"),
        "color": "#00FF88",
        "speech": "I'm okay."
    },
    "GESTURE:THANK_YOU": {
        "text": "THANK YOU",
        "desc": "Thumbs Up 👍",
        "audio": os.path.join(AUDIO_DIR, "0004.wav"),
        "color": "#FFD700",
        "speech": "Thank you."
    },
    "GESTURE:CALL_DOCTOR": {
        "text": "DOCTOR!",
        "desc": "Flat Hand 👋",
        "audio": os.path.join(AUDIO_DIR, "0005.wav"),
        "color": "#FF5533",
        "speech": "Doctor!"
    },
    "GESTURE:PEACE": {
        "text": "PEACE",
        "desc": "Peace Sign ✌️",
        "audio": os.path.join(AUDIO_DIR, "0006.wav"),
        "color": "#33E0FF",
        "speech": "Peace."
    },
    "GESTURE:LOVE": {
        "text": "LOVE",
        "desc": "I Love You 🤟",
        "audio": os.path.join(AUDIO_DIR, "0007.wav"),
        "color": "#FF66CC",
        "speech": "Love."
    }
}

class ThreadedAudioPlayer:
    def __init__(self):
        self.last_play_time = 0
        self.lock = threading.Lock()
        self.cooldown = 1.6 # Minimum spacing between words in seconds

    def play(self, audio_path, force=False):
        now = time.time()
        with self.lock:
            if not force and (now - self.last_play_time < self.cooldown):
                return False
            self.last_play_time = now

        if not os.path.exists(audio_path):
            print(f"[!] Audio file missing: {audio_path}")
            return False

        # Dedicated playback worker thread: plays completely without race conditions or cuts
        def worker():
            try:
                winsound.PlaySound(audio_path, winsound.SND_FILENAME)
            except Exception as e:
                print(f"[!] PlaySound error: {e}")

        t = threading.Thread(target=worker, daemon=True)
        t.start()
        return True

class SignbridgeApp:
    def __init__(self, root):
        self.root = root
        self.root.title("SignBridge - Dual Audio System (Laptop + Speaker)")
        self.root.geometry("880x660")
        self.root.minsize(780, 560)
        self.root.configure(bg="#0D1117")

        self.player = ThreadedAudioPlayer()
        self.ser = None
        self.is_running = True
        self.gesture_clear_timer = None

        self.fingers = ["Thumb", "Index", "Middle", "Ring", "Pinky"]
        self.finger_pcts = {f: 0 for f in self.fingers}

        self.build_ui()
        self.bind_hotkeys()

        # Start serial worker in background
        self.serial_thread = threading.Thread(target=self.serial_worker, daemon=True)
        self.serial_thread.start()

    def build_ui(self):
        header = tk.Frame(self.root, bg="#161B22", height=70)
        header.pack(fill=tk.X, padx=0, pady=0)

        title = tk.Label(
            header,
            text="SIGNBRIDGE GLOVE",
            font=("Segoe UI", 20, "bold"),
            fg="#58A6FF",
            bg="#161B22"
        )
        title.pack(side=tk.LEFT, padx=20, pady=12)

        subtitle = tk.Label(
            header,
            text="Dual Audio Active: Laptop + Glove Speaker",
            font=("Segoe UI", 11),
            fg="#8B949E",
            bg="#161B22"
        )
        subtitle.pack(side=tk.LEFT, padx=0, pady=16)

        self.status_lbl = tk.Label(
            header,
            text="SEARCHING FOR GLOVE...",
            font=("Segoe UI", 10, "bold"),
            fg="#FFA657",
            bg="#21262D",
            padx=12,
            pady=4
        )
        self.status_lbl.pack(side=tk.RIGHT, padx=20, pady=15)

        # Center Gesture Banner
        banner_frame = tk.Frame(self.root, bg="#161B22", bd=2, relief=tk.GROOVE)
        banner_frame.pack(fill=tk.X, padx=20, pady=16)

        subbanner = tk.Label(
            banner_frame,
            text="RECOGNIZED SIGN / VOICE OUTPUT",
            font=("Segoe UI", 10, "bold"),
            fg="#8B949E",
            bg="#161B22"
        )
        subbanner.pack(pady=(12, 0))

        self.gesture_lbl = tk.Label(
            banner_frame,
            text="READY - HOLD A GESTURE",
            font=("Segoe UI", 36, "bold"),
            fg="#7EE787",
            bg="#161B22",
            pady=16
        )
        self.gesture_lbl.pack()

        self.desc_lbl = tk.Label(
            banner_frame,
            text="Dual Sound: Speaking through Laptop & Glove Top Speaker simultaneously",
            font=("Segoe UI", 12),
            fg="#8B949E",
            bg="#161B22"
        )
        self.desc_lbl.pack(pady=(0, 14))

        # Mid Frame
        mid_frame = tk.Frame(self.root, bg="#0D1117")
        mid_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=0)

        gauge_box = tk.LabelFrame(
            mid_frame,
            text=" Live Finger Deflection ",
            font=("Segoe UI", 11, "bold"),
            fg="#58A6FF",
            bg="#161B22",
            padx=15,
            pady=12
        )
        gauge_box.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 10))

        self.gauge_bars = {}
        self.gauge_val_lbls = {}

        for f in self.fingers:
            row = tk.Frame(gauge_box, bg="#161B22")
            row.pack(fill=tk.X, pady=6)

            lbl = tk.Label(row, text=f"{f:<7}", font=("Segoe UI", 10, "bold"), fg="#C9D1D9", bg="#161B22", width=7, anchor="w")
            lbl.pack(side=tk.LEFT)

            canvas = tk.Canvas(row, width=170, height=18, bg="#21262D", highlightthickness=0)
            canvas.pack(side=tk.LEFT, padx=8)
            bar = canvas.create_rectangle(0, 0, 0, 18, fill="#58A6FF", width=0)
            self.gauge_bars[f] = (canvas, bar)

            v_lbl = tk.Label(row, text="0%", font=("Segoe UI", 10, "bold"), fg="#58A6FF", bg="#161B22", width=5, anchor="e")
            v_lbl.pack(side=tk.LEFT)
            self.gauge_val_lbls[f] = v_lbl

        log_box = tk.LabelFrame(
            mid_frame,
            text=" Real-Time Recognition Log ",
            font=("Segoe UI", 11, "bold"),
            fg="#58A6FF",
            bg="#161B22",
            padx=10,
            pady=10
        )
        log_box.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(10, 0))

        self.log_list = tk.Listbox(
            log_box,
            font=("Consolas", 10),
            bg="#0D1117",
            fg="#C9D1D9",
            selectbackground="#21262D",
            highlightthickness=0,
            bd=0
        )
        self.log_list.pack(fill=tk.BOTH, expand=True)

        demo_frame = tk.LabelFrame(
            self.root,
            text=" Demo Voice Buttons (Click or Press 1-7) ",
            font=("Segoe UI", 10, "bold"),
            fg="#8B949E",
            bg="#161B22",
            padx=10,
            pady=10
        )
        demo_frame.pack(fill=tk.X, padx=20, pady=(12, 16))

        btn_row = tk.Frame(demo_frame, bg="#161B22")
        btn_row.pack(fill=tk.X)

        test_buttons = [
            ("1: HELP", "GESTURE:NEED_HELP", "#FF3344"),
            ("2: WATER", "GESTURE:WANT_WATER", "#00D4FF"),
            ("3: I'M OKAY", "GESTURE:AM_OKAY", "#00FF88"),
            ("4: THANK YOU", "GESTURE:THANK_YOU", "#FFD700"),
            ("5: DOCTOR", "GESTURE:CALL_DOCTOR", "#FF5533"),
            ("6: PEACE", "GESTURE:PEACE", "#33E0FF"),
            ("7: LOVE", "GESTURE:LOVE", "#FF66CC"),
        ]

        for text, g_key, col in test_buttons:
            btn = tk.Button(
                btn_row,
                text=text,
                font=("Segoe UI", 9, "bold"),
                bg="#21262D",
                fg=col,
                activebackground=col,
                activeforeground="#0D1117",
                relief=tk.FLAT,
                padx=8,
                pady=6,
                command=lambda k=g_key: self.trigger_manual(k)
            )
            btn.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=3)

    def bind_hotkeys(self):
        self.root.bind("1", lambda e: self.trigger_manual("GESTURE:NEED_HELP"))
        self.root.bind("2", lambda e: self.trigger_manual("GESTURE:WANT_WATER"))
        self.root.bind("3", lambda e: self.trigger_manual("GESTURE:AM_OKAY"))
        self.root.bind("4", lambda e: self.trigger_manual("GESTURE:THANK_YOU"))
        self.root.bind("5", lambda e: self.trigger_manual("GESTURE:CALL_DOCTOR"))
        self.root.bind("6", lambda e: self.trigger_manual("GESTURE:PEACE"))
        self.root.bind("7", lambda e: self.trigger_manual("GESTURE:LOVE"))
        self.root.bind("c", lambda e: self.send_serial("c"))
        self.root.bind("C", lambda e: self.send_serial("c"))

    def send_serial(self, char):
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(char.encode('utf-8'))
                self.log_event(f"[COMMAND] Sent '{char}' to Arduino")
            except Exception:
                pass

    def trigger_manual(self, g_key):
        if g_key in GESTURE_MAP:
            info = GESTURE_MAP[g_key]
            self.player.play(info["audio"], force=True)
            self.update_ui_on_gesture(g_key, source="DEMO BUTTON")

    def log_event(self, msg):
        timestamp = time.strftime("%H:%M:%S")
        entry = f"[{timestamp}] {msg}"
        self.log_list.insert(0, entry)
        if self.log_list.size() > 50:
            self.log_list.delete(50, tk.END)

    def update_finger_gauges(self, pcts):
        for f, pct in pcts.items():
            if f in self.gauge_bars:
                canvas, bar = self.gauge_bars[f]
                w = int((min(100, max(0, pct)) / 100.0) * 170)
                canvas.coords(bar, 0, 0, w, 18)
                color = "#FF3344" if pct >= 35 else "#58A6FF"
                canvas.itemconfig(bar, fill=color)
                self.gauge_val_lbls[f].config(text=f"{pct}%", fg=color)

    def update_ui_on_gesture(self, g_key, source="GLOVE"):
        if g_key not in GESTURE_MAP:
            return
        info = GESTURE_MAP[g_key]
        self.gesture_lbl.config(text=info["text"], fg=info["color"])
        self.desc_lbl.config(
            text=f"Spoken: \"{info['speech']}\" [{info['desc']}]  [Dual Audio: Laptop + Glove Speaker]",
            fg=info["color"]
        )
        self.log_event(f"SPEECH: \"{info['speech']}\" ({info['text']}) via {source}")

        if self.gesture_clear_timer:
            self.root.after_cancel(self.gesture_clear_timer)
        self.gesture_clear_timer = self.root.after(2800, self.reset_banner)

    def reset_banner(self):
        self.gesture_lbl.config(text="LISTENING...", fg="#8B949E")
        self.desc_lbl.config(text="Ready for Next Gesture", fg="#484F58")

    def find_serial_port(self):
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            if p.device == "COM3":
                return p.device
        for p in ports:
            desc = (p.description or "").lower()
            if "arduino" in desc or "ch340" in desc or "serial" in desc or "ftdi" in desc:
                return p.device
        return ports[0].device if ports else None

    def serial_worker(self):
        while self.is_running:
            port = self.find_serial_port()
            if not port:
                self.root.after(0, lambda: self.status_lbl.config(text="NO DEVICE", fg="#FF7B72", bg="#21262D"))
                time.sleep(1.0)
                continue

            try:
                self.ser = serial.Serial(port, 115200, timeout=0.08, dsrdtr=False)
                self.root.after(0, lambda p=port: self.status_lbl.config(text=f"CONNECTED ({p})", fg="#7EE787", bg="#238636"))
                self.log_event(f"Connected to Glove on {port} - Dual Audio Active!")
                
                # Test chime on connection so user knows laptop audio is working
                try:
                    winsound.MessageBeep(winsound.MB_ICONASTERISK)
                except Exception:
                    pass

                while self.is_running:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if not line:
                        continue

                    # Instant gesture trigger check
                    for g_key in GESTURE_MAP.keys():
                        if g_key in line:
                            # 1. Play sound immediately in background thread
                            info = GESTURE_MAP[g_key]
                            played = self.player.play(info["audio"])
                            # 2. Update UI
                            self.root.after(0, lambda k=g_key: self.update_ui_on_gesture(k, source="GLOVE"))
                            break

                    # Check for finger percentage data
                    if "[FLEX]" in line:
                        try:
                            parts = line.split("[FLEX]")[1].split("|")[0].strip().split()
                            new_pcts = {}
                            name_map = {'T': 'Thumb', 'I': 'Index', 'M': 'Middle', 'R': 'Ring', 'P': 'Pinky'}
                            for part in parts:
                                if ':' in part and '(' in part and '%)' in part:
                                    initial = part.split(':')[0].strip()
                                    pct_str = part.split('(')[1].split('%)')[0].strip()
                                    if initial in name_map:
                                        new_pcts[name_map[initial]] = int(pct_str)
                            if new_pcts:
                                self.root.after(0, lambda p=new_pcts: self.update_finger_gauges(p))
                        except Exception:
                            pass

            except Exception as e:
                self.root.after(0, lambda: self.status_lbl.config(text="DISCONNECTED", fg="#FF7B72", bg="#21262D"))
                self.log_event(f"Glove disconnected: {e}")
                if self.ser:
                    try:
                        self.ser.close()
                    except Exception:
                        pass
                self.ser = None
                time.sleep(1.5)

    def close(self):
        self.is_running = False
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = SignbridgeApp(root)
    root.protocol("WM_DELETE_WINDOW", app.close)
    root.mainloop()
