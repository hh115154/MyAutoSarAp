/**
 * @file SafetyMonitorService.cpp
 * @brief SafetyMonitorService SWC 实现（ASIL-B）
 */

#include "SafetyMonitorService.h"
#include "ara/log/logger.h"
#include <iostream>
#include <thread>

namespace app {
namespace swc {

SafetyMonitorServiceSWC::SafetyMonitorServiceSWC()
    : execClient_()
    , lastWdgKick_(std::chrono::steady_clock::now())
{}

SafetyMonitorServiceSWC::~SafetyMonitorServiceSWC()
{
    if (running_.load()) {
        Shutdown();
    }
}

bool SafetyMonitorServiceSWC::Init()
{
    // 1. 向 PHM 注册（Alive 监督，10ms 周期）
    supervisedEntity_ = std::make_unique<ara::phm::SupervisedEntity>("SafetyMonitorService");

    // 2. 配置 PHM 故障恢复动作
    ara::phm::PlatformHealthManagement::GetInstance()
        .SetPhmAction("SafetyMonitorService", ara::phm::PhmAction::kTriggerSm);

    // 3. 初始化 SM FunctionGroup
    ara::sm::StateManagement::GetInstance().Initialize();

    // 4. 上报执行状态
    execClient_.ReportExecutionState(ara::exec::ExecutionState::kRunning);

    // 5. 启动 PHM
    ara::phm::PlatformHealthManagement::GetInstance().Start();

    return true;
}

void SafetyMonitorServiceSWC::Run()
{
    running_.store(true);

    while (running_.load()) {
        auto loopStart = std::chrono::steady_clock::now();

        // 10ms 周期安全检查
        PeriodicSafetyCheck();

        // 计算剩余等待时间
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - loopStart);
        auto remaining = std::chrono::milliseconds(10) - elapsed;
        if (remaining.count() > 0) {
            std::this_thread::sleep_for(remaining);
        }
    }
}

void SafetyMonitorServiceSWC::Shutdown()
{
    running_.store(false);
    EscalateSafetyLevel(SafetyLevel::kLevel3_SafeStop, "Planned shutdown");
    execClient_.ReportExecutionState(ara::exec::ExecutionState::kTerminating);
    ara::phm::PlatformHealthManagement::GetInstance().Stop();
}

void SafetyMonitorServiceSWC::PeriodicSafetyCheck()
{
    // 1. 踢 SOC WDG（在 kWdgTimeoutMs 内必须调用）
    ara::phm::PlatformHealthManagement::GetInstance().KickWatchdog();
    supervisedEntity_->CheckpointReached(0x01); // Alive checkpoint
    lastWdgKick_ = std::chrono::steady_clock::now();

    // 2. 获取 PHM 全局状态
    auto phmStatus = ara::phm::PlatformHealthManagement::GetInstance().GetGlobalStatus();

    if (phmStatus == ara::phm::GlobalSupervisionStatus::kExpired) {
        EscalateSafetyLevel(SafetyLevel::kLevel3_SafeStop, "PHM supervision expired");
        return;
    }
    if (phmStatus == ara::phm::GlobalSupervisionStatus::kFailed) {
        EscalateSafetyLevel(SafetyLevel::kLevel2_MinRisk, "PHM supervision failed");
        return;
    }

    // 3. 检查 MCU 安全状态
    McuSafetyStatus mcuStatus;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mcuStatus = lastMcuStatus_;
    }

    if (mcuStatus.asil_dViolation) {
        EscalateSafetyLevel(SafetyLevel::kLevel4_Emergency, "MCU ASIL-D violation");
        return;
    }
    if (mcuStatus.safetyLevel >= SafetyLevel::kLevel2_MinRisk) {
        EscalateSafetyLevel(mcuStatus.safetyLevel, "MCU safety escalation");
    }
}

void SafetyMonitorServiceSWC::EscalateSafetyLevel(
    SafetyLevel newLevel, const std::string& reason)
{
    SafetyLevel current = currentLevel_.load();
    if (static_cast<uint8_t>(newLevel) <= static_cast<uint8_t>(current)) {
        return; // 已在更严重或相同状态，不降级
    }

    currentLevel_.store(newLevel);
    ExecuteSafetyAction(newLevel);

    if (safetyLevelCallback_) {
        safetyLevelCallback_(newLevel, reason);
    }
}

void SafetyMonitorServiceSWC::ExecuteSafetyAction(SafetyLevel level)
{
    auto& sm = ara::sm::StateManagement::GetInstance();

    switch (level) {
        case SafetyLevel::kLevel1_Degraded:
            // 关闭信息娱乐功能组
            {
                auto fg = sm.GetFunctionGroup("Infotainment");
                if (fg) fg->RequestStateChange(ara::sm::FunctionGroupState::kOff);
            }
            break;

        case SafetyLevel::kLevel2_MinRisk:
            // 关闭所有非安全功能，进入 Parked
            {
                auto fg1 = sm.GetFunctionGroup("Infotainment");
                auto fg2 = sm.GetFunctionGroup("OTA");
                if (fg1) fg1->RequestStateChange(ara::sm::FunctionGroupState::kOff);
                if (fg2) fg2->RequestStateChange(ara::sm::FunctionGroupState::kOff);
                sm.RequestMachineStateChange(ara::sm::MachineState::kParked);
            }
            break;

        case SafetyLevel::kLevel3_SafeStop:
        case SafetyLevel::kLevel4_Emergency:
            // 触发受控关机
            sm.TriggerErrorState("Safety level " + std::to_string(static_cast<int>(level)));
            sm.RequestMachineStateChange(ara::sm::MachineState::kShutdown);
            break;

        default:
            break;
    }
}

void SafetyMonitorServiceSWC::OnMcuSafetyStatus(const McuSafetyStatus& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lastMcuStatus_ = status;
}

SocSafetySnapshot SafetyMonitorServiceSWC::GetSafetySnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    SocSafetySnapshot snap;
    snap.socLevel   = currentLevel_.load();
    snap.mcuLevel   = lastMcuStatus_.safetyLevel;
    snap.systemLevel = std::max(snap.socLevel, snap.mcuLevel);
    snap.phmStatus  = ara::phm::PlatformHealthManagement::GetInstance().GetGlobalStatus();
    snap.ipcConnected = true; // 简化：实际检查 ETH 连接
    return snap;
}

void SafetyMonitorServiceSWC::SetSafetyLevelCallback(SafetyLevelCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    safetyLevelCallback_ = std::move(cb);
}

} // namespace swc
} // namespace app
