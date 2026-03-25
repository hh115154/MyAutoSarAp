# ara::tsync — Time Synchronization 详细设计文档

**文档编号**：DD-TSYNC-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_TimeSync  
**关联架构文档**：SOC-SW-001 §3.4  
**功能安全等级**：QM（时间精度影响事件关联，不直接触发安全动作）

---

## 1. 功能概述

`ara::tsync` 提供 **全局时基同步**，使 SOC 侧所有 AA 使用统一时间戳，并与 MCU 侧 StbM（Synchronized Time Base Manager）保持时钟对齐。

### 1.1 时基规划

| 时基 ID | 描述 | 同步源 | 精度目标 |
|--------|------|--------|---------|
| kSystemTime | 系统主时基 | gPTP/PTP over ETH | ≤1μs |
| kVehicleTime | 整车时基 | MCU StbM via SPI | ≤1ms |
| kGpsTime | GPS 时基 | 导航模块 NMEA | ≤100ms |
| kLocalMono | 本地单调钟 | CLOCK_MONOTONIC | 不同步 |

---

## 2. 架构设计

### 2.1 SOC-MCU 时钟同步链路

```
GPS 接收机（可选）
        │ NMEA/PPS
        ▼
MCU StbM（主时钟 Master）
  TimeSync 服务通过 SPI MICROSAR.IPC 周期推送时间戳（1ms）
        │
        ▼ SPI（1ms 周期，IPC-ARCH-001 §3）
SOC SynchronizedTimeBaseProvider(kVehicleTime)
        │ SetTime() / ApplyCorrection()
        ▼
SynchronizedTimeBaseConsumer(kVehicleTime)
        │ GetCurrentTime() → 返回与 MCU 对齐的时间戳
        ▼
VehicleSignalService SWC：信号时间戳标注
SafetyMonitorService SWC：故障事件时间戳
DLT 日志：ara::log 时间戳标注
```

### 2.2 时间戳格式

```
struct TimePoint {
    int64_t  seconds;       // 自 Unix Epoch 以来的秒数
    uint32_t nanoseconds;   // 0 ~ 999,999,999
};

// 总精度：纳秒级（64+32 bit = 96 bit 时间戳）
```

---

## 3. 接口定义

### 3.1 SynchronizedTimeBaseConsumer

```cpp
class SynchronizedTimeBaseConsumer {
    explicit SynchronizedTimeBaseConsumer(TimeBaseId timeBaseId);

    TimePoint GetCurrentTime() const;           // 获取当前时间（SWS_TS_00050）
    static TimeDiff GetTimeDiff(t1, t2);        // 计算时间差（纳秒）
    SyncStatus GetSyncStatus() const;           // 查询同步状态
    TimeBaseId GetTimeBaseId() const;
};
```

### 3.2 SynchronizedTimeBaseProvider

```cpp
class SynchronizedTimeBaseProvider {
    explicit SynchronizedTimeBaseProvider(TimeBaseId timeBaseId);

    void SetTime(const TimePoint& timePoint);       // 更新时间（由驱动调用）
    void SetSyncStatus(SyncStatus status);          // 设置同步状态
    void ApplyCorrection(int64_t correctionNs);     // 应用 ΔT 校正（SWS_TS_00080）
};
```

### 3.3 便利函数

```cpp
TimePoint GetCurrentTime();  // 等价于 Consumer(kSystemTime).GetCurrentTime()
std::chrono::system_clock::time_point ToChronoTimePoint(const TimePoint&);
TimePoint FromChronoTimePoint(const std::chrono::system_clock::time_point&);
```

---

## 4. 时钟漂移补偿

MCU StbM 每 1ms 通过 SPI 推送时间戳，SOC tsync 收到后：

```
if |SOC_time - MCU_time| > 1ms:
    ApplyCorrection(MCU_time - SOC_time)  // 线性补偿
else:
    保持当前时钟（避免跳变）
```

---

## 5. 日志时间戳集成

`ara::log` 的所有日志条目使用 `GetCurrentTime()` 获取时间戳，
格式：`[秒.纳秒]`（DLT v2 协议标准时间戳格式）。
