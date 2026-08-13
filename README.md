# Nova-RTOS

A preemptive, priority-based real-time operating system built from scratch in C and ARM assembly for the **ARM926EJ-S** processor, running on the **VersatilePB** platform (QEMU-emulated).

No RTOS libraries. No HAL. No vendor SDK. Every layer — from the reset vector to the scheduler to the mutex implementation — was written and debugged from first principles.

```
Bootloader → Vector Table → Kernel → Preemptive Scheduler → Application (Sensor Fusion)
```

---

## Why This Project Exists

Most "embedded" student projects stop at *"I used FreeRTOS to blink an LED."* Nova-RTOS goes one level deeper: it **is** the RTOS. The goal was to demonstrate genuine systems-level understanding — memory layout, CPU modes, interrupt handling, and the classic failure modes that real RTOS engineers deal with in production (priority inversion, stack corruption, race conditions).

Every bug listed in the [Debugging Journey](#debugging-journey--real-bugs-found-and-fixed) section below was hit, diagnosed, and fixed during development — nothing here is theoretical.

---

## Hardware / Toolchain

| Component | Choice | Why |
|---|---|---|
| CPU | ARM926EJ-S (ARMv5TE) | Classic ARM architecture — no Cortex-M conveniences like PendSV. Forces you to understand banked registers and mode-switching the way real ARM/Linux systems do it. |
| Platform | VersatilePB (QEMU `-M versatilepb`) | Free, cycle-accurate emulation. No hardware purchase required — directly portable to real ARM926-based silicon. |
| Toolchain | `arm-none-eabi-gcc` (freestanding) | No OS, no libc — everything from scratch. |
| Timer | SP804 dual timer @ `0x101E2000` | Drives the 10ms preemption tick. |
| Interrupt Controller | PL190 VIC @ `0x10140000` | Routes the timer IRQ to the CPU. |
| UART | PL011 @ `0x101F1000` | Only I/O — all debugging is done over serial. |

> **Note on hardware:** this was developed and fully validated on QEMU rather than physical silicon, by design — QEMU's ARM926EJ-S emulation is cycle-accurate, and the code makes no QEMU-specific assumptions. It is directly portable to real ARM926-based boards with only peripheral base-address changes (if a different SoC's timer/UART/VIC addresses differ).

---

## Architecture

```
┌─────────────────────────────────────────────┐
│           Application Tasks                  │
│  LOW / MEDIUM / HIGH priority demo tasks      │
│  SENSOR task  →  FUSION task (Kalman filter)  │
├─────────────────────────────────────────────┤
│              Kernel Services                  │
│  Scheduler (priority + round-robin tiebreak)  │
│  Mutex (priority inheritance)                 │
│  task_sleep() (blocking delay)                │
│  Stack canary (overflow detection)            │
├─────────────────────────────────────────────┤
│           Context Switch Layer                │
│  IRQ_Handler (ARM926 banked-register save)    │
│  cpu_switch_context (SVC/IRQ mode dance)      │
├─────────────────────────────────────────────┤
│              Drivers                          │
│  UART (PL011) — atomic critical-section prints│
│  Timer (SP804) — 10ms periodic tick            │
│  VIC (PL190) — IRQ routing                     │
├─────────────────────────────────────────────┤
│           Boot / Vector Table                 │
│  Reset_Handler → stack setup → kernel_main    │
│  Vector table @ 0x00000000                     │
└─────────────────────────────────────────────┘
```

### Memory Map (`linker.ld`)

```
0x00000000  ┌─────────────────┐
            │  Vector Table    │  8 entries, ARM exception vectors
            ├─────────────────┤
            │  .text           │  code (boot, kernel, drivers)
            ├─────────────────┤
            │  .rodata         │  constants
            ├─────────────────┤
            │  .data / .bss    │  globals, task stacks, TCBs
            ├─────────────────┤
                    ...
            ├─────────────────┤
0x08000000  │  stack_top       │  top of 128MB RAM, minus 4KB reserved
            └─────────────────┘
```

The vector table is pinned to `0x00000000` deliberately — on reset, the ARM926 fetches its exception vectors from address 0, and the IRQ vector specifically lives at offset `0x18`. Getting this placement wrong was one of the very first bugs hit (see below).

---

## Feature Breakdown

