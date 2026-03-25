/**
 * @file sm.cpp
 * @brief ara::sm — State Management 实现
 *
 * 实现 MachineState 状态机 + FunctionGroup 生命周期管理。
 * 
 * 合法状态转换矩阵（SOC-SW-001 §3.6.1）：
 *   Startup  → Driving, Parked
 *   Driving  → Parked, Shutdown, Error
 *   Parked   → Driving, Charging, Shutdown, Error
 *   Charging → Parked, Error
 *   任意     → Shutdown（关机优先），任意 → Error
 */

#include "ara/sm/state_management.h"
#include <iostream>
#include <stdexcept>

namespace ara {
namespace sm {

// ============================================================
// FunctionGroup 实现
// ============================================================

FunctionGroup::FunctionGroup(const std::string& name, FunctionGroupState initialState)
    : name_(name)
    , currentState_(initialState)
{}

StateTransitionResult FunctionGroup::RequestStateChange(FunctionGroupState targetState)
{
    std::lock_guard<std::mutex> lock(mutex_);

    FunctionGroupState oldState = currentState_.load();
    if (oldState == targetState) {
        return StateTransitionResult::kSuccess; // 已在目标状态
    }

    // 简化实现：允许所有 FunctionGroup 状态切换
    currentState_.store(targetState);

    if (stateChangeCallback_) {
        stateChangeCallback_(oldState, targetState);
    }
    return StateTransitionResult::kSuccess;
}

FunctionGroupState FunctionGroup::GetCurrentState() const
{
    return currentState_.load();
}

void FunctionGroup::SetStateChangeCallback(
    std::function<void(FunctionGroupState, FunctionGroupState)> cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    stateChangeCallback_ = std::move(cb);
}

// ============================================================
// StateManagement 实现
// ============================================================

StateManagement& StateManagement::GetInstance()
{
    static StateManagement instance;
    return instance;
}

bool StateManagement::IsValidTransition(MachineState from, MachineState to) const
{
    // Shutdown 优先级最高，任意状态均可 Shutdown
    if (to == MachineState::kShutdown) return true;
    // Error 可从任意状态进入
    if (to == MachineState::kError) return true;

    switch (from) {
        case MachineState::kStartup:
            return (to == MachineState::kDriving || to == MachineState::kParked);
        case MachineState::kDriving:
            return (to == MachineState::kParked);
        case MachineState::kParked:
            return (to == MachineState::kDriving || to == MachineState::kCharging);
        case MachineState::kCharging:
            return (to == MachineState::kParked);
        default:
            return false;
    }
}

StateTransitionResult StateManagement::RequestMachineStateChange(MachineState targetState)
{
    std::lock_guard<std::mutex> lock(mutex_);

    MachineState current = currentState_.load();

    if (current == targetState) {
        return StateTransitionResult::kSuccess;
    }

    if (!IsValidTransition(current, targetState)) {
        return StateTransitionResult::kRejected;
    }

    // 执行状态切换
    currentState_.store(targetState);

    if (machineStateCallback_) {
        machineStateCallback_(current, targetState);
    }

    return StateTransitionResult::kSuccess;
}

MachineState StateManagement::GetCurrentMachineState() const
{
    return currentState_.load();
}

void StateManagement::RegisterFunctionGroup(std::shared_ptr<FunctionGroup> fg)
{
    if (!fg) return;
    std::lock_guard<std::mutex> lock(mutex_);
    functionGroups_[fg->GetName()] = std::move(fg);
}

std::shared_ptr<FunctionGroup> StateManagement::GetFunctionGroup(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = functionGroups_.find(name);
    if (it != functionGroups_.end()) {
        return it->second;
    }
    return nullptr;
}

void StateManagement::SetMachineStateCallback(MachineStateCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    machineStateCallback_ = std::move(cb);
}

void StateManagement::TriggerErrorState(const std::string& reason)
{
    std::cerr << "[ara::sm] Error state triggered: " << reason << "\n";
    RequestMachineStateChange(MachineState::kError);
}

bool StateManagement::Initialize()
{
    // 注册默认功能组（与 SOC-SW-001 §3.6.2 对应）
    auto driveAssist = std::make_shared<FunctionGroup>("DriveAssist", FunctionGroupState::kOff);
    auto infotainment = std::make_shared<FunctionGroup>("Infotainment", FunctionGroupState::kOff);
    auto diagnostic   = std::make_shared<FunctionGroup>("Diagnostic",  FunctionGroupState::kOff);
    auto ota          = std::make_shared<FunctionGroup>("OTA",         FunctionGroupState::kOff);

    RegisterFunctionGroup(driveAssist);
    RegisterFunctionGroup(infotainment);
    RegisterFunctionGroup(diagnostic);
    RegisterFunctionGroup(ota);

    return true;
}

void StateManagement::Shutdown()
{
    RequestMachineStateChange(MachineState::kShutdown);
}

} // namespace sm
} // namespace ara
