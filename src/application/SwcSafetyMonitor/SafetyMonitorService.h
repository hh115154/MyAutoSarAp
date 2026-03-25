/**
 * @file SafetyMonitorService.h
 * @brief SafetyMonitorService SWC — 功能安全监控服务
 *
 * 对应 SOC-SW-001 §4.2 SafetyMonitor SWC
 * 对应 FS-ARCH-001 §3 安全监控机制
 *
 * 功能安全等级：ASIL-B（SOC VM1 分区）
 *
 * 功能：
 * 1. 订阅 SafetyStatusService（MCU→SOC，Service ID 0x1002）
 * 2. 监控 ara::phm 全局健康状态
 * 3. 实施安全状态降级（Level 0~4）
 * 4. 向 ara::sm 触发状态切换
 * 5. 定期踢 SOC WDG（通过 ara::phm）
 *
 * 安全状态定义（FS-ARCH-001 §4）：
 *   Level 0: 正常运行
 *   Level 1: 功能降级（非安全相关功能关闭）
 *   Level 2: 最小风险状态（仅保留安全告警）
 *   Level 3: 安全停机（受控关闭）
 *   Level 4: 紧急停机（立即关闭）
 */

#ifndef APP_SWC_SAFETY_MONITOR_SERVICE_H
#define APP_SWC_SAFETY_MONITOR_SERVICE_H

#include "ara/com/types.h"
#include "ara/com/proxy_base.h"
#include "ara/phm/platform_health_management.h"
#include "ara/sm/state_management.h"
#include "ara/exec/execution_client.h"
#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>

namespace app {
namespace swc {

// ============================================================
// 安全状态数据结构
// ============================================================

/**
 * @brief 安全状态级别（FS-ARCH-001 §4）
 */
enum class SafetyLevel : uint8_t {
    kLevel0_Normal    = 0,  ///< 正常运行
    kLevel1_Degraded  = 1,  ///< 功能降级
    kLevel2_MinRisk   = 2,  ///< 最小风险状态
    kLevel3_SafeStop  = 3,  ///< 安全停机
    kLevel4_Emergency = 4   ///< 紧急停机
};

/**
 * @brief MCU 安全状态报文（Service ID 0x1002）
 * 对应 IPC-ARCH-001 §2.4 SafetyStatusService
 */
struct McuSafetyStatus {
    SafetyLevel  safetyLevel;        ///< MCU 当前安全级别
    uint8_t      wdgStatus;          ///< MCU WDG 状态（0=OK, 1=接近超时, 2=复位中）
    bool         asil_dViolation;    ///< MCU ASIL-D 违反（触发 SAFE_STATE GPIO）
    uint8_t      faultCode;          ///< 故障码（低 4 位 = 模块，高 4 位 = 严重度）
    uint32_t     timestampMs;        ///< 时间戳
    uint8_t      e2eCounter;         ///< E2E Profile 2 计数器
};

/**
 * @brief SOC 安全状态快照（综合 PHM + MCU 状态）
 */
struct SocSafetySnapshot {
    SafetyLevel          socLevel;           ///< SOC 当前安全级别
    SafetyLevel          mcuLevel;           ///< MCU 最新报告安全级别
    SafetyLevel          systemLevel;        ///< 综合安全级别（取严重者）
    ara::phm::GlobalSupervisionStatus phmStatus; ///< PHM 全局状态
    uint32_t             lastWdgKickMs;      ///< 最后一次踢狗时间
    bool                 ipcConnected;       ///< IPC 连接状态
};

// ============================================================
// SafetyMonitorServiceSWC — 主 SWC 类
// ============================================================

/**
 * @brief SafetyMonitorService SWC（ASIL-B 安全监控）
 *
 * 生命周期：Init() → Run()（10ms 周期监控） → Shutdown()
 */
class SafetyMonitorServiceSWC {
public:
    using SafetyLevelCallback = std::function<void(SafetyLevel newLevel, const std::string& reason)>;

    SafetyMonitorServiceSWC();
    ~SafetyMonitorServiceSWC();

    /**
     * @brief 初始化（注册 PHM、订阅安全状态服务）
     */
    bool Init();

    /**
     * @brief 主监控循环（10ms 周期，ASIL-B 实时要求）
     */
    void Run();

    /**
     * @brief 关闭
     */
    void Shutdown();

    /**
     * @brief 获取当前安全快照
     */
    SocSafetySnapshot GetSafetySnapshot() const;

    /**
     * @brief 获取当前系统安全级别
     */
    SafetyLevel GetCurrentSafetyLevel() const { return currentLevel_.load(); }

    /**
     * @brief 注册安全级别变化通知
     */
    void SetSafetyLevelCallback(SafetyLevelCallback cb);

    /**
     * @brief 由 MCU 安全状态消息触发（IPC-ARCH-001 §2.4）
     */
    void OnMcuSafetyStatus(const McuSafetyStatus& status);

    bool IsRunning() const { return running_.load(); }

private:
    /**
     * @brief 执行周期性安全检查
     */
    void PeriodicSafetyCheck();

    /**
     * @brief 触发安全级别升级
     */
    void EscalateSafetyLevel(SafetyLevel newLevel, const std::string& reason);

    /**
     * @brief 执行安全级别对应的恢复动作
     */
    void ExecuteSafetyAction(SafetyLevel level);

    ara::exec::ExecutionClient execClient_;

    mutable std::mutex mutex_;
    std::atomic<SafetyLevel> currentLevel_{SafetyLevel::kLevel0_Normal};
    McuSafetyStatus lastMcuStatus_;
    SafetyLevelCallback safetyLevelCallback_;
    std::atomic<bool> running_{false};

    // WDG 踢狗追踪
    std::chrono::steady_clock::time_point lastWdgKick_;
    static constexpr uint32_t kWdgTimeoutMs = 50; ///< SOC WDG 超时 50ms

    // PHM 监督实体
    std::unique_ptr<ara::phm::SupervisedEntity> supervisedEntity_;
};

} // namespace swc
} // namespace app

#endif // APP_SWC_SAFETY_MONITOR_SERVICE_H
