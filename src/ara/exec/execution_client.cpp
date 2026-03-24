/**
 * @file execution_client.cpp
 * @brief ara::exec ExecutionClient 实现
 */

#include "ara/exec/execution_client.h"
#include <iostream>

namespace ara {
namespace exec {

ExecutionClient::ExecutionClient() {
    // 与 Execution Management 建立 IPC 连接
}

ExecutionClient::~ExecutionClient() {
    // 断开连接
}

ExecErrc ExecutionClient::ReportExecutionState(ExecutionState state) noexcept {
    // 实际实现中通过 IPC（Unix Domain Socket）向 EM 上报状态
    std::cout << "[ara::exec] ReportExecutionState: "
              << (state == ExecutionState::kRunning ? "kRunning" : "Unknown")
              << std::endl;
    return ExecErrc::kSuccess;
}

} // namespace exec
} // namespace ara
