<!--
  Project: OS-001
  Authors: kwan, oda, sokpheng001
-->

# OS-001 — Research Foundations

Papers that guide how OS-001 is designed and optimized. We take **ideas** from them, never code
(see [CONTRIBUTING.md](../CONTRIBUTING.md) §1). When a design follows a paper, cite it in a code comment.

Every entry was checked against a live page (arXiv, USENIX, NeurIPS, IEEE, or an author or university copy).
Add only verified papers with real links.

## How the research shapes OS-001

OS-001 is two systems in one: a **normal OS** (kernel, processes, files, desktop) and an **AI system**
(a local model that runs as an OS service). The papers map onto both halves:

| OS-001 subsystem | Roadmap phase | Key papers | Design decision we take from them |
|---|---|---|---|
| Kernel scope | All | Liedtke 1995 | Keep in the kernel only what *must* be there (minimality principle) |
| IPC and syscalls | 4 | Liedtke 1993, Heiser & Elphinstone 2016 | Short messages in registers, direct process switch, no IPC timeouts |
| Permissions | 4 | seL4, EROS, Capsicum, Roesner 2012 | Capabilities: a process can only use what it was handed; the user grants access through normal UI actions (no pop-up spam) |
| Memory | 3 | Navarro 2002, TPP 2023 | 2 MB block mappings for big data (AI model weights); hot/cold page tiers |
| Multicore | 7 | Multikernel 2009 | Per-core state, message passing between cores |
| Scheduling | 4, 7 | ghOSt 2021 | Policy can live in user space; AI work yields to the interactive desktop |
| AI service | 7+ | AIOS, MemGPT, Ge 2023, vLLM | LLM as a scheduled, permission-checked system service; paged context and KV cache |
| On-device inference | 7+ | GPTQ, AWQ, LLM.int8(), LLM in a Flash, PowerInfer, MobileLLM | 4-bit models; stream weights from flash when RAM is small; under-1B models for the low tier |
| Semantic file index | 7+ | Gifford 1991, HNSW, Product Quantization, DiskANN | Search by meaning exposed as virtual folders; compressed vectors so the index fits in tens of MB |

## Top 5 to read first

1. **Liedtke, "On µ-Kernel Construction" (1995).** It decides what belongs in the kernel at all.
2. **Klein et al., seL4 (2009) + Heiser & Elphinstone (2016).** Capability and IPC design, plus 20 years of lessons.
3. **Alizadeh et al., "LLM in a Flash" (2024).** The core technique for "AI scales with RAM".
4. **Mei et al., AIOS (2024/2025).** The most concrete design for an LLM as a system service.
5. **Gifford et al., "Semantic File Systems" (1991).** Meaning-based search through normal file interfaces.

## 1. Microkernel design and fast IPC

