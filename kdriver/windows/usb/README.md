# Windows driver for Memryx AI chip


## Enviroment setup 
1. Install Microsoft Visual Studio Enterprise 2022 V 17.3.0.
2. Import memryx.vsconfig to install all dependency to Microsoft Visual Studio.
3. Install Windows driver kit.[https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk]

## Kernel Driver project:
- kdriver/windows/kdriver.sln

### User Driver project 
- udriver/udriver.sln

## Example project
- examples/object_detection/ssd_mobilenet_v1_coco_camera_windows/ssd_mobilenet_v1_coco_camera.sln

# Usage 
## Install driver 
1. Open and build kdriver project.
2. Add kdriver/windows/build/memryx.cert to root certification.
3. Install kdriver/windows/build/memryx.cert/memryx.inf for Cascade Single.  
- devcon update memryx.inf "USB\VID_0559&PID_4006"
4. Install kdriver/windows/build/memryx.cert/memryx.inf for Cascade. 
- devcon update memryx.inf "USB\VID_0559&PID_4007"
- devcon update memryx.inf "USB\VID_0559&PID_4008"

## Example app
1. Open and build ssd_mobilenet_v1_coco_camera project.
2. Execute examples/object_detection/ssd_mobilenet_v1_coco_camera_windows/build/ssd_mobilenet_v1_coco_camera.exe

## Add udirver to your project.
1. Copy udriver/build/udriver.dll, udriver/build/udriver.lib and memx.h to your project.
2. Add linker option and path of udriver.lib.
3. Add inlcude path of memx.h.
