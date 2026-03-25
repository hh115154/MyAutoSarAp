/**
 * @file IPCBridgeService.cpp
 * @brief IPCBridgeService SWC 实现
 */

#include "IPCBridgeService.h"
#include "ara/phm/platform_health_management.h"
#include "ara/log/logger.h"
#include <thread>
#include <iostream>
#include <numeric>

namespace app {
namespace swc {

// ============================================================
// SpiIpcFrame CRC16
// ============================================================

uint16_t SpiIpcFrame::ComputeCRC16(const std::vector<uint8_t>& data)
{
    // CRC-16/CCITT-FALSE: poly=0x1021, init=0xFFFF
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data) {
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ============================================================
// IPCBridgeServiceSWC 实现
// ============================================================

IPCBridgeServiceSWC::IPCBridgeServiceSWC()
    : execClient_()
    , handshakeStartTime_(std::chrono::steady_clock::now())
    , lastHeartbeatTime_(std::chrono::steady_clock::now())
{}

IPCBridgeServiceSWC::~IPCBridgeServiceSWC()
{
    if (running_.load()) {
        Shutdown();
    }
}

bool IPCBridgeServiceSWC::Init()
{
    // 1. 初始化 NM 并请求 IPC 以太网网络
    ara::nm::NetworkManagement::GetInstance().Initialize();
    networkHandle_ = std::make_unique<ara::nm::NetworkHandle>(
        ara::nm::NetworkId::kEthIPC);

    if (!networkHandle_->IsValid()) {
        std::cerr << "[IPCBridge] Failed to request EthIPC network\n";
        return false;
    }

    // 2. 注册 PHM
    ara::phm::AliveSupervisionConfig conf{};
    conf.aliveReferenceCycleMs = 50; // 50ms Alive 周期
    conf.minMargin = 0;
    conf.maxMargin = 2;
    conf.failedSupervisionCyclesTol = 3;
    ara::phm::PlatformHealthManagement::GetInstance()
        .RegisterEntity("IPCBridgeService", conf);

    // 3. 初始化 GPIO（SOC_READY = HIGH，表示 SOC 启动完成）
    gpioState_[static_cast<int>(GpioSignal::kSocReady)] = true;
    gpioState_[static_cast<int>(GpioSignal::kSocPwrEn)] = true;

    // 4. 上报执行状态
    execClient_.ReportExecutionState(ara::exec::ExecutionState::kRunning);

    // 5. 进入握手状态
    TransitionState(IpcConnectionState::kHandshake);
    handshakeStartTime_ = std::chrono::steady_clock::now();

    return true;
}

void IPCBridgeServiceSWC::Run()
{
    running_.store(true);

    while (running_.load()) {
        // PHM Alive
        ara::phm::PlatformHealthManagement::GetInstance()
            .ReportCheckpoint("IPCBridgeService", 0x01);

        IpcConnectionState state = connectionState_.load();
        switch (state) {
            case IpcConnectionState::kHandshake:
                HandleHandshake();
                break;
            case IpcConnectionState::kNormal:
                HandleNormalOperation();
                break;
            case IpcConnectionState::kError:
                HandleError();
                break;
            default:
                break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void IPCBridgeServiceSWC::Shutdown()
{
    running_.store(false);
    StopIpcServices();
    // 释放网络（RAII，networkHandle_ 析构时自动释放）
    networkHandle_.reset();
    execClient_.ReportExecutionState(ara::exec::ExecutionState::kTerminating);
}

void IPCBridgeServiceSWC::HandleHandshake()
{
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - handshakeStartTime_).count();

    if (elapsed > kHandshakeTimeoutMs) {
        TransitionState(IpcConnectionState::kError);
        return;
    }

    // 模拟握手：发送 SOC_READY GPIO 后 100ms 视为握手成功
    if (elapsed > 100 && gpioState_[static_cast<int>(GpioSignal::kSocReady)]) {
        TransitionState(IpcConnectionState::kNormal);
        OfferIpcServices();
        lastHeartbeatTime_ = std::chrono::steady_clock::now();
    }
}

void IPCBridgeServiceSWC::HandleNormalOperation()
{
    // 检查心跳超时
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - lastHeartbeatTime_).count();

    if (elapsed > kHeartbeatTimeoutMs) {
        TransitionState(IpcConnectionState::kError);
        return;
    }

    // 模拟心跳刷新（实际由 MCU 周期消息触发）
    if (elapsed > kHeartbeatIntervalMs) {
        lastHeartbeatTime_ = std::chrono::steady_clock::now();
    }

    // 处理 SPI 发送队列
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!spiTxQueue_.empty()) {
            SpiIpcFrame& frame = spiTxQueue_.front();
            // 模拟 SPI 发送（生产替换为 Linux SPI dev ioctl）
            (void)frame;
            spiTxQueue_.pop();
        }
    }
}

void IPCBridgeServiceSWC::HandleError()
{
    // 尝试重置恢复
    TransitionState(IpcConnectionState::kReset);

    // 断言 SOC_RESET_N 低电平（复位 MCU）
    SetGpioSignal(GpioSignal::kSocResetN, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    SetGpioSignal(GpioSignal::kSocResetN, true);

    // 重新进入握手
    TransitionState(IpcConnectionState::kHandshake);
    handshakeStartTime_ = std::chrono::steady_clock::now();
}

void IPCBridgeServiceSWC::OfferIpcServices()
{
    // 注册 SOME/IP 服务目录中的所有 SOC 侧服务
    auto& sd = ara::com::someip::SomeIpServiceDiscovery::GetInstance();
    for (const auto& svc : ara::com::someip::kServiceCatalog) {
        sd.OfferService(svc);
    }
}

void IPCBridgeServiceSWC::StopIpcServices()
{
    auto& sd = ara::com::someip::SomeIpServiceDiscovery::GetInstance();
    for (const auto& svc : ara::com::someip::kServiceCatalog) {
        sd.StopOfferService(svc.serviceId);
    }
}

void IPCBridgeServiceSWC::TransitionState(IpcConnectionState newState)
{
    IpcConnectionState old = connectionState_.exchange(newState);
    if (old != newState && ipcStateCallback_) {
        ipcStateCallback_(old, newState);
    }
}

bool IPCBridgeServiceSWC::SendSomeIpMessage(const ara::com::someip::SomeIpMessage& msg)
{
    if (connectionState_.load() != IpcConnectionState::kNormal) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    ethTxQueue_.push(msg);
    return true;
}

bool IPCBridgeServiceSWC::SendSpiFrame(SpiIpcFrame& frame)
{
    // 填充 CRC
    std::vector<uint8_t> dataForCrc;
    dataForCrc.push_back(frame.sof);
    dataForCrc.push_back(frame.frameCtrl);
    dataForCrc.push_back(static_cast<uint8_t>(frame.length >> 8));
    dataForCrc.push_back(static_cast<uint8_t>(frame.length & 0xFF));
    dataForCrc.insert(dataForCrc.end(), frame.payload.begin(), frame.payload.end());
    frame.crc16 = SpiIpcFrame::ComputeCRC16(dataForCrc);

    std::lock_guard<std::mutex> lock(mutex_);
    spiTxQueue_.push(frame);
    return true;
}

void IPCBridgeServiceSWC::SetGpioSignal(GpioSignal signal, bool level)
{
    uint8_t idx = static_cast<uint8_t>(signal);
    if (idx >= 8) return;
    gpioState_[idx] = level;
    if (gpioCallback_) {
        gpioCallback_(signal, level);
    }
}

bool IPCBridgeServiceSWC::GetGpioSignal(GpioSignal signal) const
{
    uint8_t idx = static_cast<uint8_t>(signal);
    if (idx >= 8) return false;
    return gpioState_[idx];
}

void IPCBridgeServiceSWC::SetIpcStateCallback(IpcStateCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    ipcStateCallback_ = std::move(cb);
}

void IPCBridgeServiceSWC::SetSpiRxCallback(SpiRxCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    spiRxCallback_ = std::move(cb);
}

void IPCBridgeServiceSWC::SetGpioChangeCallback(GpioChangeCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    gpioCallback_ = std::move(cb);
}

} // namespace swc
} // namespace app
