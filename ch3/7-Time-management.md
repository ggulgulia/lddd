# Linux Kernel Time Management
* After memory, time is one of the most used resources in computer systems
* It is used for almost everything:
    - timer, sleep, scheduling, and many more
* Linux Kernel includes software timer concepts to enable kernel functions to be invoked at a later time.

## Clock source, clockevent and tick device
* In linux kernl, the timer system was programmed to fire interrupts periodically at **HZ**  frequency (more on **HZ** bleow).
* The corresponding preiod is called **jiffy** (more on **jiffy** blow).s
* Each of the interrupts (think of interrupt as an event) generated is called a **tick** and is generated in $\frac{1}{HZ} seconds$.
* The system time management in linux kernel in eralier days was bound to `jiffies` which is a global variable in kernel.
    - the jiffies values is incremented based on the subsequent *ticks*
* Additionally, a tick handler was responsible for process scheduling, statistic updating and profiling

### `jiffy`, tick  and HZ explained

#### Linux Kernel Terminology: Jiffies
* **jiffy** : In Linux kernel terminology, a **jiffy is the unit of time between two consecutive system timer interrupts (ticks)**. 
    - It acts as the fundamental "heartbeat" of the software clock maintained by the kernel. 
* **tick**: In kernel hardware timer interrupt firing event is called tick. 
* The time elapsed beween  

#### 1. The Relationship with `HZ`
* Concretley `**HZ**` in linux kernel is a compile time constant defined in the kernel configuration file. 
* The actual duration of a jiffy depends entirely on a compile-time constant called **`HZ`**, which dictates the frequency of the hardware timer interrupts per second. 
* **Calculation:** The duration of a single jiffy is exactly $\frac{1}{HZ}$ seconds.
    - For eg if HZ is 1000, then $jiffy = \frac{1}{1000} = 0.001s = 1 milliseconds$
* **Variability:** The `HZ` value varies depending on the hardware architecture and kernel configuration. Historically, it was set to 100 on x86 architectures (making a jiffy **10 milliseconds**). In later kernels, it was increased to 1000 (**1 millisecond**), and modern x86 kernels often default to 250 (**4 milliseconds**) to balance scheduling responsiveness with performance overhead.
* On a running linux machine, the HZ value can be observed by the following:
```sh
grep 'CONFIG_HZ=' /boot/config-$(uname -r)
``` 

#### 2. The `jiffies` Counter
* The kernel maintains a **global variable** named **`jiffies`** (declared in `<linux/jiffies.h>`). 
* Every time a hardware timer interrupt fires, the kernel increments this counter by 1.
* The variable essentially stores the total number of clock ticks that have elapsed since the system booted. 

#### 3. Data Type and the 64-bit Overlay
* Because `jiffies` increments constantly, a standard 32-bit integer would overflow (wrap around to zero) relatively quickly (in about 49.7 days if `HZ=1000`). s
    - To prevent this, the kernel implements a clever architecture:
* It declares a 64-bit variable named `jiffies_64`.
* On 32-bit systems, the linker script overlays the standard `unsigned long jiffies` directly on top of the lower 32 bits of `jiffies_64`. 
* Most kernel code only checks time differences over short periods and reads the fast 32-bit `jiffies` variable, while timekeeping subsystems use the full `jiffies_64` to avoid long-term overflow.

#### 4. Modern Context: `NO_HZ` (Tickless Kernel)
* In older Linux kernels, the timer interrupt would fire ruthlessly at the `HZ` rate, even if the CPU was completely idle, wasting energy. 
* Modern Linux configurations use a mechanism called `NO_HZ` (a tickless kernel). 
* When a CPU goes idle, the kernel dynamically suppresses the periodic timer interrupt, waking up only when an actual event or scheduled timer requires attention. 
* This dramatically saves power on modern processors and mobile devices.

## hrtimers or High Resolution Timers
* Because `jiffies` are not accurate down to micro or noano sceconds with chip advancement and procssor evolution, hrtimers were introduced
    - this was done in backward compatible manner to not break the existing APIs
    - hrtimers can meaure time interval with nanoseconds accuracy (since processor speeds now clock to several GHz)

* Starting from kernel version 2.6.21, as first improvement, high resolution or *hrtimers*  implementation was merged.
    - Can be configured through `CONFIG_HIGH_RES_TIMERS`
    - On currently running linux machine, this can be examined by
    ```sh
    cat /boot/config-`uname -r` | grep "CONFIG_HIGH_RES_TIMERS"
    ```
