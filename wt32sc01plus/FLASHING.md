# Flashing a release

Each release contains four files that must all be flashed together, at
fixed offsets (this partition table reserves two OTA app slots, so the
board also needs `boot_app0.bin` to know which one to boot):

| File | Flash offset |
|------|--------------|
| `bootloader.bin` | `0x0` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xe000` |
| `firmware.bin` | `0x10000` |

## Using esptool

1. Install [esptool](https://github.com/espressif/esptool): `pip install esptool`.
2. Put the WT32-SC01 Plus into bootloader mode (hold BOOT, tap RESET, release
   BOOT - exact buttons depend on your board revision) and note its serial
   port (e.g. `COM5` on Windows, `/dev/ttyUSB0` on Linux).
3. From the folder the four files were extracted into:

   ```
   esptool.py --chip esp32s3 --port <PORT> --baud 460800 write_flash \
     0x0 bootloader.bin \
     0x8000 partitions.bin \
     0xe000 boot_app0.bin \
     0x10000 firmware.bin
   ```

## Using PlatformIO instead

If you have the source checked out and PlatformIO installed, `pio run -t
upload` flashes all of this automatically at the right offsets - no need to
handle the four files by hand.

## Wi-Fi setup

After flashing, the device opens a **RFIDwisp-Setup** Wi-Fi access point on
first boot. See the main README ("First boot / Wi-Fi setup") for the
captive-portal steps.
