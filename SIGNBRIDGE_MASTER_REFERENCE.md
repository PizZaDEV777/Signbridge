# Signbridge Smart Glove: Master Hardware & Gesture Reference Guide

This document is the **single source of truth** for all hardware wiring, pin assignments, electrical safeguards, and gesture recognition tables for the Signbridge Smart Glove.

---

# PART 1: COMPLETE HARDWARE CONNECTION & MOUNTING DIRECTORY

## 1.0 Visual Mounting & Wearable Hardware Layout

![Signbridge Smart Glove Hardware Mounting Layout](file:///C:/Users/m4713/.gemini/antigravity/brain/736bd8b4-bcc5-4d62-8e5a-e2fe15755d68/signbridge_glove_hardware_layout_1792583622354.jpg)

### Physical Component Placement Zones:
1. **Fingers (Dorsal Back Side):** 5 flex ribbons inside slim fabric channels along Thumb, Index, Middle, Ring, and Pinky.
2. **Back of Hand (Metacarpals):** MPU-6050 6-DOF IMU fixed rigid and flat between knuckles and wrist.
3. **Wrist Cuff (Smartwatch Position):** 0.96" SSD1306 OLED screen facing the wearer.
4. **Forearm Strap / Base:** DFPlayer Mini, 8Ω dynamic speaker, and Arduino Uno with bundled strain-relieved wiring.

---

## 1.1 Complete Arduino Uno Pin Allocation Map

> [!IMPORTANT]
> **Pins 0 (RX) and 1 (TX) MUST remain 100% disconnected** from all breadboard jumpers. They are reserved exclusively for USB firmware flashing and PC serial communication.

```text
               ┌───────────────────────────────┐
               │          ARDUINO UNO          │
               │                               │
       [NC] ───┤ 0  (RX)              (SCL) D15├── [NC]
       [NC] ───┤ 1  (TX)              (SDA) D14├── [NC]
Pinky Flex In ─┤ 2  (Digital In)          (AREF)├── [NC]
       [NC] ───┤ 3* (Buzzer - Disabled)   (GND)├── Breadboard Common GND Rail
       [NC] ───┤ 4                        (D13)├── Built-in LED
       [NC] ───┤ 5*                       (D12)├── [NC]
       [NC] ───┤ 6*                       (D11)├── DFPlayer RX (Module Pin 2)
       [NC] ───┤ 7                        (D10)├── DFPlayer TX (Module Pin 3)
       [NC] ───┤ 8                         (D9)├── [NC]
       [NC] ───┤ 9*                        (D8)├── [NC]
               │                               │
5V Power Rail ─┤ 5V                        (A5)├── SCL (Shared: OLED + MPU-6050)
Common GND Rail┤ GND                       (A4)├── SDA (Shared: OLED + MPU-6050)
Common GND Rail┤ GND                       (A3)├── Ring Flex Analog In (A3)
       [NC] ───┤ Vin                       (A2)├── Middle Flex Analog In (A2)
               │                           (A1)├── Index Flex Analog In (A1)
               │                           (A0)├── Thumb Flex Analog In (A0)
               └───────────────────────────────┘
```

---

## 1.2 Module-by-Module Wiring Directory

### A. DFPlayer Mini (`MP3-TF-16P V3.0`) Voice Processor
* **Visual Landmark:** Identify the **bright white printed circle / dot** at the corner of the module. That is **Pin 1**.
* **Crucial Rule:** The speaker connects **strictly between Pin 6 and Pin 8**. Pin 7 (GND) is in between them and must be skipped. Never connect the speaker to GND or 5V.

| Module Pin # | Pin Label | Connects To | Purpose & Engineering Notes |
| :--- | :--- | :--- | :--- |
| **Pin 1** | **VCC** | **Arduino 5V Power Rail** | Main logic & 3W on-board amplifier power (White dot corner) |
| **Pin 2** | **RX** | **Arduino Pin 11** | Cycle-accurate 9600 baud UART command receive line |
| **Pin 3** | **TX** | **Arduino Pin 10** | Cycle-accurate 9600 baud UART status telemetry line |
| **Pin 4** | **DAC_R** | *[LEAVE EMPTY]* | Line-level audio (unpowered, not for speakers) |
| **Pin 5** | **DAC_L** | *[LEAVE EMPTY]* | Line-level audio (unpowered, not for speakers) |
| **Pin 6** | **SPK_1** | **Speaker Wire 1 (Red)** | **Direct differential amplifier output terminal 1** |
| **Pin 7** | **GND** | **Arduino GND Rail** | **Module Ground reference (DO NOT connect speaker here!)** |
| **Pin 8** | **SPK_2** | **Speaker Wire 2 (Black)** | **Direct differential amplifier output terminal 2 (Bottom corner)** |
| **Pins 9–16** | **Right Side**| *[LEAVE ALL EMPTY]* | IO triggers, USB lines, and busy status pins |

> [!CAUTION]
> **Why the speaker must NEVER touch Pin 7 (GND):**
> The DFPlayer's internal `8002` amplifier outputs a balanced Bridge-Tied Load (BTL) signal. Both Pin 6 and Pin 8 sit at +2.5V DC. 
> * Pin 6 to Pin 8 = $2.5\text{V} - 2.5\text{V} = \mathbf{0\text{V DC}}$ (Speaker stays 100% cool!).
> * If you accidentally connect a speaker wire to Pin 7 (GND), 2.5V of continuous direct current flows through the 8Ω coil ($I = 312\text{mA}$), causing the speaker to heat up rapidly and hum!

---

### B. 8-Ohm Dynamic Voice Speaker
* **Terminal 1 (Red / + Wire):** Connects to **DFPlayer Pin 6 (`SPK_1`)**.
* **Terminal 2 (Black / - Wire):** Connects to **DFPlayer Pin 8 (`SPK_2`)**.
* **Resistors:** **ZERO resistors**. Direct connection only.

---

### C. 0.96" SSD1306 I2C OLED Display (128x64 Pixels, Address `0x3C`)

| OLED Pin | Connects To | Wire Color Suggestion | Notes |
| :--- | :--- | :--- | :--- |
| **GND** | **Arduino GND Rail** | Black / Blue | Common system ground |
| **VCC** | **Arduino 5V Rail** | Red | Powers display logic & charge pump |
| **SCL** | **Arduino Pin A5** | Yellow / White | I2C Clock Line (100kHz standard) |
| **SDA** | **Arduino Pin A4** | Green / Blue | I2C Data Line (Shared with MPU-6050) |

---

### D. MPU-6050 6-DOF IMU (Accelerometer & Gyroscope, Address `0x68`)

| MPU Pin | Connects To | Wire Color Suggestion | Notes |
| :--- | :--- | :--- | :--- |
| **VCC** | **Arduino 5V Rail** | Red | Uses on-board 3.3V low-dropout regulator |
| **GND** | **Arduino GND Rail** | Black | Common system ground |
| **SCL** | **Arduino Pin A5** | Yellow / White | Shared with OLED SCL |
| **SDA** | **Arduino Pin A4** | Green / Blue | Shared with OLED SDA |
| **AD0** | **Arduino GND Rail** | Black | Hardwires I2C bus address to `0x68` |
| **INT** | *[LEAVE EMPTY]* | - | Interrupt pin not required |

---

### E. 5-Finger Glove Flex Sensors

| Finger | Sensor Type | Signal Pin | Voltage Divider Circuit |
| :--- | :--- | :--- | :--- |
| **Thumb** | Analog Flex Ribbon | **Arduino A0** | One leg $\rightarrow$ **5V Rail**. Other leg $\rightarrow$ **A0** AND **10kΩ resistor to GND**. |
| **Index** | Analog Flex Ribbon | **Arduino A1** | One leg $\rightarrow$ **5V Rail**. Other leg $\rightarrow$ **A1** AND **10kΩ resistor to GND**. |
| **Middle** | Analog Flex Ribbon | **Arduino A2** | One leg $\rightarrow$ **5V Rail**. Other leg $\rightarrow$ **A2** AND **10kΩ resistor to GND**. |
| **Ring** | Analog Flex Ribbon | **Arduino A3** | One leg $\rightarrow$ **5V Rail**. Other leg $\rightarrow$ **A3** AND **10kΩ resistor to GND**. |
| **Pinky** | Digital Switch/Flex | **Arduino Pin 2** | One leg $\rightarrow$ **Pin 2**. Other leg $\rightarrow$ **GND Rail**. (Uses internal `INPUT_PULLUP`). |

---

# PART 2: GESTURE RECOGNITION & VOICE EMISSION MATRIX

The Signbridge firmware uses a **Single-Shot Latch**:
1. When a gesture is held for **350ms**, the voice plays **once**.
2. The system locks in silence while the gesture is held (preventing repetitive audio loops).
3. **Returning hand to Neutral (Open Flat Hand) immediately resets and unlocks the system** for the next gesture.

| # | Sign Name | Physical Hand Configuration | Detection Logic | OLED Screen Banner | Audio Track | Spoken Voice Output |
| :-: | :--- | :--- | :--- | :--- | :-: | :--- |
| **0** | **NEUTRAL / RESTING** | Hand completely flat, palm level, all 5 fingers straight. | All flex sensors $<35\%$ bent; wrist level. | `Listening...` | *None* | *(Silent / Ready)* |
| **1** | **NEED HELP** | **Closed Fist** ✊<br>All 5 fingers tightly curled into a fist. | At least 4 or 5 fingers bent $>55\%$. | `HELP!` | `0001.mp3` | 🗣️ **"I need help"** |
| **2** | **WANT WATER** | **Pointing Index** ☝️<br>Index extended forward; Middle, Ring, Pinky curled. | Index $<35\%$ (straight); Mid, Ring, Pinky $>55\%$ (bent). | `WATER` | `0002.mp3` | 🗣️ **"Please give me water"** |
| **3** | **I AM OKAY** | **Universal "OK" Sign** 👌<br>Thumb and Index tips touching in a loop; Mid, Ring, Pinky straight. | Thumb & Index $>55\%$ (bent); Mid, Ring, Pinky $<35\%$ (straight). | `I'M OKAY` | `0003.mp3` | 🗣️ **"I am okay"** |
| **4** | **THANK YOU** | **Thumbs Up** 👍<br>Thumb pointing straight up; 4 fingers closed into fist. | Thumb $<35\%$ (straight); Index, Mid, Ring, Pinky $>55\%$ (bent). | `THANK YOU` | `0004.mp3` | 🗣️ **"Thank you"** |
| **5** | **CALL DOCTOR** | **Emergency Hand Wave** 👋<br>Vigorous, rapid hand wave or shaking of the wrist. | MPU-6050 Dynamic Accel $>3.8g$ OR Gyro Energy $>140^\circ/\text{s}$. | `DOCTOR!`<br>`[WAVE!]` | `0005.mp3` | 🗣️ **"Please call a doctor"** |
| **6** | **PEACE SIGN** | **Peace / Victory Sign** ✌️<br>Index and Middle extended upright; Ring & Pinky closed. | Index & Mid straight; Ring & Pinky bent. | `PEACE` | `0003.mp3` | 🗣️ **"I am okay"** |
| **7** | **I LOVE YOU** | **ASL "I Love You" Sign** 🤟<br>Thumb, Index, and Pinky fully extended; Middle & Ring curled. | Thumb, Index, Pinky straight; Mid & Ring bent. | `LOVE` | `0004.mp3` | 🗣️ **"Thank you"** |

---

# PART 3: AUDIO STORAGE & MICRO-SD INDEX

* **Card Format:** FAT32 (4GB to 32GB Micro-SD Card).
* **Storage Structure:** Audio tracks exist in both the root directory (`/`) and the `/mp3/` folder:

```text
D:\
├── 0001.mp3      [Phrase: "I need help"]
├── 0002.mp3      [Phrase: "Please give me water"]
├── 0003.mp3      [Phrase: "I am okay"]
├── 0004.mp3      [Phrase: "Thank you"]
├── 0005.mp3      [Phrase: "Please call a doctor"]
└── mp3/
    ├── 0001.mp3
    ├── 0002.mp3
    ├── 0003.mp3
    ├── 0004.mp3
    └── 0005.mp3
```

---

# PART 4: OPERATIONAL CHECKLIST & TROUBLESHOOTING

| Symptom | Primary Cause | Immediate Fix |
| :--- | :--- | :--- |
| **Speaker heats up** | Speaker wire is touching Pin 7 (GND) or an Arduino digital pin. | Move speaker wires strictly to **Pin 6** and **Pin 8**. Ensure Pin 7 (GND) is empty between them! |
| **0 Sound from speaker** | Resistor in series with speaker, or volume too low. | Remove any resistors on speaker wires. Firmware is locked to maximum volume 30/30. |
| **OLED screen black / flickers** | Loose VCC jumper wire in breadboard. | Firmly push OLED VCC wire into 5V rail; ensure SDA is in A4 and SCL is in A5. |
| **Speech only plays once** | Single-Shot Latch is active. | Open hand completely flat (`Listening...`) to unlock the next gesture. |
| **Upload sync error (`stk500`)** | Serial port busy streaming live sensor telemetry. | Use the automated Python DTR upload script or hold the Uno Reset button until upload begins. |
