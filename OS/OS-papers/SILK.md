---
paginate: true
title: "Silk: Runtime-Guided Memory Management for Reducing Application Running Janks on Mobile Devices"
---

# **Silk: Runtime-Guided Memory Management**
### Reducing Application Running Janks on Mobile Devices  
*ACM Transactions on Architecture and Code Optimization, 2025*  
**Authors:** Ying Yuan, Zhipeng Tan, Dan Feng, et al.  
**Institution:** Huazhong University of Science and Technology, China

---

## **Motivation**
- Mobile apps run on **Android Runtime (ART)** -> objects ≠ pages.  
- Swap extends memory, but **page-level LRU misjudges object hotness**.  
- Two critical inefficiencies:
  - **App threads:** kernel mistakenly swaps out app-hot objects.  
  - **GC threads:** kernel unaware of GC state -> poor memory management -> *UI jank*.

> **Jank** = visible stutter when frame render latency > 16.6 ms.

---

## **Problem Analysis**

- Kernel uses page-level hotness -> many **pseudo-hot** objects stay in memory.  
- True warm objects evicted -> more swap in -> degraded responsiveness.
- Kernel misidentifies **GC-cold pages as hot** -> frequent swap in/out cycles.

---

## **The Silk Approach**

> **Runtime-guided cooperation between ART and Kernel**

Silk introduces two complementary components:

1. **MO-App:** Manages object hotness for application threads.  
2. **MO-GC:** Manages object hotness for GC threads.

**Goal:** Keep app-hot and GC-hot objects resident, reclaim only cold ones.

---

## **MO-App Design**

- JIT: Tracks objects using **ART compiler sampling** (low CPU overhead).  
- Use **unused bits in lock-word** -> zero memory overhead.  
- Groups objects with similar hotness in same page during GC copying.

![Insert Figure 1 – Native vs. Silk Page Organization](images/silk-fig1.png)
*[Swap-in ↓ 45%, Janks ↓ 55%]*

---

## **MO-GC Design**

- Detects **foreground/background app state** dynamically.  
- Records **region-level hotness** (new vs. old).  
- Guides kernel reclamation to **prioritize GC-cold pages**.

![Insert Figure 2 – GC Thread Timeline: Native vs. Silk](images/silk-fig2.png)

---

## **Implementation**

- Modify:
  - ART runtime for object hotness tracking.  
  - Kernel’s `isolate_lru_pages()` for **guided** reclamation.  
- **Transparent for apps** – no code modification needed.

![architecture](images/silk-architecture.png)

---

## **Testing Setup**

| Category | Example Apps | Behavior |
|-----------|---------------|----------|
| Video | TikTok, YOUKU | Streaming |
| Music | Spotify, NetEase Music | Playback |
| Social | Facebook, Instagram | Scrolling |
| Browser | UC, Baidu | Surfing |
| Map | Gaode, Baidu Map | Navigation |

- Memory pressure simulated by multiple background apps.
- Evaluation on Pixel 3 & Pixel 6 devices.

---

![Insert Figure 18 – Swap-in and Reclaim Sizes](images/silk-fig18.png)

---

## **Experimental Results**

- Swap-in reduced by **45.4%** (vs. Fleet) and **52.8%** (vs. Marvin).  

- Application janks reduced by **55–60%** and P99 frame render latency less by **35%** (Pixel 6).  

 ![Insert Figure 19 – FPS, Jank Ratio, and Frame Latency](images/silk-fig19.png)

---

