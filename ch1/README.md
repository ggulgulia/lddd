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