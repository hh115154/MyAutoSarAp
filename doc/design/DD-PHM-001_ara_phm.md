# ara::phm — Platform Health Management 详细设计文档

**文档编号**：DD-PHM-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_PlatformHealthManagement  
**关联架构文档**：SOC-SW-001 §3.7、FS-ARCH-001 §3  
**功能安全等级**：ASIL-B（SOC VM1 分区核心安全组件）

---

## 1. 功能概述

`ara::phm` 是 AUTOSAR AP R25-11 的**平台健康管理**功能簇，负责监督所有 AA（Adaptive Application）进程的运行时健康状态，在检测到故障时触发预定义的恢复动作。

### 1.1 核心职责

| 功能 | 描述 |
|------|------|
| Alive Supervision | 验证 AA 在规定周期内发出心跳（CheckpointReached） |
| Deadline Supervision | 验证两个 Checkpoint 之间的时间间隔在约束内 |
| Logical Supervision | 验证 Checkpoint 的到达顺序符合配置的转换图 |
| WDG 踢狗 | 软件看门狗：PHM 正常运行时定期踢 SOC 硬件 WDG |
| 故障恢复 | 检测到故障时触发：上报诊断 / 重启 AA / 触发 SM 状态切换 |

---

## 2. 架构设计

### 2.1 三级监督层次

```
┌────────────────────────────────────────────────────┐
│              ara::phm 监督架构                       │
│                                                    │
│  AA进程1: VehicleSignalService                      │
│    └─ SupervisedEntity("VehicleSignalService")     │
│         CheckpointReached(0x01) ──→ PHM 校验        │
│                                                    │
│  AA进程2: SafetyMonitorService                      │
│    └─ SupervisedEntity("SafetyMonitorService")     │
│         CheckpointReached(0x01) ──→ PHM 校验        │
│                                                    │
│  AA进程3: IPCBridgeService                          │
│    └─ SupervisedEntity("IPCBridgeService")         │
│         CheckpointReached(0x01) ──→ PHM 校验        │
│                          │                         │
│              ┌───────────▼────────────────────┐    │
│              │  PlatformHealthManagement       │    │
│              │  ├─ Alive Supervision Check     │    │
│              │  ├─ Deadline Supervision Check  │    │
│              │  ├─ Logical Supervision Check   │    │
│              │  ├─ GetGlobalStatus()           │    │
│              │  └─ KickWatchdog() → SOC HW WDG │    │
│              └────────────────────────────────┘    │
│                          │                         │
│              ┌───────────▼────────────────────┐    │
│              │  故障恢复动作                    │    │
│              │  kReportError → ara::diag       │    │
│              │  kRestartApp  → ara::exec       │    │
│              │  kTriggerSm   → ara::sm Error   │    │
│              │  kHardReset   → WDG 停止踢狗    │    │
│              └────────────────────────────────┘    │
└────────────────────────────────────────────────────┘
```

### 2.2 Alive Supervision 时序

```
时间轴（100ms 参考周期示例）：

|←─────────── 100ms 参考周期 ───────────→|
|   Alive     |   Alive     |   Alive     |   ← CheckpointReached()
|   Count=1   |   Count=1   |   Count=0   |   ← 第3周期缺失！
                                          ↓
                              failedCycles++（超过 failedSupervisionCyclesTol=3 后 → Failed）
```

---

## 3. 接口定义

### 3.1 SupervisedEntity

```cpp
class SupervisedEntity {
    // 构造时自动向 PHM 注册
    explicit SupervisedEntity(const std::string& instanceId);
    ~SupervisedEntity();  // 析构时自动注销

    // 上报检查点（核心接口，SWS_PHM_00061）
    void CheckpointReached(CheckpointId checkpointId);

    // 查询当前监督状态
    SupervisionStatus GetStatus() const;
};
```

### 3.2 PlatformHealthManagement（单例）

```cpp
class PlatformHealthManagement {
    static PlatformHealthManagement& GetInstance();

    // SE 注册/注销
    bool RegisterEntity(const std::string& entityId, const AliveSupervisionConfig&);
    void UnregisterEntity(const std::string& entityId);

    // 检查点上报（由 SupervisedEntity 内部调用）
    void ReportCheckpoint(const std::string& entityId, CheckpointId cpId);

    // 全局健康状态查询
    GlobalSupervisionStatus GetGlobalStatus() const;

    // WDG 踢狗（必须在 aliveReferenceCycleMs 内调用）
    void KickWatchdog();

    // 故障恢复配置
    void SetRecoveryCallback(RecoveryCallback cb);
    void SetPhmAction(const std::string& entityId, PhmAction action);

    void Start();
    void Stop();
};
```

### 3.3 配置结构

```cpp
struct AliveSupervisionConfig {
    uint32_t aliveReferenceCycleMs;         // 参考周期（ms）
    uint32_t minMargin;                     // 最小 Alive 次数容忍偏差
    uint32_t maxMargin;                     // 最大 Alive 次数容忍偏差
    uint32_t failedSupervisionCyclesTol;    // 容忍的连续失败周期数
};
```

---

## 4. 状态机

### 4.1 SE 监督状态机（SWS_PHM 标准）

```
               初始化
                 │
         ┌───────▼────────┐
         │  kDeactivated  │
         └───────┬────────┘
                 │ RegisterEntity() + Start()
         ┌───────▼────────┐
         │     kOk        │ ←──── CheckpointReached()（正常）────┐
         └───────┬────────┘                                      │
                 │ 连续 failedCycles > tolerance                  │
         ┌───────▼────────┐                                      │
         │    kFailed     │ ──── KickWatchdog() 停止 ──→ kExpired │
         └───────┬────────┘                                      │
                 │ 恢复动作执行完成（重启 AA 后）                    │
                 └───────────────────────────────────────────────┘
```

### 4.2 全局状态聚合规则

```
所有 SE 状态  →  GlobalSupervisionStatus

所有 kOk               → kNormal
至少一个 kFailed        → kFailed
至少一个 kExpired       → kExpired（最高优先级）
PHM Stop()            → kStopped
```

---

## 5. 与功能安全映射（FS-ARCH-001）

| 安全目标 | PHM 机制 | 违反动作 |
|---------|---------|---------|
| SG-01: 关键信号丢失 | VehicleSignalService Alive 监督（100ms） | kRestartApp |
| SG-02: 安全监控失效 | SafetyMonitorService Alive 监督（10ms） | kTriggerSm → Error |
| SG-03: IPC 通信中断 | IPCBridgeService Alive 监督（50ms） | kTriggerSm |
| SG-04: WDG 超时 | PHM.KickWatchdog() 50ms 周期 | 硬件 WDG 触发复位 |

---

## 6. SOC 本项目配置

| Supervised Entity | 参考周期 | maxMargin | 容忍失败周期 | 故障动作 |
|------------------|---------|-----------|------------|---------|
| VehicleSignalService | 100ms | 2 | 5 | kReportError |
| SafetyMonitorService | 10ms | 1 | 3 | kTriggerSm |
| IPCBridgeService | 50ms | 2 | 3 | kTriggerSm |

---

## 7. 单元测试要点

| 测试用例 | 验证点 |
|---------|--------|
| 注册/注销 SE | RegisterEntity 返回 true，重复注册返回 false |
| CheckpointReached | 正常上报后 status = kOk |
| GetGlobalStatus | 一个 SE Failed → Global = kFailed |
| KickWatchdog | 不抛异常 |
| SetPhmAction | 设置动作后可查询 |
