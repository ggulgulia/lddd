## Experiments: watching softirqs on my PC

Softirq counters in `/proc/softirqs` are counts of handler runs per CPU since boot. These experiments make them move on purpose, so each row's meaning becomes visible.

### 0. Helper: show how much each counter changed

Needs `bash` (it will not work in plain `sh`). Put it in a file such as `~/softirq_delta.sh`:

```bash
softirq_delta() {
  cat /proc/softirqs > /tmp/sirq_before
  "$@"                                   # the command to measure
  cat /proc/softirqs > /tmp/sirq_after
  awk 'NR==FNR { if (FNR>1) for (i=2;i<=NF;i++) b[FNR,i]=$i; next }
       FNR==1  { printf "%-10s", ""; for (i=1;i<=NF;i++) printf "%10s", $i; printf "\n"; next }
               { printf "%-10s", $1; for (i=2;i<=NF;i++) printf "%10d", $i-b[FNR,i]; printf "\n" }' \
      /tmp/sirq_before /tmp/sirq_after
}
```

Use it as `source ~/softirq_delta.sh` and then `softirq_delta <command>`. It prints how many times each softirq ran on each CPU while the command ran.

### 1. Baseline: look at the live counters

```bash
cat /proc/softirqs
watch -d -n1 cat /proc/softirqs        # -d highlights counters that changed
grep ^softirq /proc/stat               # same totals: total, then HI TIMER NET_TX NET_RX BLOCK IRQ_POLL TASKLET SCHED HRTIMER RCU
softirq_delta sleep 10                 # what an idle 10 seconds looks like
```

Expect: on an idle machine `TIMER`, `SCHED` and `RCU` still tick up a little, and everything else stays near zero.

### 2. NET_RX: a big download

```bash
ip -br link                                              # find the interface name
grep -E "CPU|<interface name>" /proc/interrupts          # which CPUs get the NIC interrupts
softirq_delta curl -s -o /dev/null <URL of a large file>
```

Expect: `NET_RX` rises, mostly on the CPUs that receive the NIC interrupts. Wi-Fi drivers often use names like `iwlwifi` in `/proc/interrupts`.

### 3. BLOCK: disk writes

```bash
softirq_delta dd if=/dev/zero of=$HOME/ddtest bs=1M count=1000 oflag=direct
rm $HOME/ddtest
```

Expect: `BLOCK` rises on the CPUs that handle the disk interrupts. On an NVMe disk it may rise only slightly, because many completions avoid this softirq. Check free disk space first, since the test writes 1 GB.

### 4. TIMER, SCHED, RCU: busy vs idle CPUs

```bash
softirq_delta timeout 10 taskset -c 3 yes > /dev/null    # keep CPU 3 busy for 10 seconds
```

Expect: CPU 3 shows noticeably higher `TIMER`, `SCHED` and `RCU` deltas than the idle CPUs, because a busy CPU keeps its tick running while idle CPUs can stop it (dynamic tick).

### 5. Move an interrupt to another CPU (optional)

```bash
grep -E "CPU|<interface name>" /proc/interrupts          # note the IRQ number <n>
cat /proc/irq/<n>/smp_affinity_list                      # write down the original value
echo 5 | sudo tee /proc/irq/<n>/smp_affinity_list        # allow it only on CPU 5
# run experiment 2 again, then restore:
echo <original value> | sudo tee /proc/irq/<n>/smp_affinity_list
```

Expect: `NET_RX` now rises on CPU 5. Some interrupts (for example NVMe) are "managed" by the kernel and refuse the change with an I/O error. A reboot also restores the original setting, and the `irqbalance` service may move it again on its own.

### 6. See every softirq as it happens (advanced)

```bash
echo 1 | sudo tee /sys/kernel/tracing/events/irq/softirq_entry/enable
sudo cat /sys/kernel/tracing/trace_pipe | head -30       # Ctrl+C to stop
echo 0 | sudo tee /sys/kernel/tracing/events/irq/softirq_entry/enable   # turn tracing off again
```

Expect lines containing `vec=<n>` with the softirq name. The vector numbers follow the row order in `/proc/softirqs`: 0 HI, 1 TIMER, 2 NET_TX, 3 NET_RX, 4 BLOCK, 5 IRQ_POLL, 6 TASKLET, 7 SCHED, 8 HRTIMER, 9 RCU. The tracing path may be `/sys/kernel/debug/tracing` on some systems.

### 7. The kernel threads that take over under load

```bash
ps -e | grep ksoftirqd             # one ksoftirqd/N per CPU
top -b -n1 | grep ksoftirqd        # during a heavy download, a busy one shows CPU use
```

`ksoftirqd/N` runs softirqs when they arrive faster than they can be handled right after the interrupt, so normal programs do not get starved.

### What to expect

| Experiment                     | Row that should move                               | Where it should move                                                 |
|--------------------------------|----------------------------------------------------|----------------------------------------------------------------------|
| 2. Big download                | `NET_RX`                                           | CPUs that serve the network card interrupt                           |
| 3. Disk write with `dd`        | `BLOCK` (and `TASKLET`/`HI` if a driver uses them) | CPUs that handle the disk interrupts; may rise only a little on NVMe |
| 4. Busy loop pinned to one CPU | `TIMER`, `SCHED`, `RCU`                            | The pinned CPU, more than the idle ones                              |
| 5. Move the network IRQ        | `NET_RX`                                           | Follows the IRQ to the CPU you chose                                 |

### Safety notes

- Experiments 1 to 4 only read counters or generate normal load. They are safe on your main PC.
- Experiment 5 changes interrupt routing until reboot. Write down the original value first.
- Experiment 6 adds tracing overhead, so switch it off afterwards.