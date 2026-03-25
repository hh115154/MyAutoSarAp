/**
 * @file state_management.h
 * @brief ara::sm — State Management
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: State Management
 * Ref: AUTOSAR_SWS_StateManagement (R25-11)
 *
 * 设计依据：SOC-SW-001 §3.6 状态管理
 * 实现 MachineState + FunctionGroup 两级状态机
 */

#ifndef ARA_SM_STATE_MANAGEMENT_H
#define ARA_SM_STATE_MANAGEMENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace ara {
namespace sm {

// ============================================================
// 枚举定义
// ============================================================

/**
 * @brief 整机状态（MachineState）
 *
 * 对应 SOC-SW-001 §3.6.1 MachineState 状态机：
 *   Startup → Driving ↔ Parked ↔ Charging → Shutdown
 *   任意状态 → Error（故障降级）
 */
enum class MachineState : uint8_t {
    kStartup   = 0,  ///< 系统启动（初始化中）
    kDriving   = 1,  ///< 行驶模式（ASIL-B 安全功能全开）
    kParked    = 2,  ///< 驻车模式（部分功能关闭）
    kCharging  = 3,  ///< 充电模式
    kShutdown  = 4,  ///< 关机序列
    kError     = 5   ///< 错误状态（故障降级）
};

/**
 * @brief FunctionGroup 状态
 *
 * 对应 SOC-SW-001 §3.6.2 FunctionGroup 状态枚举
 */
enum class FunctionGroupState : uint8_t {
    kOff       = 0,  ///< 功能组关闭（所有 AA 停止）
    kOn        = 1,  ///< 功能组运行
    kSuspend   = 2,  ///< 功能组挂起（低功耗）
    kDiag      = 3   ///< 诊断模式
};

/**
 * @brief 状态切换请求结果
 */
enum class StateTransitionResult : uint8_t {
    kSuccess          = 0,
    kRejected         = 1,  ///< 当前状态不允许该切换
    kTimeout          = 2,  ///< 等待 AA 响应超时
    kInvalidState     = 3,  ///< 目标状态无效
    kInProgress       = 4   ///< 切换正在进行中
};

// ============================================================
// FunctionGroup — 功能组管理
// ============================================================

/**
 * @brief 功能组（FunctionGroup）
 *
 * 代表一组相关 AA 的集合，具有独立的生命周期状态。
 * 对应 SOC-SW-001 功能组划分：
 *   - DriveAssist（驾驶辅助）
 *   - Infotainment（信息娱乐）
 *   - Diagnostic（诊断）
 *   - OTA（OTA 更新）
 */
class FunctionGroup {
public:
    /**
     * @brief 构造功能组
     * @param name         功能组名称（与 ARXML 配置一致）
     * @param initialState 初始状态
     */
    explicit FunctionGroup(const std::string& name,
                           FunctionGroupState initialState = FunctionGroupState::kOff);

    ~FunctionGroup() = default;

    /**
     * @brief 请求功能组状态切换（SWS_SM_00040）
     * @param targetState 目标状态
     * @return            切换结果
     *
     * 切换时 SM 会通知组内所有 AA 进行状态确认。
     */
    StateTransitionResult RequestStateChange(FunctionGroupState targetState);

    /**
     * @brief 获取当前功能组状态
     */
    FunctionGroupState GetCurrentState() const;

    /**
     * @brief 获取功能组名称
     */
    const std::string& GetName() const { return name_; }

    /**
     * @brief 注册状态变化通知回调
     */
    void SetStateChangeCallback(
        std::function<void(FunctionGroupState oldState, FunctionGroupState newState)> cb);

private:
    std::string name_;
    std::atomic<FunctionGroupState> currentState_;
    std::function<void(FunctionGroupState, FunctionGroupState)> stateChangeCallback_;
    mutable std::mutex mutex_;
};

// ============================================================
// StateManagement — SM 主控类
// ============================================================

/**
 * @brief 状态管理主控类（单例）
 *
 * 职责：
 * 1. 管理整机 MachineState 状态机
 * 2. 管理所有 FunctionGroup 的生命周期
 * 3. 响应 ara::phm 的故障触发（进入 Error 状态）
 * 4. 协调 AA 进程的启动 / 停止顺序
 *
 * 对应 SOC-SW-001 §3.6 启动流程：
 *   T0 → T1 (≤2s): Startup 完成，切换到 Driving/Parked
 *   T0 → T2 (≤6s): 完整功能就绪
 */
class StateManagement {
public:
    /// 机器状态变化通知回调
    using MachineStateCallback = std::function<void(MachineState oldState, MachineState newState)>;

    /**
     * @brief 获取 SM 单例
     */
    static StateManagement& GetInstance();

    StateManagement(const StateManagement&) = delete;
    StateManagement& operator=(const StateManagement&) = delete;

    /**
     * @brief 请求整机状态切换（SWS_SM_00010）
     * @param targetState 目标 MachineState
     * @return            切换结果
     *
     * 合法转换（SOC-SW-001 §3.6.1）：
     *   Startup → Driving, Parked
     *   Driving → Parked, Shutdown, Error
     *   Parked  → Driving, Charging, Shutdown, Error
     *   Charging → Parked, Error
     *   任意 → Shutdown（关机优先）
     */
    StateTransitionResult RequestMachineStateChange(MachineState targetState);

    /**
     * @brief 获取当前整机状态
     */
    MachineState GetCurrentMachineState() const;

    /**
     * @brief 注册 FunctionGroup（由 Manifest 解析后调用）
     */
    void RegisterFunctionGroup(std::shared_ptr<FunctionGroup> fg);

    /**
     * @brief 获取指定功能组
     * @return  nullptr 如果不存在
     */
    std::shared_ptr<FunctionGroup> GetFunctionGroup(const std::string& name) const;

    /**
     * @brief 注册整机状态变化回调
     */
    void SetMachineStateCallback(MachineStateCallback cb);

    /**
     * @brief 由 PHM 故障触发进入错误状态（SWS_SM_00080）
     */
    void TriggerErrorState(const std::string& reason);

    /**
     * @brief 初始化 SM（加载 Manifest 配置）
     */
    bool Initialize();

    /**
     * @brief 关闭 SM
     */
    void Shutdown();

private:
    StateManagement() = default;

    bool IsValidTransition(MachineState from, MachineState to) const;

    std::atomic<MachineState> currentState_{MachineState::kStartup};
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<FunctionGroup>> functionGroups_;
    MachineStateCallback machineStateCallback_;
};

} // namespace sm
} // namespace ara

#endif // ARA_SM_STATE_MANAGEMENT_H
