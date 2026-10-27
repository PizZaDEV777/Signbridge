# Signbridge Glove - Hardware & Motion Detection Testing Guide

This guide is designed for testing the **Flex Sensors** and **MPU6050 Wrist Module** with your team.

---

## 1. Wiring & Pinout Guide

### I2C Bus (Shared by MPU6050 & SSD1306 OLED)
Both the MPU6050 (GY-521) motion sensor and the SSD1306 OLED display connect to the Arduino Uno's I2C pins in parallel:

| Arduino Uno Pin | MPU6050 Pin | SSD1306 OLED Pin | Function |
| :--- | :--- | :--- | :--- |
| **A4** | **SDA** | **SDA** | I2C Data line |
| **A5** | **SCL** | **SCL** | I2C Clock line |
| **5V** | **VCC** | **VCC** | Power |
| **GND** | **GND** | **GND** | Ground |

> **Note on MPU6050:** Leave `AD0` disconnected (or connected to GND) so its I2C address defaults to `0x68`.

---

### Flex Sensors (Voltage Divider Circuit)
Each flex sensor connects as an analog voltage divider with a pull-down resistor (typically **10kΩ to 22kΩ**):

```text
       +5V
        │
    [Flex Sensor]
        │
        ├───> To Arduino Analog Pin (A0, A1, A2, A3)
        │
     [10kΩ - 22kΩ Resistor]
        │
       GND
```

#### Pin Assignment:
* **A0**: Thumb
* **A1**: Index Finger
* **A2**: Middle Finger
* **A3**: Ring / Pinky Finger

*(Note: Pins A4 and A5 are dedicated to I2C for the MPU6050 & OLED).*

---

## 2. Testing Steps for Today

### Step 1: Upload the Firmware
From PlatformIO in VS Code:
Click the **Upload** arrow at the bottom status bar, or run in terminal:
```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
```

### Step 2: Launch the Laptop Audio Bridge
In a terminal on your laptop:
```powershell
python laptop_audio_server.py
```
* The script will **auto-detect your Arduino COM port** and connect at 115200 baud.
* Whenever a gesture is detected, your laptop speakers will immediately play the voice announcement.

---

## 3. Interactive Serial Tools & Shortcuts

You can send single-character commands via the Serial Monitor (115200 baud) for testing:

| Command | Action | Purpose |
| :---: | :--- | :--- |
| **`c`** | **Run Auto-Calibration** | Guides you to hold hand flat for 3s, then close into a fist for 3s. Automatically maps straight and bent limits for your flex sensors! |
| **`p`** | **Toggle Serial Plotter** | Formats output for the **Arduino IDE Serial Plotter** (`Tools > Serial Plotter`) to see live visual waveforms of finger flexion and wrist tilt! |
| **`d`** | **Toggle Raw Diagnostics** | Prints live raw ADC values (0–1023), bend percentages, Pitch, Roll, and dynamic motion energy. |
| **`1` – `5`** | **Manual Audio Trigger** | Test audio triggers 1 through 5 directly through the bridge. |

---

## 4. Gesture Mapping Reference

| Gesture | Hand & Finger State | Wrist / Motion State | Spoken Voice Output |
| :--- | :--- | :--- | :--- |
| **I Need Help** | Tight Fist (all fingers bent) | Any | *"I need help"* |
| **Please Give Me Water** | Index finger extended straight, others bent | Flat / Pointing forward | *"Please give me water"* |
| **I Am Okay** | All fingers flat & straight | Flat / Neutral | *"I am okay"* |
| **Thank You** | Thumbs Up (Thumb straight, others bent) | Tilted | *"Thank you"* |
| **Call Doctor** | Any | Rapid wrist wave / vigorous shake | *"Please call a doctor"* |

---

## 5. Troubleshooting Checklist

1. **"MPU6050: FAILED!" on boot:**
   * Check your SDA/SCL connections: MPU6050 `SDA` must connect to `A4`, `SCL` must connect to `A5`.
   * Verify MPU6050 power LED is lit.
2. **Flex sensor readings not changing:**
   * Verify the pull-down resistor (10kΩ) is connected between the analog pin and GND.
   * Send `'d'` in Serial to inspect raw ADC numbers while bending.
3. **Fingers detected backwards (shows bent when straight):**
   * Simply press `'c'` to run the 2-step auto-calibration routine. It dynamically adjusts for inverted voltage divider configurations.
