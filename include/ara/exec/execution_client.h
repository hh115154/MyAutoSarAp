/**
 * @file execution_client.h
 * @brief ara::exec 执行管理客户端
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Execution Management
 * Ref: AUTOSAR_SWS_ExecutionManagement (R25-11)
 */

#ifndef ARA_EXEC_EXECUTION_CLIENT_H
#define ARA_EXEC_EXECUTION_CLIENT_H

#include <cstdint>

namespace ara {
namespace exec {

/**
 * @brief 执行状态枚举
 * 
 * Adaptive Application 向 Execution Management 上报的状态
 */
enum class ExecutionState : uint8_t {
    kRunning = 0  ///< 应用已完成初始化，进入运行状态
};

/**
 * @brief 执行错误码
 */
enum class ExecErrc : uint8_t {
    kSuccess = 0,
    kGeneralError = 1,
    kInvalidTransition = 2,
    kAlreadyInState = 3
};

/**
 * @brief 执行客户端
 *
 * 每个 Adaptive Application 通过此类向 Execution Management 上报状态
 *
 * @code
 * ara::exec::ExecutionClient exec_client;
 * exec_client.ReportExecutionState(ara::exec::ExecutionState::kRunning);
 * @endcode
 */
class ExecutionClient {
public:
    ExecutionClient();
    ~ExecutionClient();

    /**
     * @brief 上报执行状态
     * @param state 当前执行状态
     * @return ExecErrc 执行结果
     */
    ExecErrc ReportExecutionState(ExecutionState state) noexcept;
};

} // namespace exec
} // namespace ara

#endif // ARA_EXEC_EXECUTION_CLIENT_H
