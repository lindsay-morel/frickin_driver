# memx_usb_update_flash_tool

USB update flash tool for MX3 USB module.

---

## Modes

- **`-all`**   : (USB boot) Download firmware, wait, then flash. *(default on Linux)*
- **`-flash`** : (QSPI boot) Flash only. *(default on Windows)*

---

### Linux
#### Install dependencies
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libusb-1.0-0-dev
```

#### udev rules (permissions)

Without udev permissions, non-root users often get `LIBUSB_ERROR_ACCESS`.

1. Create a **udev rule** for MX3 Plus:
   ```bash
   sudo tee /etc/udev/rules.d/50-memx_usb_update_flash_tool.rules >/dev/null <<'RULE'
   SUBSYSTEM=="usb", ATTR{idVendor}=="0559", MODE="0666", TAG+="uaccess"
   RULE
   ```

   > Notes:
   > - `TAG+="uaccess"` (systemd) grants access to the active desktop user.
   > - On older distros, use the fallback line and ensure your user is in the `plugdev` group:
   >   ```bash
   >   sudo groupadd -f plugdev
   >   sudo usermod -aG plugdev "$USER"
   >   ```

2. Reload rules and replug the device:
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   # then unplug/replug the USB device
   ```

#### Run
```bash
./memx_usb_update_flash_tool [-flash | -all] cascade.bin
```

---

### Windows
#### Install dependencies
1. Install **WinUSB driver** for your MX3 device:
   - Download [Zadig](https://zadig.akeo.ie/).
   - Select your MX3 USB device (VID `0559` / PID `4006`).
   - Install the **WinUSB** driver.
    <p align="left">
    <img src="../doc/WinUSB.png" alt="WinUSB install for MX3" width="400"/>
    </p>
#### Run
```powershell
memx_usb_update_flash_tool.exe -flash cascade.bin
```