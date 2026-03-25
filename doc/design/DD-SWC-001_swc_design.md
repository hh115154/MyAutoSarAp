# SWC 详细设计文档 — VehicleSignalService / SafetyMonitor / IPCBridge

**文档编号**：DD-SWC-001  
**版本**：V1.0  
**日期**：2026-03-25  
**关联架构文档**：SOC-SW-001 §4、IPC-ARCH-001、FS-ARCH-001  
**功能安全等级**：ASIL-B（SafetyMonitor）/ QM（VehicleSignal、IPCBridge）

---

## 1. VehicleSignalService SWC

### 1.1 功能概述

订阅来自 MCU 的整车信号（SOME/IP Service ID 0x1001），提供实时整车数据给其他 AA。

### 1.2 整车信号结构

```cpp
struct VehicleSignals {
    float    vehicleSpeedKph;   // 车速（0~300 km/h）
    uint16_t engineRpm;         // 转速（0~8000 rpm）
    uint8_t  gearPosition;      // 档位（0=P,1=R,2=N,3=D）
    float    batteryVoltage;    // 电池电压（V）
    float    batteryCurrentA;   // 电流（A，正=放电）
    bool     ignitionOn;        // 点火开关
    bool     brakePressed;      // 制动踏板
    uint32_t odometer;          // 里程（km）
    uint32_t timestampMs;       // 时间戳（MCU StbM ms）
    uint8_t  e2eCounterValue;   // E2E 计数器
    uint8_t  e2eCrcValue;       // E2E CRC
};
```

### 1.3 生命周期时序

```
Init():
  1. PHM 注册（Alive 100ms）
  2. ExecClient.ReportExecutionState(kRunning)
  3. 订阅 SOME/IP 0x1001

Run()（100ms 周期）:
  1. PHM CheckpointReached(0x01)
  2. 处理新到达的 VehicleSignals
  3. E2E 校验（counter 连续性）
  4. 通知内部订阅者

Shutdown():
  1. UnsubscribeVehicleSignals()
  2. ExecClient.ReportExecutionState(kTerminating)
```

### 1.4 E2E Profile 2 校验逻辑

```
expectedCounter = lastE2ECounter + 1（mod 256）
if received.counter ≠ expectedCounter AND lastCounter ≠ 0（非首帧）:
    e2eErrorCount++
    if e2eErrorCount > 3:
        PHM.ReportCheckpoint("VehicleSignalService", 0xFF)  // 故障点
    return false（丢弃该帧）
else:
    lastE2ECounter = received.counter
    更新 latestSignals_
    通知回调
```

---

## 2. SafetyMonitorService SWC（ASIL-B）

### 2.1 功能概述

核心安全监控进程，10ms 周期运行，负责：
- 监控 PHM 全局健康状态
- 接收 MCU 安全状态通知（SOME/IP 0x1002）
- 实施 4 级安全降级策略
- 踢 SOC 硬件看门狗

### 2.2 安全状态降级策略（FS-ARCH-001 §4）

| 触发条件 | 目标级别 | SM 动作 |
|---------|---------|---------|
| PHM 全局状态 kExpired | Level3_SafeStop | SM → Error → Shutdown |
| PHM 全局状态 kFailed | Level2_MinRisk | SM → Parked，关闭 Infotainment/OTA |
| MCU asil_dViolation=true | Level4_Emergency | 立即 SM → Shutdown |
| MCU safetyLevel ≥ Level2 | 同 MCU 级别 | 对应 SM 动作 |

### 2.3 10ms 周期安全检查时序

```
T(n):  KickWatchdog() → CheckpointReached("SafetyMonitorService", 0x01)
         ↓
       GetGlobalStatus()
         ├── kExpired → EscalateSafetyLevel(Level3, "PHM expired")
         ├── kFailed  → EscalateSafetyLevel(Level2, "PHM failed")
         └── kNormal  → 检查 MCU 状态
                          ├── asil_dViolation → Level4
                          ├── safetyLevel≥2   → EscalateSafetyLevel
                          └── OK → 无动作
T(n+10ms): 重复
```

---

## 3. IPCBridgeService SWC

### 3.1 功能概述

管理 SOC↔MCU 芯片间三条通信通道：
- **ETH**: SOME/IP（主数据通道）
- **SPI**: MICROSAR.IPC（低时延控制信号，1ms 周期）
- **GPIO x8**: 硬件握手/复位/唤醒

### 3.2 IPC 连接状态机（IPC-ARCH-001 §5.1）

```
OFFLINE ──────────────────────────────────────────────────────┐
  │ Init() + SOC_READY=HIGH                                    │
  ▼                                                            │
HANDSHAKE ──(超时>2s)──────────────────────────────────────→ ERROR
  │ MCU ACK 收到（100ms 内）                                   │
  ▼                                                            │
NORMAL ──(心跳超时>500ms)────────────────────────────────→ ERROR
  │                                                            │
  │ (正常运行)                                                  │
  │  ├── SOME/IP 服务 Offer（7 个服务）                         │
  │  ├── SPI 帧发送/接收（1ms 周期）                            │
  │  └── WDG_KICK GPIO 切换（1ms 周期）                         │
  │                                              ↓             │
  │                              RESET（拉低 SOC_RESET_N 10ms）  │
  │                                              │             │
  └──────────────────────────────────────────────┘ (复位完成)  │
                                                               │
  └──────────────────────────────────────────────────────────┘
```

### 3.3 GPIO 信号定义（IPC-ARCH-001 §4）

| GPIO | 方向 | 描述 | 初始值 |
|------|------|------|-------|
| SOC_RESET_N (0) | SOC→MCU | SOC 复位 MCU（低有效） | HIGH |
| SOC_PWREN (1) | SOC→MCU | 电源使能 | HIGH |
| SOC_READY (2) | SOC→MCU | SOC 启动完成信号 | LOW→HIGH |
| WDG_KICK (3) | SOC→MCU | 踢狗（1ms 翻转） | - |
| WAKE_REQ (4) | 双向 | 唤醒请求 | LOW |
| SLEEP_ACK (5) | MCU→SOC | 休眠确认 | - |
| SAFE_STATE (6) | MCU→SOC | MCU ASIL-D 违反告警 | LOW |
| IRQ_SPI (7) | MCU→SOC | SPI 数据就绪中断 | LOW |

### 3.4 SPI MICROSAR.IPC 帧格式（IPC-ARCH-001 §3.2）

```
┌────┬──────────┬────────┬──────────────┬────────┐
│SOF │FrameCtrl │Length  │   Payload    │ CRC16  │
│0xA5│  1 byte  │ 2 bytes│  N bytes     │ 2 bytes│
└────┴──────────┴────────┴──────────────┴────────┘

CRC16: CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF)
```
