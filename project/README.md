# CrowPanel HaLow Camera HMI

ESP-IDF 5.5.6 / LVGL project for CrowPanel Advance 5-inch ESP32-S3, 800x480, hardware V1.0.

## What is included

- Camera screen for three XIAO OV3660 MJPEG camera endpoints; one feed is selected and shown at a time.
- Camera picker, network selector (Auto / Wi-Fi / HaLow), connection status, settings page, and retry/reconnect controls.
- WPA3-SAE profile named `hallow-server`; SSID and password are entered at runtime and are never embedded in source.
- Native ESP-IDF RGB display initialization using the V1.0 pin table and touch input for the GT911-compatible 0x5D controller.
- A transport boundary for the Elecrow MM6108 HaLow vendor driver.

## Hardware facts and limits

The board target here is explicitly the **V1.0 ESP32-S3** board. Elecrow's V1.0 materials identify an ST7262 800x480 RGB display, touch I2C at 0x5D, and RGB pins in `main/board_pins.h`. The HaLow radio is the DAC00106D/MM6108. Based on the V1.0 Eagle schematic review, SPI candidates are MOSI=6, MISO=4, SCLK=5; GPIO2 is a CS candidate through the board's function selection. The schematic and software revisions disagree in some public examples (for example HSPI_SS=19 appears in the repo README). `HALOW_PIN_CS` and interrupt/reset/wakeup are therefore deliberately unassigned in the default adapter. Confirm the physical switch setting and actual module connector continuity before enabling a vendor driver.

GPIO53/54 are invalid for ESP32-S3 and are not used. GPIO7/8/9/10 are assigned to the RGB display and GPIO8 also appears as buzzer in V1.0 documentation. GPIO4/5/6 are shared with SD/audio through board selection circuitry. HaLow and those shared functions cannot be assumed to work simultaneously. Set the board function selector to wireless-module mode as documented by Elecrow.

Elecrow documents its original HaLow setup with ESP-IDF 5.1.1. This project targets ESP-IDF 5.5.6 based on the newer Morse Micro driver's declared SDK range. The HaLow adapter uses Morse Micro 2.11.2-esp32-2 and starts the radio when you scan or connect. Auto uses normal Wi-Fi. Display timing, touch, and HaLow still need validation on the physical board. `preview.svg` is a static layout preview.

The camera dashboard offers 320 × 240 (QVGA, the default) and 640 × 480 (VGA). The selection applies to the active camera and carries over when switching cameras. Tap the camera preview to open the full-screen view, then tap anywhere to return. The full-screen view reserves a centered area at the selected resolution, with no scaling or cropping planned for playback. In `main/app_config.h`, set each QVGA and VGA URL to an HTTP MJPEG endpoint that serves that resolution. The selector chooses an endpoint. The camera firmware must serve frames at the configured resolution. An empty URL appears as missing in the HMI. Most HaLow modules expose an IP interface; this UI expects the cameras and CrowPanel to be routable over the chosen network. JPEG decoding and rendering of arbitrary MJPEG streams is not implemented in this starter; the feed area is a ready status/preview panel and the stream transport hook is marked for integration.

## Build and flash

Install **ESP-IDF v5.5.6** alongside your other SDKs in Espressif Installation Manager (EIM). With the EIM CLI, run the following from the project directory to open a shell for this version:

```sh
eim install -i v5.5.6
eim shell v5.5.6
```

If you use EIM's graphical installer, install v5.5.6 there and open a terminal for that installation. In the activated terminal, run:

```sh
idf.py --version
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
idf.py -p PORT flash monitor
```

LVGL 9.2.x is fetched through the ESP Component Manager. First configure/build requires registry access. For a reproducible project, keep `dependencies.lock` after the first successful configure.

The firmware was built with ESP-IDF v5.5.6 and LVGL 9.2.2. It uses a 2 MiB factory partition from `partitions.csv`. If EIM picks an older Xtensa compiler from its shared tools directory, put the v5.5.6 compiler (`esp-14.2.0_20260121`) first on the shell's PATH and configure in a fresh build directory.

Open Settings to choose Auto, Wi-Fi, or HaLow, enter credentials, and reconnect. Settings also shows the selected network mode and connection status. Scan Wi-Fi lists nearby networks and their RSSI. Tap a result to fill the SSID field. Scan HaLow lists nearby 802.11ah networks and their RSSI. Set the physical selector to WM with power off before trying HaLow. The build uses the Canadian channel plan. Adjust the country code in the HaLow defaults for deployments elsewhere. Credentials are held in RAM only in this starter; add an encrypted NVS/secure provisioning implementation before expecting credentials to persist across reboot.

## Integration hooks

- `halow_adapter.c`: Morse Micro driver integration for scan, station connection, and RSSI.
- `camera_stream.c`: provide HTTP MJPEG multipart parsing and JPEG decode (ESP JPEG decoder) into an LVGL image buffer. Only one stream should be active at a time to limit memory and HaLow airtime.
- Board selector: set the physical S1/S0 wireless-module route before boot.

The top clock uses 12-hour local time with AM and PM. It restores valid time from the onboard BM8563 real-time clock and saves the time there after a successful network sync. It shows `--:--` if no valid time is available. The firmware starts network time sync when the board gets an IP address and accepts a time server supplied by DHCP. The default time zone is Toronto. On an isolated network, use DHCP to advertise a local time server or set `CLOCK_NTP_SERVER` in `main/app_config.h`.

The home bar shows Wi-Fi association (green when connected, gray otherwise), and HaLow link state (amber when unavailable).

Settings shows HaLow link quality as RSSI in dBm when connected and a sample is available. The MM6108 adapter caches the reading for the current association. Settings shows an unavailable state if the radio fails to initialize.