### 1. Custom Bootloader
`boot/boot.s` sets up the CPU from reset: configures the vector table, sets up **separate stack pointers for IRQ mode and System mode** (a step that's easy to skip and immediately fatal if skipped — see Debugging Journey), and jumps into `kernel_main()`.

### 2. Preemptive, Priority-Based Scheduler
Every 10ms, the SP804 timer fires an IRQ. The handler saves the current task's full register context onto **that task's own stack**, calls `schedule_next()` to pick the next task by priority (with round-robin tie-breaking among equal priorities), and restores the new task's context. This is true preemption — a task can be interrupted mid-instruction, mid-`printf`, anywhere.

**Design choice:** the ARM926 has no hardware divider, so `%` (modulo) silently pulls in a software division routine (`__aeabi_idivmod`) from libgcc that isn't linked by default. Rather than link the whole soft-division library for one operation, the scheduler's round-robin index uses **manual wraparound** (`if (idx >= task_count) idx = 0;`) instead of `% task_count`. Zero-cost, deterministic, and avoids an unnecessary dependency.

### 3. ARM926 Context Switching (Banked Registers)
Unlike Cortex-M (which has `PendSV` for painless context switching), the ARM926 only banks `SP` and `LR` per CPU mode — `r0`–`r12` are shared across all modes. The IRQ handler exploits this: it saves `r0`–`r12` onto the **interrupted task's own stack** (by briefly switching to System mode inside the handler), then separately preserves the return PC and CPSR via the IRQ-mode banked registers. This mirrors how real ARM Linux context-switches at the kernel level.

Stack frame layout (low → high address): `[CPSR, PC, r0..r12, LR]` — every task's initial stack, built by `task_create()`, is hand-constructed to match this exact layout so the very first "switch" into a brand-new task looks identical to resuming a previously-preempted one.

### 4. Mutex with Priority Inheritance
Nova-RTOS implements the classic **Priority Inheritance Protocol** to solve priority inversion — the same class of bug that caused the Mars Pathfinder mission's watchdog resets in 1997.

**Scenario demonstrated in `kernel.c`:**
- `LOW` (priority 1) acquires a shared mutex.
- `HIGH` (priority 3) wants the same mutex and must wait.
- `MEDIUM` (priority 2) is CPU-hungry but never touches the mutex.

Without inheritance, `MEDIUM` would keep preempting `LOW` indefinitely (since `MEDIUM` > `LOW` in priority), and `HIGH` would starve forever waiting on a mutex held by a task that never gets to run — this is priority inversion.

**Fix implemented:** when `HIGH` blocks on the mutex, it temporarily boosts `LOW`'s priority to match its own. `MEDIUM` can no longer preempt the boosted `LOW`. `LOW` finishes quickly, releases the mutex, its priority reverts to baseline, and `HIGH` proceeds.

### 5. Real Blocking (`task_sleep`)
Tasks that "wait" do so with a genuine `TASK_SLEEPING` / `TASK_BLOCKED` state, not a busy-spin. The scheduler skips sleeping/blocked tasks entirely when selecting the next task to run, which is what makes CPU time actually available to lower-priority tasks. (A busy-spin looks identical to "always ready" from the scheduler's point of view — see Debugging Journey, starvation bug.)

### 6. Stack Overflow Detection (Canary)
Every task's stack is stamped with a magic value (`0xDEADBEEF`) at the lowest address of its stack region at creation time. On every scheduler tick, the outgoing task's canary is checked. If a task overflows its stack (deep recursion, oversized locals), the canary is the first thing to get corrupted — Nova-RTOS detects this and halts safely with the offending task's name printed, rather than continuing into undefined behavior.

**Verified live:** a deliberately recursive test function was used to trigger a real overflow; the system correctly detected it and printed:
```
!!! STACK OVERFLOW DETECTED !!!
Task: MEDIUM
System halted for safety.
```

### 7. Sensor Fusion with a Kalman Filter
A simulated sensor task generates noisy readings (via a lightweight custom LCG pseudo-random generator — `rand()` isn't available in a freestanding environment). A separate fusion task runs a 1D Kalman filter over the raw stream to produce a smoothed estimate:

```
prediction:  P = P + Q
gain:        K = P / (P + R)
update:      x = x + K·(measurement - x)
             P = (1 - K)·P
```

**Fault injection & graceful degradation:** the sensor task periodically simulates a sensor failure. During a fault window, the fusion task detects the fault flag and **holds its last known-good estimate** instead of fusing garbage data — the system degrades gracefully instead of crashing or producing a corrupted output. When the sensor recovers, fusion resumes automatically.

### 8. Race-Condition-Free UART
Because preemption can interrupt a task mid-`uart_puts()`, concurrent prints from two tasks were originally interleaving character-by-character (garbled output — visible proof that preemption was genuinely working mid-function). Fixed by wrapping UART output in a critical section that disables/restores IRQs around each print (`irq_disable_save()` / `irq_restore()`), implemented with `MRS`/`MSR` on `CPSR` since ARMv5 does not support the `CPSID`/`CPSIE` instructions available on newer ARM cores.

---

## Debugging Journey — Real Bugs Found and Fixed

This section exists because *how* these were diagnosed is more representative of real embedded engineering than the final working code.

| # | Symptom | Root Cause | Fix |
|---|---|---|---|
| 1 | Linker error: `undefined reference to __aeabi_idivmod` | `%` (modulo) on ARM926 has no hardware divider — GCC emits a libgcc call that wasn't linked | Replaced modulo with manual wraparound in the scheduler; later, for the Kalman filter (which genuinely needs float division), linked via `gcc -lgcc` instead of raw `ld` |
| 2 | System boots, prints one line, then silently hangs | IRQ-mode and System-mode stack pointers were never separately initialized in the reset handler — first IRQ corrupted memory | Explicitly set `SP_irq` and `SP_sys` in `Reset_Handler` before enabling interrupts |
| 3 | New tasks crash immediately on first switch | Initial stack frame written in the wrong byte order — `task_create()` wrote `[PC, CPSR, zeros]` but the restore code expected `[CPSR, PC, r0..r12, LR]` (low→high) | Rewrote `task_create()` to push fields in reverse order so the final memory layout matches what `context_restore` expects |
| 4 | Concurrent task output appeared garbled/interleaved | Preemption was interrupting `uart_puts()` mid-string — proof preemption worked, but unsafe | Added IRQ-disable/restore critical section around UART output |
| 5 | `cpsid i` / `cpsie i` — "instruction not supported" | Those instructions require ARMv6+; ARM926 is ARMv5TE | Replaced with manual `MRS`/`MSR` on `CPSR_c` to save/restore the IRQ-enable bit |
| 6 | High-priority task ran forever; low/medium priority tasks never printed at all | Tasks were "waiting" via busy-spin loops, which never actually change task state — the scheduler saw every task as perpetually `READY` and always picked the highest priority one | Implemented genuine `TASK_BLOCKED`/`TASK_SLEEPING` states plus a real `task_sleep()` that removes a task from scheduling consideration until it's explicitly woken |
| 7 | Deliberate stack-overflow test caused a silent freeze instead of a detected crash | The recursive test function consumed the entire stack in microseconds — faster than the 10ms scheduler tick that performs the canary check, so corruption reached adjacent memory (and an unhandled CPU fault) before detection could run | Slowed the test recursion down (smaller buffer + inter-call delay) to make the failure mode "detectable" within one tick — and noted as a real lesson: **a periodic software canary check alone isn't sufficient for very fast corruption; a hardware watchdog is the correct production backstop** |
| 8 | Sensor fault-injection only ever triggered once, near the start of the run | The cycle counter used for fault-window timing incremented forever without wrapping | Added manual wraparound so the fault window repeats periodically, making the fault → hold-estimate → recover cycle observable repeatedly during a run |

---

## Project Structure

```
Nova-RTOS/
├── boot/
│   ├── boot.s            # Reset handler, stack setup, vector table entry
│   ├── irq_handler.s      # Vector table, IRQ context save/restore
│   └── linker.ld          # Memory layout, vector table @ 0x0
├── drivers/
│   ├── uart.c / uart.h    # PL011 UART, atomic prints, int/float formatting
│   └── timer.c / timer.h  # SP804 timer + PL190 VIC configuration
├── kernel/
│   ├── task.c / task.h    # TCB, task creation, stack canary
│   ├── scheduler.c / .h   # Priority-based preemptive scheduler
│   ├── mutex.c / .h       # Mutex with priority inheritance
│   ├── kalman.c / .h      # 1D Kalman filter
│   ├── critical.h         # IRQ disable/restore helpers
│   └── kernel.c           # Application entry point, demo tasks
└── Makefile
```

---

## Building and Running

**Requirements:** `arm-none-eabi-gcc` toolchain, `qemu-system-arm`.

```bash
make clean
make run
```

This compiles all sources, links against `libgcc` (needed for the software float/division routines used by the Kalman filter), and boots the resulting `kernel.elf` directly in QEMU on the VersatilePB machine.

### Expected Output

```
Nova-RTOS: Sensor Fusion + Priority Inheritance Demo
[LOW] Started
[HIGH] Started
[MEDIUM] Started
[SENSOR] Started
[FUSION] Started
[HIGH] Trying to get mutex...
[LOW] Got mutex, doing work...
[LOW] Releasing mutex
[HIGH] Got mutex! (priority inheritance worked)
[FUSION] Raw=100.30 Fused=99.92
[MEDIUM] Running (mutex se koi lena dena nahi)...
...
[FUSION] WARNING: sensor fault! Holding last good estimate: 100.05
[FUSION] Sensor recovered, resuming fusion.
```

---

## What This Demonstrates

- Low-level ARM architecture fluency (CPU modes, banked registers, exception vectors, CPSR manipulation)
- Real-time scheduling theory in practice (preemption, priority scheduling, priority inversion and its fix)
- Systems debugging methodology (stack frame tracing, register-level reasoning, root-causing silent hangs)
- Fault-tolerant design (stack canaries, graceful sensor degradation, safe halt on corruption)
- Applied numerical methods on constrained hardware (Kalman filtering without an FPU, via software float)

---

## Possible Extensions

- Hardware watchdog timer as a backstop to the software stack canary (see Debugging Journey #7)
- Message queues for inter-task communication
- Power-aware idle (`WFI` when no task is ready)
- Port to physical ARM926-based hardware
