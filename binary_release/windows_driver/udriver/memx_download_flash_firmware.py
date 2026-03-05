import mxa # mxa driver
import os, sys, glob, time, struct, argparse
from pathlib import Path

def __parse():
        """
        Setup the parser (`argparse`) and parse the command line arguments.
        """

        epilog = "Examples:\n"
        epilog += "\n"
        epilog += "# download cascade_4chips_flash.bin to device 0 (default)\r\n python memx_download_flash_fw \r\n"
        epilog += "\n"
        epilog += "# download firmware.bin to device 1\r\n python memx_download_flash_fw -g 1 -f firmware.bin \r\n"
        epilog += "\n"
        epilog += "# Check cascade_4chips_flash.bin information only\r\n python memx_download_flash_fw -d\r\n"

        parser = argparse.ArgumentParser(
                 description = "\033[34mUser Application Tools\033[0m",
                 formatter_class = argparse.RawDescriptionHelpFormatter,
                 epilog=epilog)

        control = parser.add_argument_group("Control")
        #-- Control -----------------------------------------------------------
        control.add_argument("-f",
                             dest    =   "file_path",
                             action  =   "store",
                             type    =   str,
                             default =   "cascade_4chips_flash.bin",
                             metavar =   "",
                             help    =   "the path for firmware file")

        control.add_argument("-g",
                             dest    =   "device_group",
                             action  =   "store",
                             type    =   int,
                             default =   0,
                             metavar =   "",
                             help    =   "the device group index for running")

        control.add_argument("-d",
                            dest    = "dry_run",
                            action  =  "store_true",
                            default =  False,
                            help    = "Parse Firmware Image Only")

        cmd_args = parser.parse_args()

        return cmd_args

def image_parse(file_path):
    with open(file_path, 'rb') as file:
        data = bytes(file.read())
        img_fmt  = int.from_bytes(data[0x6F08:0x6F0C], byteorder='little')
        git_version = int.from_bytes(data[0x6F0C:0x6F10], byteorder='little')
        fw_size     = int.from_bytes(data[0x7000:0x7004], byteorder='little')
        crc_offset  = 0x7000+fw_size+4
        crc_total   = int.from_bytes(data[crc_offset:crc_offset+4], byteorder='little')

    print('img_fmt {} fw_size {} git_version {} crc_total {}'.format(img_fmt, fw_size, hex(git_version), hex(crc_total)))

    return hex(git_version)


def main(cmd_arg):
    result = 0
    path_str  	 = str(Path(cmd_arg.file_path))
    device_group = cmd_arg.device_group
    check        = cmd_arg.dry_run

    if check == True:
        if (Path.is_file(Path(cmd_arg.file_path)) == False):
            print('Firmware path {} is not existed'.format(path_str))
        else:
            image_parse(path_str)
            print('Check Firmware Image {} Done'.format(path_str))
    else:
        if (Path.is_file(Path(cmd_arg.file_path)) == False):
            print('Firmware path {} is not existed'.format(path_str))
        else:
            image_parse(path_str)
            result = mxa.update_firmware(device_group, path_str, 0)

        if result != 0:
            print('Download firmware {} to device group {} failed'.format(path_str, device_group))
        else:
            print('Download firmware {} to device group {} success'.format(path_str, device_group))

    return result


if __name__=="__main__":
    cmd_arg = __parse()
    result = main(cmd_arg)

    sys.exit(result)
