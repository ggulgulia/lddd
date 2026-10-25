## Inspecting clock sources and clock events on an ARM64 system

### 1. Which clock source is active?

```bash
cat /sys/devices/system/clocksource/clocksource0/current_clocksource
cat /sys/devices/system/clocksource/clocksource0/available_clocksource
dmesg | grep -i clocksource
```

Expect: `arch_sys_counter` (the ARM architected system counter), and a "Switched to clocksource arch_sys_counter" line in `dmesg`.

### 2. Which clock event devices exist?

```bash
ls /sys/devices/system/clockevents/
for d in /sys/devices/system/clockevents/clockevent*; do echo "$d: $(cat $d/current_device)"; done
```

Expect: one `clockevent<N>` per CPU plus `broadcast`. The device name is often `arch_sys_timer`, or a SoC-specific timer on some boards.

### 3. Is high-resolution mode active?

```bash
sudo grep -E "Tick Device|Clock Event Device|mode:|event_handler|resolution" /proc/timer_list | head -40
python3 -c "import time; print(time.clock_getres(time.CLOCK_MONOTONIC))"
```

Expect: `mode: ONESHOT` (or `mode: 1` on the tick device line), `event_handler: hrtimer_interrupt`, and a resolution of `1 nsecs`. The Python line should print `1e-09`. A value like `0.004` would mean a jiffy-based fallback at HZ=250.

### 4. Are timer interrupts firing on every CPU?

```bash
grep -i timer /proc/interrupts
watch -n1 "grep -i timer /proc/interrupts"
```

Expect: a line such as `arch_timer` with one counter per CPU, and the counts rising. An idle CPU in deep sleep may rise slowly.

### 5. Kernel configuration

```bash
zcat /proc/config.gz | grep -E "CONFIG_HZ=|CONFIG_HIGH_RES_TIMERS|CONFIG_ARM_ARCH_TIMER|CONFIG_TICK_ONESHOT|CONFIG_NO_HZ"
# or, if there is no /proc/config.gz:
grep -E "CONFIG_HZ=|CONFIG_HIGH_RES_TIMERS|CONFIG_ARM_ARCH_TIMER" /boot/config-$(uname -r)
```

Expect: `CONFIG_HIGH_RES_TIMERS=y`, `CONFIG_ARM_ARCH_TIMER=y`, and the board's `CONFIG_HZ` value (often 100, 250 or 1000).

### 6. Timer hardware description and boot messages

```bash
dmesg | grep -i -E "arch_timer|sched_clock|timer"
ls /proc/device-tree | grep -i timer
tr '\0' '\n' < /proc/device-tree/timer/compatible
```

Expect: a line such as "arch_timer: cp15 timer(s) running at NN MHz (phys)" giving the counter frequency, and a `timer` node whose `compatible` is `arm,armv8-timer`. On a Raspberry Pi, a node for the BCM2835 System Timer (`brcm,bcm2835-system-timer`) may also appear under the SoC node. The node paths can differ between boards.

### What to compare with x86

| | x86 (your PC) | ARM64 Cortex-A (typical) |
|---|---|---|
| Clock source | `tsc` | `arch_sys_counter` |
| Clock event device | `lapic-deadline` (per CPU) | `arch_sys_timer` (per CPU) |
| Timer interrupt in `/proc/interrupts` | `LOC` / Local timer interrupts | `arch_timer` |

### Notes

- Names such as `arch_sys_timer` depend on the board and kernel version, so look at what your system prints first.
- Don't write to `current_clocksource` on a real target. Changing it can make timekeeping slower or break timing-sensitive code. Use a VM or a spare board.
- `/proc/timer_list` may be missing on stripped-down embedded kernels, and reading it may require root.