# Testing `printk_levels_demo`

A small kernel module that prints one message at each printk log level. Use it to practise reading and filtering kernel logs.

## Files

- `printk_levels_demo.c`: the module source
- `Makefile`: Kbuild Makefile (recipe lines must start with a tab, not spaces)

## Module parameters

| Parameter | Type | Default | Meaning |
|---|---|---|---|
| `repeat` | int | 1 | How many times to print the full set of messages |
| `emerg` | bool | 0 | Also print a `KERN_EMERG` message (off by default because some systems broadcast it to every open terminal) |

## Prerequisites

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

## Build

```bash
make
modinfo ./printk_levels_demo.ko     # shows the repeat and emerg parameters
```

## Watch the logs (use two terminals)

Terminal 1, follow the ring buffer with levels decoded:

```bash
sudo dmesg -C                       # clear the buffer so only new output shows
sudo dmesg -w -x                    # -w follows, -x shows facility:level on each line
```

Terminal 2, load and unload:

```bash
sudo insmod printk_levels_demo.ko repeat=2
sudo rmmod printk_levels_demo
```

Expected: levels 1 to 6 printed for each round. `pr_debug` (level 7) and `KERN_EMERG` (level 0) are missing at this point, for the reasons in the sections below.

## Filter by level

```bash
sudo dmesg -x | grep printk_levels_demo      # everything from this module
sudo dmesg --level=err,warn                  # only errors and warnings
journalctl -k -p err                         # kernel messages at err or more severe, via systemd
journalctl -k -f -p warning                  # follow warning and above
```

## See `pr_debug` (level 7)

`pr_debug` is compiled out by default. Enable dynamic debug at load time (needs `CONFIG_DYNAMIC_DEBUG`):

```bash
sudo rmmod printk_levels_demo 2>/dev/null
sudo insmod printk_levels_demo.ko dyndbg=+p
sudo dmesg -x | tail
```

Use `+pmf` instead of `+p` to also print the module name and function name.

## See `KERN_EMERG` (level 0)

```bash
sudo insmod printk_levels_demo.ko emerg=1
```

## Change the console log level

```bash
cat /proc/sys/kernel/printk          # four numbers, the first is console_loglevel
sudo dmesg -n 3                      # console now only shows levels 0-2
sudo dmesg -n 7                      # console shows everything again
```

A terminal emulator does not show console output, so this is only visible on a real text console (Ctrl+Alt+F3) or a serial console. `dmesg` and `journalctl -k` are unaffected, because the ring buffer keeps all levels.

## Inject a message from user space

```bash
echo "<3>hello from userspace" | sudo tee /dev/kmsg
sudo dmesg -x | tail -n 3
```

The `<3>` prefix sets the level to `err`. This is a quick way to test filters without reloading the module.

## Clean up

```bash
sudo rmmod printk_levels_demo
make clean
```

## Log level reference

| Level | Macro | `pr_*` helper | Meaning |
|---|---|---|---|
| 0 | `KERN_EMERG` | `pr_emerg()` | System is unusable |
| 1 | `KERN_ALERT` | `pr_alert()` | Action must be taken immediately |
| 2 | `KERN_CRIT` | `pr_crit()` | Critical conditions |
| 3 | `KERN_ERR` | `pr_err()` | Error conditions |
| 4 | `KERN_WARNING` | `pr_warn()` | Warning conditions |
| 5 | `KERN_NOTICE` | `pr_notice()` | Normal but significant condition |
| 6 | `KERN_INFO` | `pr_info()` | Informational |
| 7 | `KERN_DEBUG` | `pr_debug()` | Debug-level (compiled out unless `DEBUG` or dynamic debug) |

Defined in `include/linux/kern_levels.h`; the `pr_*` helpers are in `include/linux/printk.h`.