### HaLow driver references

The [V1.0 Eagle schematic](https://github.com/Elecrow-RD/CrowPanel-Advance-5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480/blob/master/Eagle_SCH%26PCB/Version%201.0/ESP32%20Display%205.0%20inch%20V1.0.sch) identifies the host-side wireless connector routing. In wireless mode, J9 pins 2/3/4 route to GPIO5/4/6 for SPI clock/MISO/MOSI, and J11 pin 5 routes to GPIO2 on the wireless CS net. J11 pin 1 and J9 pin 1 reach GPIO19 and GPIO20 through R48 and R49, respectively. J11 pin 6 is the GPIO0 boot/BUSY net. These are host connector assignments. The installed module's mating header must be traced to confirm MM6108 IRQ, reset, wake, and BUSY and to identify its board configuration file (BCF).

The [Elecrow scan example](https://github.com/Elecrow-RD/Wireless-Module-for-Wi-Fi-HaLow/tree/master/example_code/mm-iot-esp32/examples/scan) uses `mmwlan_scan_request()` after booting the transceiver and selecting its channel list. Its [shim defaults](https://github.com/Elecrow-RD/Wireless-Module-for-Wi-Fi-HaLow/blob/master/example_code/mm-iot-esp32/framework/mm_shims/Kconfig) use GPIO7/9/8 for SPI clock/MOSI/MISO. Those pins conflict with the V1.0 RGB display, so use the CrowPanel-specific wiring when integrating the scanner.

Figure 8 of the [DAC00106D datasheet](https://www.elecrow.com/download/product/DAC00106D/Wireless_Module_for_Wi-Fi_HaLow_DataSheet.pdf) shows the module header from below. Aligning it with the V1.0 PCB footprint by CLK/MISO/MOSI and the power pins suggests the following mapping. Confirm the orientation and continuity on the actual assembly before using the inferred control assignments.

| Module signal | Panel header | V1.0 host route |
| --- | --- | --- |
| CLK / MISO / MOSI | J9 pins 2 / 3 / 4 | GPIO5 / GPIO4 / GPIO6 |
| CS | J9 pin 1 | GPIO20 through R49 and the wireless mux |
| INT | J11 pin 1 | GPIO19 through R48 and the wireless mux |
| BUSY | J11 pin 4 | No net shown in the panel schematic |
| RESET_N | J11 pin 5 | GPIO2 through the wireless mux, labeled `IO2_W_CS` on the panel |
| WAKEUP_IN | J11 pin 6 | GPIO0, also used for BOOT |

The panel net names reflect the carrier and do not identify the function of every mating module pad. BUSY is disconnected on this assembly. For Morse Micro 2.11.2-esp32-2, leave `CONFIG_MM_BUSY=-1` and `CONFIG_HALOW_PS_MODE` disabled. The [driver initialization](https://github.com/MorseMicro/esp-halow/blob/2.11.2-esp32-2/halow/mmhalow.c) disables transceiver power saving after boot in this configuration. When using the low-level scan example directly, call `mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED)` after a successful `mmwlan_boot()` and before requesting a scan. The remaining bring-up work is confirming the inferred control pins and selecting the BCF for the exact radio variant.

The [Morse Micro firmware commit](https://github.com/MorseMicro/morse-firmware/tree/ea18605f53387281c1029ea9eb8e0de2bda7bae2/firmware) contains `mm6108.bin`. Its separate [Quectel BCF directory](https://github.com/MorseMicro/morse-firmware/tree/ea18605f53387281c1029ea9eb8e0de2bda7bae2/bcf/quectel) has distinct files for FGH100M variants. Elecrow identifies the radio family as FGH100M. Use the full Quectel part number or a BCF specified by Elecrow to choose the file. Use the chip firmware bundled with the selected ESP-IDF HaLow driver for initial bring-up.

The installed module is **Quectel FGH100MABMD**. Its matching [Morse Micro BCF](https://github.com/MorseMicro/morse-firmware/blob/ea18605f53387281c1029ea9eb8e0de2bda7bae2/bcf/quectel/bcf_fgh100mabmd.bin) is `bcf_fgh100mabmd.bin`. CMake loads `sdkconfig.defaults.halow` alongside the base defaults in a fresh build. The driver embeds the BCF from the project root with its bundled MM6108 firmware. The current build uses the schematic-derived control pin assignments, with BUSY and power saving disabled. The radio scan and association still need a hardware check.

Elecrow's [station connection example](https://github.com/Elecrow-RD/Wireless-Module-for-Wi-Fi-HaLow/tree/master/example_code/mm-iot-esp32/examples/sta_connect) ships with Morse Micro SDK 2.6.4-esp32. Use it as a reference for bringing up the module and connecting as a station. The upstream [Morse Micro WLAN header](https://github.com/MorseMicro/mm-iot-esp32/blob/main/framework/morselib/include/mmwlan.h) exposes `mmwlan_get_rssi()` for the associated AP. In a port, read it from the HaLow driver task, treat `INT32_MIN` as no reading, and clear the cached reading on disconnect. Settings can then display the cached value without calling the radio driver from the UI task.

The [Morse Micro registry component](https://components.espressif.com/components/morsemicro/halow/versions/2.11.2-esp32-2/dependencies?language=en) declares ESP-IDF >=5.4.4 and <6.0 for release 2.11.2-esp32-2. ESP-IDF v5.5.6 is the latest published patch in the 5.5 series checked for this project. The project manifest accepts v5.5.6 and newer 5.5 patches. Confirm the V1.0 module wiring and board configuration file before enabling the radio.