* As a result of hrtimersand its transparent (or backward compatible integration) there are following points and advantages of hrtimer to keep in mind:
    -  the existing user space applications keep working as its, but with better time accuracy.
    - **New data type `ktime_t`** holding time value in nanoseconds. 
    - **Old time implementation stayed**: the tick-based, low-res timer (jiffy) still exists.
* The follwoing API's were converted to use hrtimers
    - `nanosleep()`
    - interval timers (`itimers`, `settimer()`)
    - POSIX Timers: `time_create()` and firends
    - **`nanosleep()` and `clock_nanosleep()` were unified**. This became possible after `nanosleep()` and the POSIX timers were both converted to hrtimers.
    - **Without hrtimers**, the best accuracy for a timer event would be **one jiffy**, and the length of a jiffy depends on `HZ`, the kernel's tick rate.

* A timer that can only fire on a tick can be late by up to one jiffy. An hrtimer is programmed to fire at the exact requested time, limited by the hardware timer.
* For eg `msleep()` is built on jiffies whereas `usleep_range()` is built on hrtimers. 
    - this is the reason whay `usleep_range()` is better choice over `msleep()` as it provides better granularity
### Low-res vs high-res timers

| | Low-resolution (jiffy-based) | High-resolution (hrtimer) |
|---|---|---|
| Time unit | Jiffies (ticks) | Nanoseconds (`ktime_t`) |
| Accuracy | About one jiffy | Limited by the hardware timer, far better than a jiffy |
| Needs `CONFIG_HIGH_RES_TIMERS`? | No | Yes |
| Typical kernel users | Timeouts, `msleep()` | `usleep_range()`, `hrtimer_start()`, POSIX timers, `nanosleep()` |

### Check on running linux machine machine

```bash
grep -E 'CONFIG_HZ=|CONFIG_HIGH_RES_TIMERS' /boot/config-$(uname -r)   # 
sudo grep -i resolution /proc/timer_list | head    # "1 nsecs" means high-res mode is active
## or 
sudo cat  /proc/timer_list | head
```
## Clocksource framework and clock source devices
* A **clock source** is monotonic, atomic and free-running counter.
* This can be considered as a timer that acts as a free running counter that provides time stamping and read access to monotonically increasing time value.
* In simple words : A clocksource is a free-running hardware counter that the kernel reads to answer “how much time has passed?"

* The most common operation performed on clock source devices is reading the counter's value
* In kernel, there is a `clocksource_list` , a global ist that tracks the clock source deivces registered with the system
    - they are enqueued ordered by rating
    - this allows the linux kernel to know about all registered clock source deivce
    - also this allows kernel to switch to the best clock source with better ratings and features
* **How the selection works**
    - A clock source needs to be registered with kernel by invoking `clock_source_mmio_init()` or `clocksource_register_hz()` 
    - The function `__clocksource_select()` function is invoked after each registration of a new clock source
    - this ensures that the best clock source is always selected .

* Commands to explore clocksource
    - Check available clock source
    ```sh
    cat /sys/devices/system/clocksource/clocksource0/available_clocksource
    ```
    - Check current clock source
    ```sh
    cat /sys/devices/system/clocksource/clocksource0/current_clocksource
    ```
    - Check clocksource message from bool
    ```sh
    sudo dmesg | grep clocksource
    ```
    - To change the current clock source (caution), the following command can be executed
    ```sh
    echo acpi_pm > /sys/devices/system/clocksource/clocksource0/current_clocksource  # run with caution and only on a VM
    ```
###  Sample output from `dmesg | grep clocksource` explained
* Following sample output was observed on my machine from the boot logs for clock source  
![clock-source-slection during boot](../00-media/ch3/00-clocksource-in-running-machine.png)  

* In the first line of the output, it can be seen that the system registers `refined-jiffies`
    - This is available very early on during bootup
    - can be used as as a fallback clock source.
* In the second line it can be seen that the `tsc-early` clock source is registered. 
    - This is the CPUs time stamp counter is registered
    - has an early rough calibration (what does this mean ?)
* In third line the `jiffies` clock source is registered. 
    - This is the lowest quality tick based fallback.
* In the fourth line the boot logs say `Switched to clocksource tsc-early`
    - this happened becuase of all the three clock source `tsc-early` is the best one so far
* In 5th line acpi_pm or the ACPI Power managment timer was registerd.
    - because it is slower to read than `tsc-early`, it is not chosen 
