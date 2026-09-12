A feature-rich, real-time automotive dashboard for the **Hosyond 4.0-inch ESP32-32E
CYD** with a 320×480 ST7796S TFT, used in **480×320 landscape** orientation. It is
purpose-built for Ford Mustang enthusiasts and displays live OBD-II data with a
Mustang-themed UI, configurable RGB666 boot image, SD card logging, and a
fully configurable theme system.

## Hardware Requirements & Wiring

### Required Components

| Component | Description |
|---|---|
| Hosyond 4.0″ ESP32-32E CYD | ESP32-D0WD-V3 + 4″ 320×480 ST7796S TFT, used at 480×320 landscape, with resistive touch |
| ELM327 Bluetooth | OBD-II adapter, v1.5 or later (avoid cheap clones) |
| MicroSD card | FAT32 formatted, ≤32 GB recommended |
| 12V → 5V USB adapter | Powers the CYD from the vehicle's OBD port or 12V rail |

### CYD Pin Reference

The Hosyond 4.0″ ESP32-32E CYD uses the following display and SD-card pins. The display
uses the ST7796S controller over SPI and the dashboard uses it in landscape orientation.

| Function | GPIO |
|---|---|
| TFT MOSI | GPIO 13 |
| TFT MISO | GPIO 12 |
| TFT SCLK | GPIO 14 |
| TFT CS | GPIO 15 |
| TFT DC | GPIO 2 |
| TFT BL (backlight) | GPIO 27 |
| Touch CS | GPIO 33 |
| SD CS | GPIO 5 |
| SD MOSI | GPIO 23 |
| SD MISO | GPIO 19 |
| SD CLK | GPIO 18 |

> **Note:** The SD card shares its own SPI bus (VSPI) separate from the TFT bus (HSPI). Both can operate simultaneously without bus conflicts.

### Wiring Overview Diagram

```
Vehicle OBD-II Port
        │
   ELM327 Adapter  ──── Bluetooth ────►  ESP32 (CYD)
                                              │
                                         ST7796S TFT  ──► 480×320 landscape display
                                              │
                                         SD Card Slot ──► CSV logging
                                              │
                                      12V→5V USB adapter (from OBD port pin 16)
```
