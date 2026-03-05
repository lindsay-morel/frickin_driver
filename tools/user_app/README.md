# User Application Tool

Linux
- Install kernel module
- Install udriver.so
- Put Pymodule at root folder

Windows
- Install kernel driver
- Put udriver.dll at root folder
- Put Pymodule at root folder

**Please update the udriver and pymodule base on your requirement**
usage: memx_download_flash_firmware.py [-h] [-v] [-f] [-g]

# download cascade_4chips_flash.bin to device 0 (default)
 python memx_download_flash_fw

# download firmware.bin to device 1
 python memx_download_flash_fw -g 1 -f firmware.bin

# Check cascade_4chips_flash.bin information only
 python memx_download_flash_fw -d
 
usage: memx_list_device_info.py [-h]

# List All MX3+ Device Info
 python memx_list_device_info.py
