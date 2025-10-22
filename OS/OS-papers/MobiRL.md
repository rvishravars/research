---
marp: true
title: "MobiRL: Intelligent Scheduling on Android OS"
paginate: true
theme: default
---

# MobiRL: Intelligent Scheduling on Android OS  
### Optimizing UI Smoothness and Power  
**Xinglei Dou, Lei Liu, Limin Xiao – Beihang University (ACM TACO, 2025)**  

---

## Problem in Android Frequency Scheduling

- Android devices must balance **UI responsiveness** vs **battery drain**.  
- Current frequency governors (`schedutil`, `interactive`, Hyper Boost):  
  - Use **heuristic triggers** (screen touch, app launch).  
  - Lack full OS context (cache misses, GPU load, IPC).  
- Consequences:  
  - **Over-provision** -> wasted power.  
  - **Under-provision** -> dropped frames and lag.  

---

## Background

![](images/fig1-mobirl.png)

---

## CPU Power–Frequency Characteristics

- Modern SoCs have **heterogeneous CPU clusters**:
  - **Cortex-A55 (LITTLE)** – energy-efficient cores  
  - **Cortex-A78 (MID)** – balanced cores  
  - **Cortex-X1 (BIG)** – performance cores  
- **Power increases super-linearly** with frequency, especially on BIG cores.  
- Even a small MHz boost on X1 can double power use.  
- Explains why **per-cluster adaptive scaling** is essential — not one rule fits all.  
- MobiRL learns when high frequency is worth the energy cost.

---

![Power vs Frequency for CPU clusters](images/fig2-mobirl.png)

---

![placeholder: Figure 3 – Example of TikTok lag due to fixed frequency policy](images/fig3-mobirl.png)

---

## Android OS Context

- **Android kernel (Linux base):**
  - `CPUFreq` & `devfreq` subsystems handle CPU/GPU scaling.  
  - `schedutil` governor estimates load from CPU utilization only.  
- **System layers:**
  - `system_server` hosts performance services.  
  - OEM frameworks (e.g., Hyper Boost, PerfGenius) hook into `powerHint` APIs.  
- Missing link: adaptive feedback from real runtime behavior (frame time, temperature).  

---

![placeholder: Diagram – Android power management stack (HAL → Kernel → Governors)](images/androidPMArchitecture.gif)

---

## MobiRL Integration in Android

- Introduced as a **system component** inside `system_server`.  
- Uses standard Linux interfaces:
  - `/sys/devices/system/cpu/cpufreq/` for CPU.  
  - `/sys/class/kgsl/kgsl-3d0/devfreq/` for GPU.  
- Collects metrics every 10 frames:
  - CPU/GPU load, temperature, DDR freq, app thread info.  
- Issues frequency-limit updates through kernel files.  

---

![placeholder: Figure 5 – MobiRL architecture within Android](images/fig5-mobirl.png)

---

## Architecture & Control Flow

**Main elements:**
1. **System Monitor** – gathers kernel and app metrics.  
2. **Frequency Controller** – writes new freq limits to CPUFreq/devfreq.  
3. **Decision Engine** – computes target freq limits (based on learned policy).  
4. **Reward Evaluator** – measures frame time and power feedback.

**Cycle:** every 167 ms (~6 Hz)  
→ collect → decide → apply → evaluate.

---

## Results on Real Android Hardware

**Platform:** Snapdragon 888, Android 12, kernel 5.4  
**Compared with:** Hyper Boost, `schedutil`, `performance`, `powersave`

| Scheduler | Frame Drops ↓ | Power ↓ |
|------------|---------------|---------|
| **MobiRL** | 4 % | 43 % |
| Q-Learning | 2.5 % | 33 % |
| schedutil  | 3.9 % | 36 % |

- UI smoothness improved, battery drain reduced.  
- Works across multiple apps (TikTok, Weibo, Taobao, Camera).  

---

![placeholder: Figure 7 – Power vs frame-drop comparison chart](images/fig7-mobirl.png)

---

## Key Takeaways for Android Engineers

- **Smart frequency scaling** can be built *within existing Android frameworks*.  
- MobiRL leverages `CPUFreq`/`devfreq` and system metrics, not app hints.  
- Implemented as a lightweight service → runs in `system_server`.  
- Deployed successfully on commercial smartphones.  

**Future direction:**  
> Extend adaptive scheduling to DDR and multi-core activation for finer control.  

---

![placeholder: Closing diagram – Android scheduler feedback loop](images/fig4-mobirl.png)
