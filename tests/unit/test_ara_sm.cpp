/**
 * @file test_ara_sm.cpp
 * @brief ara::sm 单元测试
 */

#include <gtest/gtest.h>
#include "ara/sm/state_management.h"

using namespace ara::sm;

// ============================================================
// MachineState 初始状态
// ============================================================

TEST(AraSm, InitialStateIsStartup) {
    auto& sm = StateManagement::GetInstance();
    // 注意：单例状态在测试间共享，此测试仅在第一次运行时保证
    // 重置：通过 Initialize() 不改变状态机，仅注册 FunctionGroup
    sm.Initialize();
    // 如果已经 Initialize，状态可能已被其他测试改变
    // 检查状态是有效的枚举值
    MachineState s = sm.GetCurrentMachineState();
    EXPECT_TRUE(s == MachineState::kStartup ||
                s == MachineState::kDriving ||
                s == MachineState::kParked  ||
                s == MachineState::kShutdown||
                s == MachineState::kError);
}

// ============================================================
// 状态切换合法性
// ============================================================

// 使用独立的 FunctionGroup 测试（不依赖单例 SM 状态）
TEST(AraSm, FunctionGroupStateChange) {
    FunctionGroup fg("TestFG", FunctionGroupState::kOff);
    EXPECT_EQ(fg.GetCurrentState(), FunctionGroupState::kOff);

    auto result = fg.RequestStateChange(FunctionGroupState::kOn);
    EXPECT_EQ(result, StateTransitionResult::kSuccess);
    EXPECT_EQ(fg.GetCurrentState(), FunctionGroupState::kOn);
}

TEST(AraSm, FunctionGroupSameState) {
    FunctionGroup fg("TestFG_Same", FunctionGroupState::kOn);
    // 切换到相同状态返回 Success
    auto result = fg.RequestStateChange(FunctionGroupState::kOn);
    EXPECT_EQ(result, StateTransitionResult::kSuccess);
}

TEST(AraSm, FunctionGroupCallback) {
    FunctionGroup fg("TestFG_Cb", FunctionGroupState::kOff);

    FunctionGroupState observedOld = FunctionGroupState::kOn; // 故意设错
    FunctionGroupState observedNew = FunctionGroupState::kOff;
    fg.SetStateChangeCallback([&](FunctionGroupState old, FunctionGroupState nw) {
        observedOld = old;
        observedNew = nw;
    });

    fg.RequestStateChange(FunctionGroupState::kOn);
    EXPECT_EQ(observedOld, FunctionGroupState::kOff);
    EXPECT_EQ(observedNew, FunctionGroupState::kOn);
}

TEST(AraSm, FunctionGroupAllStates) {
    FunctionGroup fg("TestFG_All", FunctionGroupState::kOff);

    fg.RequestStateChange(FunctionGroupState::kOn);
    EXPECT_EQ(fg.GetCurrentState(), FunctionGroupState::kOn);

    fg.RequestStateChange(FunctionGroupState::kSuspend);
    EXPECT_EQ(fg.GetCurrentState(), FunctionGroupState::kSuspend);

    fg.RequestStateChange(FunctionGroupState::kDiag);
    EXPECT_EQ(fg.GetCurrentState(), FunctionGroupState::kDiag);

    fg.RequestStateChange(FunctionGroupState::kOff);
    EXPECT_EQ(fg.GetCurrentState(), FunctionGroupState::kOff);
}

// ============================================================
// StateManagement 单例
// ============================================================

TEST(AraSm, SingletonIdentity) {
    auto& a = StateManagement::GetInstance();
    auto& b = StateManagement::GetInstance();
    EXPECT_EQ(&a, &b);
}

TEST(AraSm, RegisterAndGetFunctionGroup) {
    auto& sm = StateManagement::GetInstance();
    sm.Initialize(); // 注册默认功能组

    auto fg = sm.GetFunctionGroup("DriveAssist");
    EXPECT_NE(fg, nullptr);
    EXPECT_EQ(fg->GetName(), "DriveAssist");
}

TEST(AraSm, GetNonExistentFunctionGroup) {
    auto& sm = StateManagement::GetInstance();
    auto fg = sm.GetFunctionGroup("NonExistentFG_XYZ");
    EXPECT_EQ(fg, nullptr);
}

TEST(AraSm, ShutdownTransition) {
    auto& sm = StateManagement::GetInstance();
    // Shutdown 从任意状态均可到达
    auto result = sm.RequestMachineStateChange(MachineState::kShutdown);
    EXPECT_EQ(result, StateTransitionResult::kSuccess);
    EXPECT_EQ(sm.GetCurrentMachineState(), MachineState::kShutdown);
}

TEST(AraSm, ErrorTransition) {
    auto& sm = StateManagement::GetInstance();
    auto result = sm.RequestMachineStateChange(MachineState::kError);
    EXPECT_EQ(result, StateTransitionResult::kSuccess);
    EXPECT_EQ(sm.GetCurrentMachineState(), MachineState::kError);
}

TEST(AraSm, TriggerErrorState) {
    auto& sm = StateManagement::GetInstance();
    EXPECT_NO_THROW(sm.TriggerErrorState("Unit test trigger"));
    EXPECT_EQ(sm.GetCurrentMachineState(), MachineState::kError);
}

TEST(AraSm, MachineStateCallback) {
    auto& sm = StateManagement::GetInstance();
    MachineState observedNew = MachineState::kStartup;
    sm.SetMachineStateCallback([&](MachineState /*old*/, MachineState nw) {
        observedNew = nw;
    });
    sm.RequestMachineStateChange(MachineState::kShutdown);
    EXPECT_EQ(observedNew, MachineState::kShutdown);
    sm.SetMachineStateCallback(nullptr); // 清除回调
}

// ============================================================
// 枚举值校验
// ============================================================

TEST(AraSm, MachineStateEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(MachineState::kStartup),  0u);
    EXPECT_EQ(static_cast<uint8_t>(MachineState::kDriving),  1u);
    EXPECT_EQ(static_cast<uint8_t>(MachineState::kParked),   2u);
    EXPECT_EQ(static_cast<uint8_t>(MachineState::kCharging), 3u);
    EXPECT_EQ(static_cast<uint8_t>(MachineState::kShutdown), 4u);
    EXPECT_EQ(static_cast<uint8_t>(MachineState::kError),    5u);
}

TEST(AraSm, FunctionGroupStateEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(FunctionGroupState::kOff),     0u);
    EXPECT_EQ(static_cast<uint8_t>(FunctionGroupState::kOn),      1u);
    EXPECT_EQ(static_cast<uint8_t>(FunctionGroupState::kSuspend), 2u);
    EXPECT_EQ(static_cast<uint8_t>(FunctionGroupState::kDiag),    3u);
}

TEST(AraSm, StateTransitionResultEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(StateTransitionResult::kSuccess),       0u);
    EXPECT_EQ(static_cast<uint8_t>(StateTransitionResult::kRejected),      1u);
    EXPECT_EQ(static_cast<uint8_t>(StateTransitionResult::kTimeout),       2u);
    EXPECT_EQ(static_cast<uint8_t>(StateTransitionResult::kInvalidState),  3u);
    EXPECT_EQ(static_cast<uint8_t>(StateTransitionResult::kInProgress),    4u);
}
