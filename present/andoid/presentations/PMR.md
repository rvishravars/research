# PMR: Fast Application Response via Parallel Memory Reclaim on Mobile Devices

This paper is included in the Proceedings of the
2025 USENIX Annual Technical Conference.
July 7–9, 2025 • Boston, MA, USA
ISBN 978-1-939133-48-9

Wentong Li , Li-Pin Chang , Yu Mao, Liang Shi,
MoE Engineering Research Center of Software/Hardware Co-Design Technology and Application,
East China Normal University, Shanghai, China
School of Computer Science and Technology, East China Normal University
National Yang Ming Chiao Tung University, Taiwan
City University of Hong Kong, Hong Kong, MBZUAI, Abu Dhabi

---

# Challeges

- Mobile apps increasingly need **GBs of RAM**  
- Devices have limited DRAM (8–16 GB typical)  
- **Conventional reclaim is sluggish**, triggering LMKD kills  
- Frequent app terminations hurt user experience  

![Placeholder: Graph of LMKD kills over time on Pixel devices](images/freq-memory-kill.png)

---

![](images/traditional-memory-reclaim.png)

---

# Bottlenecks in Android Reclaim

- **Sequential flow:** page shrinking → page writeback  
- **Page Shrinking**: slow victim isolation (~55% latency)  
- **Page Writeback**: per-page unmap & 4 KB writes → overhead  
- Storage throughput underutilized (<100 MB/s vs >300 MB/s possible)

---

# PMR Design Overview

- **Parallelizes memory reclaim path**  
- Two components:  
  1. **Proactive Page Shrinking (PPS)**  
  2. **Storage-Friendly Page Writeback (SPW)**  
- Decouples scanning and writing → pipeline efficiency  
- Minimally invasive kernel changes  

---

![](images/PMR-reclaim.png)

---

# Proactive Page Shrinking (PPS)

- New kernel thread: **`kshrinkd`**  
- Scans LRU lists continuously, isolates “victim” pages  
- Maintains **Victim Page List** (ready-to-reclaim)  
- Triggered by **page watermark**, not just low memory  
- Ensures immediate supply of pages for reclaim  

---

# Storage-Friendly Page Writeback (SPW)

- **Application-aware unmap:** bulk unmap of process pages  
- Groups pages → reduces lock contention
- **Batch I/O writes** (≈10 MB sweet spot/optimal)  
- Fully uses flash storage parallelism  
- Achieves near-peak device throughput  

---

# Storage-Friendly Page Writeback (SPW)

![Graph comparing 4 KB vs bulk I/O write throughput](images/4kbvs10MB.png)

---


# Results & Impact

- **Due to faster memory reclaim, App response time reduced by 43.6%**  

- **LMKD kills reduced by up to 82%**  & Reclaim throughput ↑ ~80%  

- Near-peak flash memory utilization & Minimal CPU cost/overhead (<6%)  

![Placeholder: Bar chart showing app response time improvement with PMR](images/pmr-improvement.png)

---
