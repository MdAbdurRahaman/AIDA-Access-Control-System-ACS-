# AIDA-ACS
AI-Driven Distributed Access Control System

This repository currently contains the ESP32 firmware developed using
the Arduino framework for an AI-assisted access control system.

The ESP32-based integrated board communicates with a remote AI server
over the internet to receive access decisions and control door hardware.


## 📁 Uploading Images to ESP32 (SPIFFS)

This project stores images inside ESP32 flash using SPIFFS (no SD card required).

### Folder Structure

```plaintext
data/images/bg.jpg
data/images/bg3.jpg
```

### How to Upload Images

1. Place your images inside the `data/images/` folder
2. Open the project in Arduino IDE
3. Use:

```
Tools → ESP32 Sketch Data Upload
```

This will upload the images to ESP32 internal storage.

### Important Notes

* Image format: **JPG**
* Recommended size: **320x240**
* Paths must match exactly (`/images/...`)
* You must upload data **after flashing code**

If images do not show:

* Check SPIFFS upload
* Verify file paths
* Check Serial Monitor for errors



🚧 Work in progress — AI modules and documentation will be added later.