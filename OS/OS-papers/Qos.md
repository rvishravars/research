---
title: "QoS-Aware Power Management via Scheduling and Governing Co-Optimization on Mobile Devices"
author: "Qianlong Sang et al., IEEE Transactions on Mobile Computing, Dec 2024"
revealOptions:
  transition: 'fade'
---

# QoS-Aware Power Management via Scheduling and Governing Co-Optimization on Mobile Devices  
**IEEE Transactions on Mobile Computing (Dec 2024)**  
*Qianlong Sang, Jinqi Yan, Rui Xie, Chuang Hu, Kun Suo, Dazhao Cheng*

---

## Motivation and Problem

- Mobile devices must balance **Quality of Service (QoS)** and **power consumption**.  
- Two key control mechanisms:
  - **Scheduling** – assigns threads to CPU cores  
  - **Governing** – adjusts CPU cluster frequencies
- Current flaws:
  1. **QoS-unaware**: decisions rely only on CPU utilization  
  2. **Decoupled control**: scheduling & governing act independently
- Wasted power up to **64.8 %** in native governor due to overprovisioning 【Figure 2】  

---

## Observations and Insights

- **Observation 1:** Governing should reduce frequency while keeping target frame rate  
- **Observation 2:** Scheduler should prioritize QoS-related threads (UI, Render, SurfaceFlinger)  
- **Observation 3:** Sharing info between modules enables joint optimization  
- ⇒ Need **QoS-aware co-optimization** of scheduling + governing

---

## Orthrus: Proposed System

- Named after the two-headed dog guarding Geryon’s cattle  

Three cooperative modules:

1. **Governor:** Deep RL (PPO) frequency control  
2. **Scheduler:** Finite-State Machine (FSM) QoS-aware thread placement  
3. **Coordinator:** Expert Fuzzy Control (EFC) synchronization  

Goal → **Minimize power** while **guaranteeing QoS** on heterogeneous SoCs  

📊 *Figure 5 Placeholder – Orthrus Architecture*

---

## DRL-Based Governing (PPO)

Formulation — *QoS-Aware Governing Optimization (QGO)*

Inline: objective is to maximize $\frac{1}{T}\sum_{t=1}^{T}\big(Q(t)-P(t)\big)$ subject to $f_c(t)\in F_c$.

Display:

$$
\max_{\pi}\ \frac{1}{T}\sum_{t=1}^{T}\big(Q(t)-P(t)\big)
\quad \text{s.t.}\quad f_c(t)\in F_c
$$

**State:** {power $p(t)$, frame-rate $q(t)$, per-cluster utilization $u_c(t)$, frequency $f_c(t)$, LLC miss $cm(t)$, page fault $pf(t)$}  
**Action:** Discrete CPU frequencies per cluster  
**Reward:** $r(t)=\alpha\,Q(t)-\beta\,P(t)$ with penalty if QoS < threshold  

Stable training via PPO’s clipped objective  

🧠 *Figure 6 Placeholder – PPO workflow*

---

## FSM-Based Scheduling

Three system states:

| State | Meaning |
|:--|:--|
| **U** | Underprovisioned – QoS unmet |
| **M** | Moderate – QoS satisfied, balanced power |
| **O** | Overprovisioned – excess power use |

Detect via **IPS** & **task-clock** metrics  

Thread priority:

$$
i_k=\frac{tc_k}{\sum tc_k}
$$

Prefer frequency scaling (≈ 500 µs) > thread migration (≈ 2 ms)  

📈 *Figure 7 Placeholder – IPS vs Task Clock*

---

## Expert Fuzzy Control Coordinator

- Synchronizes **scheduling → governing** and **governing → scheduling**
- Inputs: utilization $u(t)$, priority $i(t)$
- Linguistic rule example:  
  *If $u(t)=\mathrm{PL}$ and $i(t)=\mathrm{PL}$ → increase frequency (+1)*
- **Scale Hint** reconciles fuzzy and numeric adjustments  
- Frequency info shared via memory for next scheduling step  

🔄 *Figure 8 Placeholder – Coordinator Design*

---

## Implementation

Platform → **Google Pixel 3 (Snapdragon 845, Android 11)**  
Languages → **Python 3.7**, **Java 11**

Steps:
1. Collect system state (`sysfs`, `perf_event`, `SurfaceFlinger`)  
2. Train PPO → convert to TensorFlow Lite  
3. Control CPU frequency via `/sys/.../scaling_setspeed`  
4. Manage threads using `taskset`  

Deployed as Android background service  

🧩 *Figure 9 Placeholder – Experimental Setup*

---

## Evaluation Setup

**Foreground Apps:** TikTok, YouTube, Netflix  
**Background Loads:** CPU, Memory, IO stress  
**Baselines:** Default EAS + schedutil, SmartBalance, zTT, AdaMD  

**Metrics:**  
- QoS Loss $= \sum (G - q(t)) / (G\,T)$  
- Power Consumption (mW)  

**Scenarios:** Light / Medium / Heavy Workload  

📋 *Tables III – IV Placeholders – Baselines & Workloads*

---

## Results Summary

| Workload | Power Reduction vs Default | QoS Maintained |
|:--|:--|:--|
| Light | ≈ 8 % | Yes |
| Medium | 17–21 % | Yes |
| Heavy | **35.7 %** | Yes |

Component study → removing scheduler/governor/coordinator degrades efficiency  

Overheads: 42 MB RAM (1 %), 180 mW runtime cost ≪ savings  

📊 *Figures 10–15 Placeholders – Experimental Results*

---

## Adaptability & Scalability

- Supports multiple QoS metrics (FPS, latency, IPS)  
- Works across heterogeneous CPUs (Pixel 3 → Pixel 6 → OnePlus 9 Pro)  
- Efficient sampling keeps RL action space tractable  
- 1-hour daily use case: ≤ 0.1 % QoS loss, 10 % power saving  

🌐 *Figures 16–18 Placeholders – QoS Adaptation, Scalability, Case Study*

---

## Summary

- **Orthrus** = unified, QoS-aware, intelligent framework for mobile power management  
- **Innovations**
  - PPO-based governor  
  - FSM-based scheduler  
  - Fuzzy-logic coordinator  
- **Results**
  - ≤ 35 % power saving  
  - QoS guaranteed under varied loads  
  - Portable to future heterogeneous SoCs  

> *“Two heads are better than one — Orthrus proves it.”*
