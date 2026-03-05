#!/bin/bash
stty -echo
function menu {
	echo -e "\tSelect Environment"
	echo -e "\t1. (x86_64)"
	echo -e "\t2. (aarch64)"
	echo -e "\t3. (arm32)"
	read -n 1 option
}

while [ 1 ]
do
	menu
	case $option in
	2)
		cp -rf ./aarch64/pcieupdateflash ./pcieupdateflash
		cp -rf ./aarch64/read_fwver ./read_fwver
		cp -rf ./aarch64/check_version ./check_version
		echo -e "\t-->Use ./aarch64/pcieupdateflash\n\n"
		break;;
	3)
		cp -rf ./arm32/pcieupdateflash ./pcieupdateflash
		cp -rf ./arm32/read_fwver ./read_fwvwer
		cp -rf ./arm32/check_version ./check_version
		echo -e "\t-->Use ./arm32/pcieupdateflash\n\n"
		break;;
	*)
		cp -rf ./x86_64/pcieupdateflash ./pcieupdateflash
		cp -rf ./x86_64/read_fwver ./read_fwver
		cp -rf ./x86_64/check_version ./check_version
		echo -e "\t-->Use ./x86_64/pcieupdateflash\n\n"
		break;;
esac
done

stty echo
