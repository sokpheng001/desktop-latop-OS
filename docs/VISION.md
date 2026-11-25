<!--
  Project: OS-001
  Authors: kwan, oda, sokpheng001
-->

# OS-001 — Vision

> **A private, lightweight desktop OS where AI is built in and runs entirely on your own machine.**

## Why this OS exists

| Today's desktops | OS-001 |
|---|---|
| AI features send your data to the cloud | AI runs **locally**; your data never leaves the machine |
| AI is an app bolted on top | AI is a **system service**, part of the OS |
| Telemetry, accounts, background tracking | **No telemetry, no account, works fully offline** |
| Needs modern, expensive hardware | Base OS runs on a **Raspberry Pi or an old laptop** |
| Files found by name and folder | Files found by **meaning** |

## The three pillars

### 1. AI-native
- **Plain-language shell**: "show photos from the beach trip", "free up disk space", alongside normal commands.
- **Semantic file system**: every file is indexed by its content and meaning, not just its path.
- **Explainable system**: ask "what is this app doing?" and get a clear answer about its files, network and CPU use.
- AI is an **OS service** with one API that every app can use. There is no per-app cloud subscription.

### 2. Privacy
- AI inference happens **on-device only**. There is no cloud AI by default.
- **No telemetry, no required account, offline-first.**
- **Permission-based access**: apps, *including the AI assistant*, can touch only the files, network, camera and microphone the user grants.
- The user can see **every network connection** and block any of them.

### 3. Lightweight
- Small kernel, fast boot, low memory use.
- **AI scales with the hardware**. The OS is fully usable without AI, and AI gets better with more RAM:

| Hardware tier | Example | Experience |
|---|---|---|
| Base (512 MB+) | Pi 3, old laptop | Full desktop OS, keyword search, no local AI model |
| AI-lite (4 GB+) | Pi 4/5, budget laptop | Small local model: commands, search by meaning |
| AI-full (8 GB+) | Pi 5 8 GB, modern PC | Larger model: assistant, summaries, explanations |

## Design principles

1. **Local first:** nothing requires the internet.
2. **The user is in control:** AI suggests and the user approves. AI never acts silently on files or the network.
3. **Small and understandable:** every component should be readable and explainable.
4. **Portable:** one codebase. Raspberry Pi first, then x86_64 PCs, and VMs everywhere (see `HARDWARE.md`).
5. **Our own code:** our own kernel, written from scratch. We use no existing kernel.

## Architecture overview

```
┌─────────────────────────────────────────────────────┐
│  Desktop & apps   │  AI shell  │  File browser  ...  │  user space
├─────────────────────────────────────────────────────┤
│  AI service (local inference) │ Semantic index       │  system services
│  Permission manager           │ Network monitor      │  (user space)
├─────────────────────────────────────────────────────┤
│  OUR KERNEL: memory, scheduler, IPC, permissions,    │  kernel
│  syscalls, file system, drivers (through the HAL)    │
├─────────────────────────────────────────────────────┤
│  HAL: arch/aarch64, arch/x86_64, board/rpi4, board/pc │
└─────────────────────────────────────────────────────┘
```

The AI runs in **user space**, not inside the kernel. A crash or bug in the AI can never take down the OS, and the AI must ask the kernel's permission system like any other app.

## What the kernel must provide for this vision
- **Permissions in the kernel from day one:** every process has a list of what it may access.
- **Fast IPC** (inter-process communication): apps talk to the AI service by message.
- **Efficient math:** use the CPU's vector units (ARM NEON, x86 AVX) for AI inference.
- **File change notifications:** so the semantic index stays up to date.

## Open decisions
- [ ] OS name
- [ ] Local AI inference engine: write our own, or port an existing user-space engine
- [ ] Which small AI model(s) to ship by default
- [ ] Kernel style: monolithic, or microkernel (leaning microkernel for isolation)
