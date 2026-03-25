# ara::sm — State Management 详细设计文档

**文档编号**：DD-SM-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_StateManagement  
**关联架构文档**：SOC-SW-001 §3.6、FS-ARCH-001 §4  
**功能安全等级**：ASIL-B（状态切换必须有序，避免意外重启）

---

## 1. 功能概述

`ara::sm` 管理整个 AUTOSAR AP 平台的**运行状态**，包含两个层次：
1. **MachineState**：整机级状态（Startup/Driving/Parked/Charging/Shutdown/Error）
2. **FunctionGroup**：功能组级状态（DriveAssist/Infotainment/Diagnostic/OTA）

---

## 2. 状态机设计

### 2.1 MachineState 转换图（SOC-SW-001 §3.6.1）

```
                     ┌─────────┐
                     │ Startup │ ← 系统初始状态（T0）
                     └────┬────┘
                ┌─────────┤
         ≤2s   │          │ ≤2s
         ┌─────▼──┐   ┌───▼────┐
         │Driving │   │ Parked │
         └─┬──┬───┘   └───┬──┬─┘
           │  │           │  │
           │  └──── ← ────┘  │
           │  Parked←→Driving │
           │                  │
           │         ┌────────▼──┐
           │         │ Charging  │
           │         └────────┬──┘
           │                  │
        ┌──▼──────────────────▼──┐
        │        Shutdown        │ ← 任意状态均可 Shutdown
        └────────────────────────┘
                    │
        ┌───────────▼───────────┐
        │         Error         │ ← 任意状态均可进入（由 PHM 触发）
        └───────────────────────┘

箭头规则：
  - 实线 = 允许的显式转换
  - Shutdown / Error = 从任意状态可到达（最高优先级）
```

### 2.2 FunctionGroup 状态机

```
      ┌─────┐
      │ Off │ ← 初始状态（系统未就绪）
      └──┬──┘
         │ RequestStateChange(kOn)
      ┌──▼──┐       ┌─────────┐
      │ On  │ ──→   │ Suspend │ （低功耗/驻车）
      └──┬──┘       └────┬────┘
         │               │
         │◄──────────────┘ Resume
         │
         │ → kDiag（进入诊断模式，限制功能）
```

### 2.3 SOC 启动时序（SOC-SW-001 §3.8）

```
T0（上电复位）
  │
  ├─ T0+0ms    : Startup 状态进入
  ├─ T0+500ms  : ara::exec 初始化完成，AA 进程启动
  ├─ T0+1000ms : FunctionGroup DriveAssist kOn
  ├─ T0+2000ms : MachineState → Driving（T1 时间目标）✓ ≤2s
  └─ T0+6000ms : 所有 FunctionGroup 完全就绪（T2 时间目标）✓ ≤6s
```

---

## 3. 接口定义

### 3.1 FunctionGroup

```cpp
class FunctionGroup {
    explicit FunctionGroup(const std::string& name, FunctionGroupState initialState = kOff);

    StateTransitionResult RequestStateChange(FunctionGroupState targetState);
    FunctionGroupState GetCurrentState() const;
    const std::string& GetName() const;
    void SetStateChangeCallback(function<void(old, new)>);
};
```

### 3.2 StateManagement（单例）

```cpp
class StateManagement {
    static StateManagement& GetInstance();

    // 整机状态切换（SWS_SM_00010）
    StateTransitionResult RequestMachineStateChange(MachineState targetState);
    MachineState GetCurrentMachineState() const;

    // FunctionGroup 管理
    void RegisterFunctionGroup(shared_ptr<FunctionGroup>);
    shared_ptr<FunctionGroup> GetFunctionGroup(const string& name) const;

    // PHM 故障触发（SWS_SM_00080）
    void TriggerErrorState(const string& reason);

    // 初始化/关闭
    bool Initialize();
    void Shutdown();
};
```

---

## 4. 功能组配置（SOC-SW-001 §3.6.2）

| FunctionGroup | 描述 | 启动时序 | Parked 状态 | Error 状态 |
|--------------|------|---------|------------|-----------|
| DriveAssist | 驾驶辅助（ADAS/安全） | T1 ≤2s | On | Off |
| Infotainment | 信息娱乐（HMI/音乐） | T2 ≤6s | Suspend | Off |
| Diagnostic | UDS 诊断 | On 需 | On | On（保留诊断能力） |
| OTA | OTA 更新 | 按需激活 | Off | Off |

---

## 5. 与其他模块交互

```
ara::phm ──→ TriggerErrorState()    SM 切换到 Error 状态
ara::exec ──→ RegisterFunctionGroup() 进程启动时注册
SafetyMonitor SWC ──→ RequestMachineStateChange() 安全降级
OTA Client SWC ──→ GetFunctionGroup("OTA").RequestStateChange(kOn) 启动 OTA
```

---

## 6. 单元测试要点

| 测试用例 | 验证点 |
|---------|--------|
| 初始状态为 Startup | `GetCurrentMachineState() == kStartup` |
| Startup → Driving | `RequestMachineStateChange(kDriving) == kSuccess` |
| Driving → Charging（非法） | 返回 `kRejected` |
| 任意 → Shutdown | 允许（返回 kSuccess） |
| TriggerErrorState | 切换到 kError |
| FunctionGroup 注册查询 | GetFunctionGroup 返回正确对象 |
| FunctionGroup 状态切换 | RequestStateChange 触发回调 |
