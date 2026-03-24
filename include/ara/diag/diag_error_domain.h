/**
 * @file diag_error_domain.h
 * @brief ara::diag 诊断错误域
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Diagnostics
 * Ref: AUTOSAR_SWS_Diagnostics (R25-11)
 */

#ifndef ARA_DIAG_DIAG_ERROR_DOMAIN_H
#define ARA_DIAG_DIAG_ERROR_DOMAIN_H

#include <cstdint>
#include <string>

namespace ara {
namespace diag {

/**
 * @brief UDS 诊断会话类型
 */
enum class SessionControlType : uint8_t {
    kDefaultSession             = 0x01,
    kProgrammingSession         = 0x02,
    kExtendedDiagnosticSession  = 0x03
};

/**
 * @brief DTC 状态位掩码
 */
enum class DtcStatusBit : uint8_t {
    kTestFailed                     = 0x01,
    kTestFailedThisMonitoringCycle  = 0x02,
    kPendingDTC                     = 0x04,
    kConfirmedDTC                   = 0x08,
    kTestNotCompletedSinceLastClear  = 0x10,
    kTestFailedSinceLastClear       = 0x20,
    kTestNotCompletedThisMonitoringCycle = 0x40,
    kWarningIndicatorRequested      = 0x80
};

/**
 * @brief 诊断错误码
 */
enum class DiagErrc : uint8_t {
    kSuccess                  = 0x00,
    kConditionsNotCorrect     = 0x22,
    kRequestOutOfRange        = 0x31,
    kSecurityAccessDenied     = 0x33,
    kGeneralProgrammingFailure = 0x72
};

/**
 * @brief 诊断事件管理器接口
 *
 * 用于上报 DTC 状态
 *
 * @code
 * ara::diag::DiagnosticMonitor monitor("SensorFailure_DTC");
 * monitor.ReportMonitorAction(ara::diag::MonitorAction::kFailed);
 * @endcode
 */
enum class MonitorAction : uint8_t {
    kPassed           = 0x00,  ///< 监控通过
    kFailed           = 0x01,  ///< 监控失败
    kPrepassed        = 0x02,  ///< 预通过（Debounce）
    kPrefailed        = 0x03   ///< 预失败（Debounce）
};

class DiagnosticMonitor {
public:
    explicit DiagnosticMonitor(const std::string& event_id)
        : event_id_(event_id) {}

    /**
     * @brief 上报监控动作
     * @param action 监控结果
     * @return DiagErrc 执行结果
     */
    DiagErrc ReportMonitorAction(MonitorAction action) noexcept;

private:
    std::string event_id_;
};

} // namespace diag
} // namespace ara

#endif // ARA_DIAG_DIAG_ERROR_DOMAIN_H
