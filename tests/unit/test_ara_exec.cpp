/**
 * @file test_ara_exec.cpp
 * @brief ara::exec 单元测试
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Ref: AUTOSAR_SWS_ExecutionManagement (R25-11)
 */

#include <gtest/gtest.h>
#include "ara/exec/execution_client.h"

// -------------------------------------------------------
// ExecutionState 枚举测试
// -------------------------------------------------------
TEST(AraExecExecutionStateTest, KRunningValueIsZero) {
    EXPECT_EQ(static_cast<uint8_t>(ara::exec::ExecutionState::kRunning), 0u);
}

// -------------------------------------------------------
// ExecErrc 枚举测试
// -------------------------------------------------------
TEST(AraExecErrcTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::exec::ExecErrc::kSuccess),          0u);
    EXPECT_EQ(static_cast<uint8_t>(ara::exec::ExecErrc::kGeneralError),     1u);
    EXPECT_EQ(static_cast<uint8_t>(ara::exec::ExecErrc::kInvalidTransition),2u);
    EXPECT_EQ(static_cast<uint8_t>(ara::exec::ExecErrc::kAlreadyInState),   3u);
}

// -------------------------------------------------------
// ExecutionClient 构造/析构测试
// -------------------------------------------------------
TEST(AraExecClientTest, ConstructAndDestructDoesNotThrow) {
    EXPECT_NO_THROW({
        ara::exec::ExecutionClient client;
    });
}

// -------------------------------------------------------
// ReportExecutionState 测试
// -------------------------------------------------------
TEST(AraExecClientTest, ReportRunningReturnsSuccess) {
    ara::exec::ExecutionClient client;
    auto result = client.ReportExecutionState(ara::exec::ExecutionState::kRunning);
    EXPECT_EQ(result, ara::exec::ExecErrc::kSuccess);
}

TEST(AraExecClientTest, ReportExecutionStateIsNoexcept) {
    // 验证接口符合 AUTOSAR SWS 规定的 noexcept 约束
    ara::exec::ExecutionClient client;
    static_assert(
        noexcept(client.ReportExecutionState(ara::exec::ExecutionState::kRunning)),
        "ReportExecutionState must be noexcept per AUTOSAR SWS_EM"
    );
    SUCCEED();
}

TEST(AraExecClientTest, MultipleReportsReturnSuccess) {
    ara::exec::ExecutionClient client;
    // 连续上报，每次都应成功
    for (int i = 0; i < 5; ++i) {
        auto result = client.ReportExecutionState(ara::exec::ExecutionState::kRunning);
        EXPECT_EQ(result, ara::exec::ExecErrc::kSuccess) << "Failed on iteration " << i;
    }
}
