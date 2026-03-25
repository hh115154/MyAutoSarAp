/**
 * @file platform_health_management.h
 * @brief ara::phm — Platform Health Management
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Platform Health Management
 * Ref: AUTOSAR_SWS_PlatformHealthManagement (R25-11)
 *
 * 设计依据：SOC-SW-001 §3.7 平台健康管理
 * 实现三级监督机制：Alive / Deadline / Logical
 */

#ifndef ARA_PHM_PLATFORM_HEALTH_MANAGEMENT_H
#define ARA_PHM_PLATFORM_HEALTH_MANAGEMENT_H

#include <cstdint>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace ara {
namespace phm {

// ============================================================
// 枚举类型定义
// ============================================================

/// 监督状态（对应 AUTOSAR SWS_PHM 各 supervision 状态机）
enum class SupervisionStatus : uint8_t {
    kOk           = 0,  ///< 监督正常
    kFailed       = 1,  ///< 监督失败（违反约束）
    kExpired      = 2,  ///< 超出 expiration 时间
    kStopped      = 3,  ///< 监督已停止
    kDeactivated  = 4   ///< 监督未激活
};

/// 全局监督状态（PHM 综合判断）
enum class GlobalSupervisionStatus : uint8_t {
    kNormal       = 0,  ///< 所有 SE 正常
    kFailed       = 1,  ///< 至少一个 SE 失败
    kExpired      = 2,  ///< 至少一个 SE 超期
    kStopped      = 3   ///< PHM 已停止
};

/// PHM 动作类型（当检测到故障时触发）
enum class PhmAction : uint8_t {
    kNone         = 0,  ///< 不采取动作
    kReportError  = 1,  ///< 上报错误到 ara::diag
    kRestartApp   = 2,  ///< 重启应用进程
    kTriggerSm    = 3,  ///< 触发 ara::sm 状态切换
    kHardReset    = 4   ///< 触发硬件复位（WDG 停止踢）
};

/// Alive 监督配置（SWS_PHM_00119）
struct AliveSupervisionConfig {
    uint32_t aliveReferenceCycleMs;  ///< 参考周期（ms）
    uint32_t minMargin;              ///< 最小 Alive 次数容忍偏差
    uint32_t maxMargin;              ///< 最大 Alive 次数容忍偏差
    uint32_t failedSupervisionCyclesTol; ///< 容忍的连续失败周期数
};

/// Deadline 监督配置（SWS_PHM_00130）
struct DeadlineSupervisionConfig {
    uint32_t minDeadlineMs;  ///< 最小允许时间间隔（ms）
    uint32_t maxDeadlineMs;  ///< 最大允许时间间隔（ms）
};

/// Logical 监督配置（SWS_PHM_00140）
struct LogicalSupervisionConfig {
    uint32_t checkpointCount;  ///< Checkpoint 总数
    std::string transitionGraph; ///< Checkpoint 转换图（JSON 序列化）
};

// ============================================================
// CheckpointReached — 受监督实体的检查点上报接口
// ============================================================

/**
 * @brief 检查点 ID 类型
 * 每个 Supervised Entity 有一组预定义的 Checkpoint ID
 */
using CheckpointId = uint32_t;

/**
 * @brief 受监督实体（Supervised Entity）
 *
 * 每个 AA 进程在初始化时向 PHM 注册一组 SE，
 * 运行时通过 CheckpointReached() 上报状态点。
 *
 * 对应 SWS_PHM_00050: SupervisedEntity class
 */
class SupervisedEntity {
public:
    /**
     * @brief 构造 SE 并注册到 PHM
     * @param instanceId  SE 的 Instance Specifier（唯一名称）
     */
    explicit SupervisedEntity(const std::string& instanceId);

    ~SupervisedEntity();

    /**
     * @brief 上报到达检查点（SWS_PHM_00061）
     * @param checkpointId  检查点 ID（由 ARXML 配置定义）
     *
     * PHM 将根据配置的 supervision 类型校验：
     * - Alive：周期内 Checkpoint 到达次数
     * - Deadline：两个 Checkpoint 之间的时间间隔
     * - Logical：Checkpoint 的到达顺序/转换图
     */
    void CheckpointReached(CheckpointId checkpointId);

