# Linux Kernel Thread Monitor & User-space RR Scheduler

![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)

本專案的3_2實作了一個自定義的 **使用者空間排程器 (User-space Scheduler)**，並結合 **Linux 核心模組 (Kernel Module)** 透過 `/proc` 檔案系統達成雙向溝通，用以監控執行緒的即時運行狀態與 CPU 耗時。

---

## 核心功能

### 1. 核心空間監控 (Kernel Space)
* 建立 `/proc/Mythread_info` 節點作為溝通橋樑。
* 利用核心內部 `current` 指標擷取執行緒的 `utime` (CPU 執行時間)、`prio` (優先級) 與 `tgid`。
* 實作 `msg_pool` 陣列，確保不同 PID 的執行緒在讀取監控資訊時不會互相干擾。

### 2. 使用者空間排程 (User Space)
* **Round-Robin 排程**：利用 `SIGALRM` 訊號與 `setitimer` 實作每 20ms 一次的強制任務換手 (Preemptive Scheduling)。
* **TCB 管理**：維護TCB，追蹤 `READY`、`RUNNING` 與 `TERMINATED` 狀態。
* **任務**：Task 1 (CPU )：執行大規模矩陣乘法運算。Task 2 (I/O)：定時向核心回報「心跳」訊息並讀取監控數據。

---

## 3_2專案結構

| 檔案 | 描述 |
| :--- | :--- |
| `My_Kernel.c` | 核心模組源碼，處理 `/proc` 讀寫邏輯 |
| `scheduler.c` | 排程器核心，包含心跳發送、RR 排程演算法與執行緒切換 |
| `task.c` | 矩陣運算 |


---

## 快速開始

```bash
cd 3/3_2
make all
make load
make Prog_2thread
make check
