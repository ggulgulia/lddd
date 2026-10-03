# Chapter 2 notes
* Loadable modules are plugins. Usually device drivers, filesystems and frameworks are compiled as loadable modules.
* Loadable modules extend the kernel feature without needing to restart the machine
* To support module loading, the kernel must be built with following feature enabled:

```txt
CONFIG_MODULES=y
```
* To check if the current running system has modules enabled:
```sh
cat /boot/config-`uname -r` | grep CONFIG_MODULE
```
* For a module that is loadable, it makes sense to configure it to be unloadable as well. To be able to unload a module the following:
```txt
CONFIG_MODULE_UNLOAD=y
```

## Loadable Modules
* __init and __exit are kernel macros, defined in include/linux/init.h, as shown here:
```c
#define __init __section(.init.text)
#define __exit __section(.exit.text)
```

- The `__init` and `__exit` macros are direct instructions (or "hints") to the compiler and linker to isolate initialization and cleanup code into specialized, dedicated Executable and Linkable Format (ELF) memory sections.

- Instead of treating them like ordinary functions, this separation opens up a window for the Linux kernel to perform runtime memory optimizations:

* Minimum requirement for a loadable module is the initialization method called when the module is loaded (by probably `modprobe` or `insmod` )
```c
static __init module_entry_point_function(void);
```

* If a module is built as loadable module, then the exit point method should be also provided and is executed when the module is unloaded (by probably `modprobe -r` or `rmmod`)
```c
static void __exit module_exit_point_function(void);
```

* `__init` and `__exit` method are invoked only once, whatever the number of deivce currently being handled by the module (provided the module is a device driver)

* It is common for modules that are platform or device driver to register a platform driver and the associted `probe`/`remove` callback in their init function
   - The `probe` is then executed each time a device handled by the module is added to system
   - The `remove` is executed each time a device handled by the module is removed from system

* Different behavior of `__exit` sections for statically compiled and loadable module
   - For statically compiled module (`CONFIG_MODULE_UNLOAD=n`)
        - Because a built-in module can never be unloaded, its exit function will never be  called.    
        - In this specific scenario, the __exit macro takes effect by telling the compiler to entirely omit and discard that function's code to save RAM.
    - For (un)loadable moudle (`CONFIG_MODULE_UNLOAD=y`) , with `*.ko` extension
        - `__exit` has no effect—meaning it does not strip or omit any code. 
        - The exit function is kept fully intact in memory so the kernel can safely execute it when you unload the module.

### Memory layout and timeline of the __init and __exit sections
```txt
1. COMPILED BINARY FILE (.ko file on disk)
+-------------------+--------------------+------------------------+

| .text section     | .init.text section | .exit.text section     |
| (Regular Code)    | (Functions w/      | (Functions w/          |
|                   |  __init macro)     |  __exit macro)         |
+-------------------+--------------------+------------------------+


2. LIVE IN KERNEL MEMORY (Right after loading via insmod)
+-----------------------------------------------------------------+

| RAM ALLOCATION                                                  |
|  +----------------+  +-------------------+  +-----------------+ |
|  | .text section  |  | .init.text        |  | .exit.text      | |
|  | (Regular Code) |  | (init function)   |  | (exit function) | |
|  +----------------+  +-------------------+  +-----------------+ |
+-----------------------------------------------------------------+
                         |
                         |  Kernel calls the init function...
                         v  
                  Initialization Complete!


3. RUNNING MODULE STATE (Optimized Layout)


   IF BUILT-IN (=y) OR NO UNLOAD:      IF DYNAMIC LOADABLE (=m):
   +----------------+                  +----------------+  +-----------------+

   | .text section  |                  | .text section  |  | .exit.text      |
   | (Regular Code) |                  | (Regular Code) |  | (exit function) |
   +----------------+                  +----------------+  +-----------------+
   [ .init.text FREED ]                [ .init.text FREED ]
   [ .exit.text FREED ]                                   
```
* To inspect the Module information section of kernel object, use
```sh
objdump -d -j .modinfo your-kernel-file.ko
```
## Bulding a Linux Kernel Module
* Two solutions exists for building a kernel module:
    1. **Out of Tree Building**
        - Applicable when the module source code is outside of kernel soruce tree
        - In this style, the module build cannot be integrated to kernel configruation
        - Is applicable exclusively to loadable modules
    2. **Building within kernel tree**
        - Module compilation can be integrated within kernel configuration
        - Allows upstream sync within kernel source tree
        - Allows to build either statically linked or loadable kernel module
* Linux kernel maintains its own build system called `kbuild`(k is lower case)
* `kbuild`allows configruation of linux kernel and compilation based on the specific configuration
* `kbuild` relies on three files to achieve the configuration and compilation
    - `Kconfig` : for feature selction (K is upper case)
    - `Kbuild`  : for compilation
    - `Makefile`: also for compilation
### `Kbuild` or `Makefiles`
* From within the build system the makefile can be called either `Kbuild` or `Makefile`
    - If both exists, `Kbuild` will be used
* In short, makefile is a special file used to execute a set of actions, most common amongst which is program compilation
* There is a dedicated tool to parse makefile called `make`
* Using the `make` tool the kernel build command pattern resembles the following:
```sh
make -C $KERNEL_SRC M=$(shell pwd) [target]
```
where
    - `$KERNEL_SRC` refers to path of prebuilt kernel
    - `-C $KERNEL_SRC` instructs make to change to the specified directory when executing and change back when finished
    - `M=(shell pwd)` instructs kernel build system to move back to this directory to find the kernel module that is being built. Value given to `M` is the absolute path of directory where the module sources or associated `Kbuild` files are located
    - `[target]` corresponds to the subset of `make` targets available when building an external module, which can be one of the following:
        1. `modules` : default target for external module. Has same functionality as if no target was specified
        2. `modules_install`: installs external modul(s). Default location is `/lib/modules/<kernel-release>/extra`, but can be overridden
        3. `clean`: removes all generated fils in module directory only

