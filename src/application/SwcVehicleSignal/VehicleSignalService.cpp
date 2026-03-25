/**
 * @file VehicleSignalService.cpp
 * @brief VehicleSignalService SWC 实现
 */

#include "VehicleSignalService.h"
#include "ara/phm/platform_health_management.h"
#include "ara/log/logger.h"
#include <iostream>

namespace app {
namespace swc {

// ============================================================
// VehicleSignalServiceProxy 实现
// ============================================================

void VehicleSignalServiceProxy::SubscribeVehicleSignals(SignalUpdateCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(cb);
    subscriptionState_ = ara::com::SubscriptionState::kSubscribed;

    // 注册 SOME/IP SD 服务可用性监听
    ara::com::someip::SomeIpServiceDiscovery::GetInstance()
        .SubscribeServiceAvailability(0x1001,
            [this](const ara::com::someip::ServiceDescriptor& svc, bool available) {
                (void)svc;
                if (available) {
                    // 服务上线：发布模拟初始信号（生产替换为真实网络接收）
                    VehicleSignals initSignals;
                    initSignals.vehicleSpeedKph = 0.0f;
                    initSignals.batteryVoltage  = 12.6f;
                    initSignals.ignitionOn      = true;
                    std::lock_guard<std::mutex> lock2(mutex_);
                    latestSignals_ = initSignals;
                    if (callback_) callback_(initSignals);
                }
            });
}

void VehicleSignalServiceProxy::UnsubscribeVehicleSignals()
{
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptionState_ = ara::com::SubscriptionState::kNotSubscribed;
    callback_ = nullptr;
}

VehicleSignals VehicleSignalServiceProxy::GetCurrentSignals() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latestSignals_;
}

// ============================================================
// VehicleSignalServiceSWC 实现
// ============================================================

VehicleSignalServiceSWC::VehicleSignalServiceSWC()
    : proxy_(std::make_unique<VehicleSignalServiceProxy>(
          ara::com::InstanceIdentifier("VehicleSignalService_0x1001")))
    , execClient_()
{}

VehicleSignalServiceSWC::~VehicleSignalServiceSWC()
{
    if (running_.load()) {
        Shutdown();
    }
}

bool VehicleSignalServiceSWC::Init()
{
    // 1. 向 PHM 注册受监督实体（Alive 监督，100ms 周期）
    ara::phm::AliveSupervisionConfig aliveConf{};
    aliveConf.aliveReferenceCycleMs       = 100;
    aliveConf.minMargin                   = 0;
    aliveConf.maxMargin                   = 2;
    aliveConf.failedSupervisionCyclesTol  = 5;
    ara::phm::PlatformHealthManagement::GetInstance()
        .RegisterEntity("VehicleSignalService", aliveConf);

    // 2. 上报执行状态
    execClient_.ReportExecutionState(ara::exec::ExecutionState::kRunning);

    // 3. 订阅 SOME/IP 服务（Service ID 0x1001）
    proxy_->SubscribeVehicleSignals(
        [this](const VehicleSignals& sig) { OnSignalReceived(sig); });

    return true;
}

void VehicleSignalServiceSWC::Run()
{
    running_.store(true);

    // 主循环：每 100ms 踢 PHM Alive 检查点
    while (running_.load()) {
        ara::phm::PlatformHealthManagement::GetInstance()
            .ReportCheckpoint("VehicleSignalService", 0x01);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void VehicleSignalServiceSWC::Shutdown()
{
    running_.store(false);
    proxy_->UnsubscribeVehicleSignals();
    execClient_.ReportExecutionState(ara::exec::ExecutionState::kTerminating);
}

VehicleSignals VehicleSignalServiceSWC::GetLatestSignals() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latestSignals_;
}

void VehicleSignalServiceSWC::SetSignalChangeCallback(
    VehicleSignalServiceProxy::SignalUpdateCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    externalCallback_ = std::move(cb);
}

void VehicleSignalServiceSWC::OnSignalReceived(const VehicleSignals& signals)
{
    // E2E 校验
    if (!ValidateE2E(signals)) {
        e2eErrorCount_++;
        if (e2eErrorCount_ > 3) {
            ara::phm::PlatformHealthManagement::GetInstance()
                .ReportCheckpoint("VehicleSignalService", 0xFF); // 故障检查点
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        latestSignals_ = signals;
    }

    // 通知内部订阅者
    if (externalCallback_) {
        externalCallback_(signals);
    }
}

bool VehicleSignalServiceSWC::ValidateE2E(const VehicleSignals& signals)
{
    // E2E Profile 2 简化校验：检查计数器连续性
    uint8_t expectedCounter = static_cast<uint8_t>(lastE2ECounter_ + 1);
    if (signals.e2eCounterValue != expectedCounter &&
        lastE2ECounter_ != 0 /* 第一帧豁免 */)
    {
        return false; // 序列号不连续
    }
    lastE2ECounter_ = signals.e2eCounterValue;
    return true;
}

} // namespace swc
} // namespace app
