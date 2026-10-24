# Introduction to examining kernel logs on console
* In many of the coding exercises, it is useful to check the kernel logs to verify if the logging code that was in kernel module actually executed
* this helps to verify 
    - if certain block of code executed
    - if there was an error
    - and all kinds of primitve debugging that lazy developers do using print statements in programming
* There are several ways to check the kernel logging:
    - `dmesg`  : 
    - `journalctl`: 
    - `/var/log/kern.log`: works on ubuntu if rsyslog is installed an persists across reboots
    - `watch -n1 'dmesg | tail -20`: crude but works anywhere

## The `dmesg` utility
* `dmesg` is a utility that helps to visualize several logs. For e.g
    - kernel log messages
    - user level messages
    - ... and many more
* It supports various command line options to facilitate visualize loggings for different purposes
    * All the command line options can be listed by `dmesg --help`

* The ones that will be mostly used in this course will be
    - `sudo dmesg | tail`
    - `sudo dmesg -C` : Clears the current kernel log
        - test this by first checking the existing kernel log: `sudo dmesg`
        - then clear the kernel log and again check the kernel log buffer using `sudo dmesg`
    - `sudo dmesg -w` : Follow  log messages without exiting (until Ctl+C it pressed)
    - `sudo dmesg -W` : same as above but follows only new log messages. No need to clear the old log to have a clutter free log visualization.
    - adding `| grep my_module_name` to any of the above two commands prints the logs from the module that is of interest
    * Other handy options to combine with it:
    ```sh
    sudo dmesg -wH         # human-readable timestamps, pager-friendly output
    sudo dmesg -wT         # wall-clock timestamps (approximate on long-running systems)
    sudo dmesg -w --color=always -l err,warn   # only errors and warnings
    ``` 
