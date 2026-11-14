# HaLow Security System

ESP-IDF/LVGL HMI project for the CrowPanel Advance 5-inch ESP32-S3 V1.0 (800×480), designed for a three-camera HaLow security system.

## Project

Source, setup instructions, board notes, and interface previews are in [project/](project/README.md). The build uses ESP-IDF 5.5.6 and pins its managed dependencies.

The source defaults leave camera endpoints blank. Enter local camera addresses and network credentials on the device or configure them locally before building. No site-specific camera addresses or credentials are included in the public source.

The camera streaming path is still under development. Driver and board compatibility limitations are documented in the project README.

## Firmware

The prebuilt firmware image was rebuilt with blank camera endpoint defaults. An automated publishing review stopped the public firmware upload because it contains a device-specific binary and could expose embedded device/network metadata. The source and build instructions are available here; the firmware image can be added after you explicitly approve publishing that binary to this public repository.

Morse Micro license notices are included at the repository root.