- **Improving IPC by Kernel Design**, J. Liedtke. SOSP 1993. [PDF](https://rcs.uwaterloo.ca/papers/l4.pdf)
  The IPC fast-path recipe: register messages, direct switch, cache/TLB-aware layout. Use it for our AArch64 syscall/IPC path (Phase 4).
- **On µ-Kernel Construction**, J. Liedtke. SOSP 1995. [PDF](https://flint.cs.yale.edu/cs428/doc/p237-liedtke.pdf)
  The minimality principle; address spaces built from grant / map / flush. Use it to scope the kernel API.
- **seL4: Formal Verification of an OS Kernel**, G. Klein et al. SOSP 2009. [PDF](https://trustworthy.systems/publications/papers/Klein_EHACDEEKNSTW_09.pdf)
  A small capability microkernel; the model for our capability spaces and untyped-memory design (ideas only).
- **L4 Microkernels: The Lessons from 20 Years of Research and Deployment**, G. Heiser, K. Elphinstone. ACM TOCS 34(1), 2016. [Abstract](https://trustworthy.systems/publications/nictaabstracts/Heiser_Elphinstone_16.abstract)
  Which L4 ideas survived and which were dropped. Read before freezing the IPC ABI.

## 2. Alternative kernel structures

- **Exokernel: An Operating System Architecture for Application-Level Resource Management**, D. Engler, M. F. Kaashoek, J. O'Toole. SOSP 1995. [PDF](https://pdos.csail.mit.edu/6.1810/2019/readings/engler95exokernel.pdf)
  Lets the AI service manage its own memory and accelerators directly instead of through generic layers.
- **The Multikernel: A New OS Architecture for Scalable Multicore Systems**, A. Baumann et al. SOSP 2009. [PDF](https://pdos.csail.mit.edu/6.828/2010/readings/barrelfish_sosp09.pdf)
  Per-core kernels with message passing; guides SMP on the Pi's 4 cores (Phase 7).
- **Unikernels: Library Operating Systems for the Cloud**, A. Madhavapeddy et al. ASPLOS 2013. [PDF](https://sites.cs.ucsb.edu/~rich/class/cs270/papers/unikernel.pdf)
  Sealed single-purpose images; a model for an isolated "inference appliance" or driver servers.

## 3. Capability-based security and least privilege

- **EROS: A Fast Capability System**, J. Shapiro et al. SOSP 1999. [PDF](https://courses.cs.washington.edu/courses/cse551/19wi/readings/eros-sosp99.pdf)
  A pure capability OS can be as fast as a conventional one.
- **Capsicum: Practical Capabilities for UNIX**, R. Watson et al. USENIX Security 2010. [Page](https://www.usenix.org/conference/usenixsecurity10/capsicum-practical-capabilities-unix)
  Capability mode and capability file descriptors; a blueprint for sandboxing real desktop apps.
- **User-Driven Access Control: Rethinking Permission Granting in Modern Operating Systems**, F. Roesner et al. IEEE S&P 2012. [PDF](https://homes.cs.washington.edu/~yoshi/papers/udac-oakland2012.pdf)
  A genuine user action grants the capability (no permission prompts); fits our desktop and plain-language shell.

## 4. LLM-as-OS and AI agent operating systems

- **AIOS: LLM Agent Operating System**, K. Mei et al. COLM 2025 (arXiv 2024). [arXiv](https://arxiv.org/abs/2403.16971)
  An agent kernel with an LLM scheduler, context switching, memory manager and access control: the template for our AI service API.
- **MemGPT: Towards LLMs as Operating Systems**, C. Packer et al. arXiv 2023. [arXiv](https://arxiv.org/abs/2310.08560)
  Pages LLM context between main and external memory; fits long assistant memory into the small context windows of models that run in 512 MB–2 GB.
- **LLM as OS, Agents as Apps: Envisioning AIOS, Agents and the AIOS-Agent Ecosystem**, Y. Ge et al. arXiv 2023. [arXiv](https://arxiv.org/abs/2312.03815)
  LLM = kernel, context = memory, tools = devices, language = programming interface; guides the "OS explains what apps are doing" layer.

## 5. Efficient on-device LLM inference

- **GPTQ: Accurate Post-Training Quantization for Generative Pre-trained Transformers**, E. Frantar et al. ICLR 2023. [arXiv](https://arxiv.org/abs/2210.17323)
- **AWQ: Activation-aware Weight Quantization for LLM Compression and Acceleration**, J. Lin et al. MLSys 2024. [arXiv](https://arxiv.org/abs/2306.00978)
- **LLM.int8(): 8-bit Matrix Multiplication for Transformers at Scale**, T. Dettmers et al. NeurIPS 2022. [arXiv](https://arxiv.org/abs/2208.07339)
  The three quantization papers: 4-bit models for small-RAM tiers; outlier handling for our int8 NEON kernels.
- **LLM in a Flash: Efficient Large Language Model Inference with Limited Memory**, K. Alizadeh et al. ACL 2024. [arXiv](https://arxiv.org/abs/2312.11514)
  Runs models larger than RAM by streaming sparse weights from flash: a storage + virtual-memory co-design the OS owns.
- **PowerInfer: Fast Large Language Model Serving with a Consumer-grade GPU**, Y. Song et al. SOSP 2024. [arXiv](https://arxiv.org/abs/2312.12456)
  and **PowerInfer-2: Fast LLM Inference on a Smartphone**, Z. Xue et al. arXiv 2024. [arXiv](https://arxiv.org/abs/2406.06282)
  Hot/cold neuron locality; a model for an OS-aware inference runtime on Pi-class hardware.
- **MobileLLM: Optimizing Sub-billion Parameter Language Models for On-Device Use Cases**, Z. Liu et al. ICML 2024. [arXiv](https://arxiv.org/abs/2402.14905)
  Picks and sizes the default model for the lowest AI tier.

## 6. Semantic file systems and low-memory vector search

- **Semantic File Systems**, D. Gifford et al. SOSP 1991. [PDF](https://web.mit.edu/6.826/www/notes/HO13.pdf)
  Attribute extractors plus virtual directories; our semantic index replaces the extractors with embeddings.
- **Efficient and Robust Approximate Nearest Neighbor Search Using HNSW Graphs**, Yu. Malkov, D. Yashunin. IEEE TPAMI 2020. [arXiv](https://arxiv.org/abs/1603.09320)
  The in-RAM vector index for 4–8 GB devices.
- **Product Quantization for Nearest Neighbor Search**, H. Jégou et al. IEEE TPAMI 2011. [PDF](https://hal.archives-ouvertes.fr/file/index/docid/825085/filename/jegou_pq_postprint.pdf)
  Compressed vectors, so the index fits on 512 MB devices.
- **DiskANN: Fast Accurate Billion-point Nearest Neighbor Search on a Single Node**, S. Jayaram Subramanya et al. NeurIPS 2019. [Page](https://proceedings.neurips.cc/paper/2019/hash/09853c7fb1d3f8ee67a61b6bf4a7f8e6-Abstract.html)
  Index on SD/NVMe storage with compressed vectors in RAM, for low-RAM tiers.

## 7. Memory management and scheduling for mixed AI + interactive work

- **Efficient Memory Management for Large Language Model Serving with PagedAttention** (vLLM), W. Kwon et al. SOSP 2023. [arXiv](https://arxiv.org/abs/2309.06180)
  OS-style paging for the AI service's KV cache; shared prompt prefixes across shell sessions.
- **Practical, Transparent Operating System Support for Superpages**, J. Navarro et al. OSDI 2002. [Page](https://www.usenix.org/conference/osdi-02/practical-transparent-operating-system-support-superpages)
  Huge-page mappings for model weights to cut TLB misses (Phase 3).
- **TPP: Transparent Page Placement for CXL-Enabled Tiered-Memory**, H. Al Maruf et al. ASPLOS 2023. [arXiv](https://arxiv.org/abs/2206.02878)
  Hot/cold page tiers; adapt to DRAM → compressed RAM → flash.
- **ghOSt: Fast & Flexible User-Space Delegation of Linux Scheduling**, J. T. Humphries et al. SOSP 2021. [Page](https://research.google/pubs/ghost-fast-and-flexible-user-space-delegation-of-linux-scheduling/)
  User-space scheduling policy, so AI threads yield to the interactive desktop without kernel changes.