* Next `tsc` was registerd
    - this has higher rating thatn `tsc-early`
* In the last line, the kernel switched from `tsc-early` to `tsc` and stayed on this clock source
    - this can be verified by `cat /sys/devices/system/clocksource/clocksource0/current_clocksource`



# Appendix

# How kernel maintains reliable jiffy count
* Imagine a scenario 
    - when the interrupts do not always perfectly arrive on time, 
    - or when the interrupts are disabled.
then the jiffy increment value will not correspond to the real time difference between the two ticks (interrupt or event).
* This will cause the jiffy value to drift and lag behind the actual timings measured by the system and will eventaully cause severe errors in scheduling tasks
* To overcome this problem the kernel forece a jiffy to mean a fixed time interval anyhow.

* Therefore if a *timer interrupt*  is delayed due to high system load or disabled interrupt
    - then the jiffy value doesn't blindly increment by 1 when the interrupt finally arrives
    - the kernel detects how much real time actually passed and "catches up" by incrementing the `jiffies` counter by the correct number of *missed* ticks.
## Mechanism of handling jiffy mean value when interrupt delay happens
### 1. Interrupt latency and jitter
* In a perfect world, if `Hz=1000`m an interrupt should arrive exactly every 1.00 millseconds.
* In reality interrupt latency occurs.
* If CPU is executing critical code section with interrupts disabled or if it is busy handling a higher priority hardware interrupt, the timer interrupt is forced to wait.
* This variation in arrival time is called **jitter**.

