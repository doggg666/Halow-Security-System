# HaLow Security System

ESP-IDF/LVGL HMI for the CrowPanel Advance 5-inch ESP32-S3 (800×480), with Wi-Fi/HaLow status and camera system controls.

## Contents

- `project/`: source, dependency lock, board configuration, and UI previews
- `firmware/`: flashed application image, bootloader, partition table, and flash instructions

See [project/README.md](project/README.md) for build requirements, board wiring, setup, and limitations. Start with [firmware/START-HERE.txt](firmware/START-HERE.txt) to flash the included image.

The camera stream pipeline is still under development; the current firmware provides the HMI and networking controls.

HaLow firmware and board support are subject to their upstream licenses. The supplied Morse Micro BCF and license notices are included in this repository.