    /**
     * @brief 获取当前 SE 监督状态
     */
    SupervisionStatus GetStatus() const;

    /**
     * @brief 获取 SE 实例 ID
     */
    const std::string& GetInstanceId() const { return instanceId_; }

private:
    std::string instanceId_;
    mutable std::mutex mutex_;
    std::atomic<SupervisionStatus> status_{SupervisionStatus::kOk};
    std::chrono::steady_clock::time_point lastCheckpointTime_;
    uint32_t checkpointCount_{0};
};

// ============================================================
// PlatformHealthManagement — PHM 主控类
// ============================================================

/**
 * @brief PHM 主控类（单例模式）
 *
 * 提供：
 * 1. SE 注册 / 注销
 * 2. 全局健康状态查询
 * 3. WDG 踢狗接口（防止硬件看门狗超时）
 * 4. 故障恢复动作回调
 *
 * 对应 SOC-SW-001 §3.7：
 *   - Alive 监督：每 10ms 检查一次心跳
 *   - Deadline 监督：关键事件序列时间约束
 *   - Logical 监督：安全状态转换顺序校验
 */
class PlatformHealthManagement {
public:
    /// 故障处理回调
    using RecoveryCallback = std::function<void(const std::string& entityId, SupervisionStatus status)>;

    /**
     * @brief 获取 PHM 单例实例
     */
    static PlatformHealthManagement& GetInstance();

    // 禁止拷贝和移动
    PlatformHealthManagement(const PlatformHealthManagement&) = delete;
    PlatformHealthManagement& operator=(const PlatformHealthManagement&) = delete;

    /**
     * @brief 注册受监督实体
     * @param entityId   实体唯一名称
     * @param aliveConf  Alive 监督配置（可选）
     * @return           注册是否成功
     */
    bool RegisterEntity(const std::string& entityId,
                        const AliveSupervisionConfig& aliveConf);

    /**
     * @brief 注销受监督实体
     */
    void UnregisterEntity(const std::string& entityId);

    /**
     * @brief 上报实体检查点（由 SupervisedEntity 内部调用）
     */
    void ReportCheckpoint(const std::string& entityId, CheckpointId cpId);

    /**
     * @brief 获取全局监督状态
     * 遍历所有注册 SE，取最严重状态
     */
    GlobalSupervisionStatus GetGlobalStatus() const;

    /**
     * @brief 踢 WDG（软件看门狗）
     * 必须在 aliveReferenceCycleMs 内调用，否则触发硬件复位
     * 对应 SOC-SW-001：ara::phm 负责 SOC WDG 看门狗踢狗
     */
    void KickWatchdog();

    /**
     * @brief 注册故障恢复回调
     */
    void SetRecoveryCallback(RecoveryCallback cb);

    /**
     * @brief 设置 PHM 动作（当检测到故障时）
     */
    void SetPhmAction(const std::string& entityId, PhmAction action);

    /**
     * @brief PHM 运行状态
     */
    bool IsRunning() const { return running_.load(); }

    /**
     * @brief 启动 PHM（开始监督检查）
     */
    void Start();

    /**
     * @brief 停止 PHM
     */
    void Stop();

private:
    PlatformHealthManagement() = default;

    struct EntityRecord {
        AliveSupervisionConfig aliveConf;
        std::atomic<SupervisionStatus> status{SupervisionStatus::kOk};
        std::chrono::steady_clock::time_point lastCheckpointTime;
        uint32_t aliveCountInCycle{0};
        PhmAction action{PhmAction::kReportError};
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, EntityRecord> entities_;
    RecoveryCallback recoveryCallback_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point lastWdgKick_;
};

} // namespace phm
} // namespace ara

#endif // ARA_PHM_PLATFORM_HEALTH_MANAGEMENT_H