### How the kernel handles delayed ticks
* If jiffy counter depended purely on the event of the intrrupt firing, a delayed interrupt would warp the system time, making the system clock run slow (what exactly is system clock and what is it's relation to jiffy ?)
* To prevent this, modern Linux Kernels decouple the software `jiffies` increment from raw hardwarae interrupt event using a high precision reference.

    - **Hardware reference**
        - Kernel tracks time using a highly accurate, continuous counter that never gets blocked
        - For e.g. CPU's **TSC** aka **Time Stamp Counter**.
        - or **HPET** aka **High Precision Event Timer**.
    - **Math check**: When the timer interrupt finally arrives, event if it is late the kernel checks at the continuous hardware counter to see how much real time has actually elapsed since the last time jiffies were updated 
    - **Catch up mechanism**
        - If the interrupt is only slightly delayed (for e.g 1.1ms instead of 1.0ms), `jiffies` increments by 1.
            - the slight error is accumulted and compensated for during the next subsequent cycles.
        - If the interrupt is signifcanly delayed and **multiple tick intervals have passed** (e.g. 3.4ms) because CPU was locked up
            - the `jiffies` is incremented by 3 in a single update to resynchronize the software clock to reality


# Concrete examples of interrupts
* Concrete hardware interrupts that trigger the scheduling tick and upate the `jiffies`value depend entirely on the systems hardware architecture. 
* In linux kernel, these hardware timer components are abstracted by a susystem called **Clock Event Devices**
* The speicfic hardware devices and their actual hardware interrupt lines (IRQs) that act as a physical heartbeat for the kernel is described blow

## On Modern multicore X86 System (Intel and AMD for eg)
* On such systems the kernel relies on the per CPU in-built *Local APIC* timer or *Local Advanced Programmable Interrupt Controller* timer
* The interrupts from the APIC Timer is called **`LOC`** 
* **How it works**
    - Instead of one single cip bomarding all CPUs with `LOC`(?), each core handles its own `LOC` interrupt locally
    - One primary CPU core is selected by the kernel via `tick_do_timer_cpu` variable to take the lead for global time keeping
        - i.e eventhough every CPU receives its own APIC to handle its own local task, only once CPU core can update the global clock.
    - when its specific local APIC interrupt fires, it executes `do_timer(1)` to increment the global `jiffies` counter
* The **High Precision Event Timer** alternative:
    - If local APIC timers are unavailable or unstable t he motherboards HPET hardware chip triggers the primary system interrupt.

## On leagcy or Early Boot x86 Systems
* Before the advent of multicore processors, systems used a single centralized tiemr chip
* This was the Interrupt or **`IRQ 0`** fired by **Programmable Interval Timer** or **PIT** 
* **How it works**:
    - The PIT (specifically the Intel 8254 chip or its modern emulated equivalent) is a motherboard timer connected directly to physcial interrupt line 0 (`IRQ 0`)
    - It sends electrical pulse to CPU at the exact rate defined by the **`HZ`** compile time constant in the kernel

## On ARM/ARM64 Systems (Raspberry Pi, Apple Silicon, Smartphones)
* ARM architectures handles timekeeping much more uniformly than x86 via the **ARM Architected Timer** or (**Generic Timer**) interrupt
* **How it works**
    - ARM speicfication requires a built-in timer block inside every CPU Core.
    - These send per-core *perripheral interrupts* (**PPI**, usually mapped to hardware IRQ 27 or 30).
    - Just like in the x86 APIC, the per core interrupts derive the scheduling clock and one core is elected to increment the `jiffies`

# Linux System Architecture Notes: Idle Hardware Interrupts
Why does the interrupt come at all? Or differently asked, what can cause an interrupt, even when for example the Linux PC is left completely untouched, all browsers, windows, and applications closed?

## Detailed Explanation
Even if your Linux PC is sitting completely untouched with a blank screen, the mouse unplugged, and all user applications closed, **the CPU is still furiously busy under the hood.** 
* An operating system like Linux never actually stops running. 
* Hardware interrupts arrive constantly to keep the machine alive, stable, and ready to react. 
* Concrete reasons why interrupts keep firing on a completely idle are discussed below.

### 1. The Kernel's Internal Heartbeat (The System Timer)
As we discussed with jiffies, the hardware timer chip (like the Local APIC or HPET) is programmed at a hardware level to fire at regular intervals. 

* **Why it fires when idle:** The kernel needs to know if any background tasks, disk maintenance routines, or network timeouts have expired. 
* **The "Idling" Scheduler:** The Linux scheduler must periodically wake up to check if it needs to adjust CPU power states or run kernel threads. 
* *Note on modern systems:* If your kernel has `CONFIG_NO_HZ_IDLE` enabled, it will significantly slow down these timer interrupts when idle to save power, but it will never shut them off completely.

### 2. Network Housekeeping (Even without a Browser open)
Your network card (Ethernet or Wi-Fi) is an independent computer on your motherboard. It generates interrupts constantly because the physical network is never silent:

* **Background Network Traffic:** Other devices on your local network are constantly broadcasting packets (like ARP requests asking "who has this IP address?", router advertisements, or MDNS/Bonjour discovery pings from smart TVs and printers).
* **The Interrupt:** Every time a stray packet hits your Wi-Fi card, the card copies it to RAM and throws a hardware interrupt to tell the CPU: *"Hey, someone sent data, inspect it to see if it's for us."* The Linux kernel processes it, realizes it's just background noise, drops it, and goes back to sleep.

### 3. Hardware Thermal and Power Management
Modern CPUs dynamically adjust their speed, voltage, and power intake millisecond by millisecond.

* **Thermal Interrupts:** 
    - Internal sensors inside the Intel/AMD or ARM processor constantly monitor temperatures. 
    - If a core hits a specific threshold or finishes dropping into a deep sleep state (C-states), it triggers an internal interrupt to adjust its own power configuration.
* **Motherboard Management:** 
    - The ACPI (Advanced Configuration and Power Interface) subsystem occasionally signals the CPU via interrupts to report battery status, power supply conditions, or fan speeds.

### 4. Background Storage Operations (SSD/HDD)
Even if you aren't saving a file, your storage controllers (NVMe or SATA) interact with the CPU via interrupts:

* **Journaling File Systems:** Linux file systems like `ext4` or `Btrfs` periodically flush internal memory caches and commit journal logs to the disk to ensure that data isn't lost if the power suddenly cuts out. 
* **Virtual Memory (Swap):** The kernel's memory management subsystem may choose an idle moment to clean up RAM pages or move unneeded background memory to swap space. When the drive finishes these tasks, it fires an interrupt to signal completion.

### 5. USB Controller Polling
Even if you don't touch your mouse or keyboard, your USB host controller chip on the motherboard is continuously monitoring the USB bus. It generates background interrupts to maintain its state machine, verify that devices are still connected, and ensure it can wake the PC up instantly the exact millisecond you do finally touch a key.

### See It Happening Right Now
You can actually watch this idle activity in real time. If you open a terminal and run the `watch` command, it will highlight which interrupts are ticking up second-by-second while you leave the computer alone:

```bash
watch -n 1 "cat /proc/interrupts"
```

*(Look closely at `LOC` for the timer, `NMI` for power/thermal management, and your network interface name like `wlan0` or `eth0` to see the invisible background chaos.)*
