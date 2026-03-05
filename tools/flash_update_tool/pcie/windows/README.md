# MemryX Firmware Update Tool on Windows

This application updates the firmware of MemryX devices.

---

## How to Use

You can update the firmware using one of the following methods:

### Option 1: Run the Executable Directly

```bash
.\Release\pcieupdateflash_win.exe -f <path>\cascade.bin
````

* Replace `<path>` with the full path to your firmware binary file (e.g., `C:\firmware\cascade.bin`).

---

### Option 2: Run the Batch Script

```bash
.\tool\update_flash_tool\update_flash.bat
```

This script performs the following:

* **Firmware Source:** Automatically loads firmware from:

  ```
  %SystemRoot%\System32\drivers\cascade_4chips_flash.bin
  ```
* **Logging:** Writes detailed output to log file:

  ```
  ..\..\flash_update.log
  ```

---

## Return Codes

| Code | Description                    |
| ---- | ------------------------------ |
| `0`  | Firmware update successful     |
| `5`  | Timeout during update process  |
| `6`  | No MemryX device found         |
| `14`  | Firmware is already up to date |
| `> 0` | Update failed with error       |

> For troubleshooting, refer to the `flash_update.log` file.

---

## Notes

* Administrator privilege may be required.
* If multiple devices are connected, the tool will update each one individually.

