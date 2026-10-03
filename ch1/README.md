# Chapter 1: Introduction to linux kernel development
1. check the ubuntu version. 
```sh
lsb_release --help
```

2. Print system information
```sh
uname --help
uname -a/-r/
```
3. Inspect the currently built in config in the kernel running in the system
```sh
cat /boot/config-`uname -r`
```

4. List all the kernel objects (.ko) that are built into the kernel.
```sh
cat  /lib/modules/$(uname -r)/modules.builtin
```
5. aliases for module loading utilities, which are used to match drivers and devices
```sh
cat /lib/modules/$(uname -r)/modules.alias
```
6. Running the linux configuration menu
```sh
ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make menuconfig

## Loading kernel module at runtime
* To load the kernel module (buit out of tree), exeucte the following command
```sh
sudo insmod kernel_name.ko
```
* Inspect the loaded kernel using `dmesg`
```sh
sudo dmesg | tail -n 10
```
* Unload the kernel using
```sh
sudo rmmod kernel_name #without .ko
```
    - inspect again the unloaded kernel using `dmesg`

### NOTE On Secure Boot Mode of System
* Reboot your Ubuntu machine.
* As soon as the computer starts turning back on, repeatedly press your BIOS/UEFI setup key.
    - Common keys for Intel Core i7 systems are F2, F12, or Del.
* Use the arrow keys to navigate to the Security, Boot, or Authentication tab.
* Find the option labeled Secure Boot and change it from Enabled to Disabled.
* Press F10 to save your changes and exit. Your computer will reboot into Ubuntu normally.