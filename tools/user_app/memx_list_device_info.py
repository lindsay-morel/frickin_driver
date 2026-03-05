import mxa # mxa driver
import os, sys, glob, time, struct, argparse
from pathlib import Path

def __parse():
        """
        Setup the parser (`argparse`) and parse the command line arguments.
        """

        epilog = "Examples:\n"
        epilog += "\n"
        epilog += "# List All MX3+ Device Info\r\n python memx_list_device_info.py \r\n"
        epilog += "\n"

        parser = argparse.ArgumentParser(
                 description = "\033[34mUser Application Tools\033[0m",
                 formatter_class = argparse.RawDescriptionHelpFormatter,
                 epilog=epilog)

        cmd_args = parser.parse_args()

        return cmd_args


def collect_device_info(index):
    fw_commit = mxa.get_fw_commit(index)
    date_code = mxa.get_date_code(index)
    reset_cnt = mxa.get_cold_warm_reboot_count(index)
    warn_cnt = mxa.get_warm_reboot_count(index)
    manufacturerID = mxa.get_manufacturer_id(index)
    kdriver_version = mxa.get_kdriver_version(index)

    module_info = mxa.get_module_info(index)
    chip_ver = module_info & 0xF
    chip_str = "A1" if chip_ver == 5 else "A0"

    boot_mode = (module_info >> 32) & 0xF
    boot_map = {0: "QSPI", 1: "USB", 2: "PCIe", 3: "UART"}
    chip_str += " " + boot_map.get(boot_mode, "XXXX")

    interface_info = mxa.get_interface_info(index)
    hif_type = interface_info & 0xF
    inf_map = {1: "USB", 2: "PCIe"}
    inf_str = inf_map.get(hif_type, "XXXX")

    return {
        "id": index,
        "fw_commit": fw_commit,
        "date_code": date_code,
        "reset_cnt": reset_cnt,
        "warn_cnt": warn_cnt,
        "kdriver_version": kdriver_version,
        "chip_str": chip_str,
        "interface_str": inf_str,
        "manufacturer_id": manufacturerID,
    }


def main(cmd_arg):
    result = 0
    device_count = mxa.get_device_count()

    print('Total Devices: {}\n'.format(device_count))

    header = (
        f"{'ID':<2} | {'FW Commit':<10} | {'Date Code':<10} | {'Reset Count':<11} | "
        f"{'Warm Count':<11} | {'KDriver Ver':<12} | {'Module Info':<12} | "
        f"{'Interface':<10} | {'ManufacturerID':<14}"
    )
    print(header)
    print("-" * len(header))

    for i in range(device_count):
        info = collect_device_info(i)
        print(
            f"{info['id']:<2} | {hex(info['fw_commit'])[2:]:<10} | {hex(info['date_code'])[2:]:<10} | "
            f"{hex(info['reset_cnt'])[2:]:<11} | {hex(info['warn_cnt'])[2:]:<11} | "
            f"{info['kdriver_version']:<12} | {info['chip_str']:<12} | "
            f"{info['interface_str']:<10} | {hex(info['manufacturer_id'])[2:]:<14}"
        )
        print("-" * len(header))

    return result


if __name__=="__main__":
    cmd_arg = __parse()
    result = main(cmd_arg)

    sys.exit(result)