* How to instruct build system to build or link the object file:
    - Specify the name of the modules to be  build along with the soruce files. 
    - Simple  example of object being builf from single source file
    ```
    obj-<X> := <module_name>.o
    ```

    - If more than one source (.c) file is used, for e.g. `foo.c` and `bar.c` to build module then use

    ```mk
    obj-<X> := foo.o bar.o
    ```
    - NOTE: <X> can have values 
        - `m` for loadable modules
        - `y` for static kernel modules, but outside of kernel source tree `<module-name>.o` will not be built
        - empty , in which case the `<module-name>.o` will not be build

### Out of tree building
* before a loadable external module can be built out of tree, ensure that complete and precompiled kernel source tree is avaiable
* steps to ensure the precompiled kernel source tree is available
    - run `uname -r` on your system to get the kernel version
    - check if the kernel source tree is present in the path `ls /lib/modules/$(uname -r)/build`. This should list several files and folders on the system
    - If the kernel source tree is not present, then it can be installed via 
        ```sh
        sudo apt update && sudo apt install build-essential linux-headers-$(uname -r)
        ```
* Browse to the path where the make file is present and compile the source by typing `make`
* For cross compiling for 32 or 64 bit the following command lines are needed
```
#32 bit cross compilation
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

#64 bit cross compilation
make ARCH=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
```
    - enusre the kernel source path is specified in the makefile correctly

### In tree building
* In-tree building allows module features to be exposed in cofiguration menu via `make menuconfig`
* First identify where the module will be hosted in the tree
    - for e.g consider subdir `driver/char` 
    - Considering the source file name to be `mychar/myChardev.c` containing the said module source code
* In the path where the module is hosted, place a `Kconfig` file
    - In this case the `Kconfig` is placed in `driver/char/mychar/` path in linux tree  
* Add the correct content in the content in the `Kconfig`
    - In this case the `Kconfig` should can the following

    ```txt
    config GG_MYCDEV
      tristate "GG's toy Character driver"
      default m
      help
          Say Y here to support /dev/mychar char device
    ```
    - In this `Kconfig`file the default state of the module is `m`
* Add a `Makefile`  in the same path where the module source code and the `mychar/Kconfig` is placed for the module with approrpiate build instructions
    - In this case the `Makefile` should go to `driver/char/mychar/` path as well
    - The `mychar/Makefile` in this case can be for eg a simple oneliner
    
    ```  
    obj-$(CONFIG_GG_MYCDEV) += mychardev.o
    ```

    Note: the string after `CONF_` is same as the config defined in `Kconfig` file which in this case is `GG_MYCDEV`
        - The entire config name in `Makfile` is then `CONFIG_${NAME_OF_CONFIG_IN_KCONFIG_FILE}`
        - In this case the config name is `CONFIG_GG_MYCDEV`
    - The name of the `.o` should match the source file name
    - In this case the object file in `Makefile` should be declared `myChardev.o`

* In the parent directory of the `drivers/char` the `Kconfig` and the `Makefile`also needs to be updated to link to the `drivers/char/mychar/Kconfig` and `drivers/char/mychar/Makefiles`
    - In this case the  following line should be added to `drivers/char/Kconfig`
    ```
    source "drivers/char/mychar/Kconfig"
    ```
    - And the following line should be added to `drivers/char/Makefile`:
    ```
    obj-y +=mychar
    ```
    Note the `-y` appended to `obj` above tells the parent `Makefile` to instruct to check the child folders `Makefile` configuration (in this case the value defined by variable `$(CONFIG_GG_MYCDEV)`) and apply that config

* In the specific architecture of interest, in the `config` file, the same device config above as in the module `Makefile` should be updated. 
    - In this case for arm64 architecture the `arch/arm64/configs`, the following should be added
    ```
    CONFIG_GG_MYCDEV=m
    ``` 
    to configure the module as loadable module
    - If the module is desired to be built as statically compiled module by default then the following should be added
    ```
    CONFIG_GG_MYCDEV=y
    ```
* If everything has been configured correctly the device should appear on the menuconfig once the correct menuconfig is launched
    - In this case
    ```
    make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
    ```
    - The following image shows how to navigate to the char driver in the menuconfig : 
        - Home Page of menuconfig highlighting device driver config option
        ![Home-window-of-menuconfig](/lddd/00-media/ch2/00-custom-driver-config-eg-1.png)

        - Device driver page of menuconfig highlighting char driver config option
        ![Deivce-driver-of-menuconfig](/lddd/00-media/ch2/00-custom-driver-config-eg-2.png)

        - Char driver page of menuconfig, at bottom, highlightig the toy char driver that can be configured
        ![Char-driver-of-menuconfig](/lddd/00-media/ch2/00-custom-driver-config-eg-3.png)

    - In the image the default configuration in the menu is `<*>` indicating statically compiled module  
        ![default-setting-in-menuconfig](/lddd/00-media/ch2/00-custom-driver-config-eg-4.png)

    - If on the keyboard, `m` is tapped, the driver is configured to be loadable module indicated by `<M>` in the menuconfig
        ![module-setting-in-menuconfig](/lddd/00-media/ch2/00-custom-driver-config-eg-5.png)
    - If on the keyboard `n` is tapped, the driver is not compiled at all, indicated by empty angled brackets `< >`
        ![turned-off-in-menuconfig](/lddd/00-media/ch2/00-custom-driver-config-eg-6.png)
