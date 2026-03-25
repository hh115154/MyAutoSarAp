/**
 * @file IPCBridgeService.h
 * @brief IPCBridgeService SWC — 芯片间通信桥接服务
 *
 * 对应 SOC-SW-001 §4.4 IPCBridge SWC
 * 对应 IPC-ARCH-001 §1.3 通道分配策略
 *
 * 功能：
 * 1. 管理 SOC↔MCU 三条通信通道（ETH/SPI/GPIO）
 * 2. 维护 IPC 连接状态机（OFFLINE→HANDSHAKE→NORMAL→ERROR→RESET）
 * 3. 路由 SOME/IP 消息（ara::com → ETH → MCU SOME/IP）
 * 4. 模拟 MICROSAR.IPC over SPI（低时延控制信号）
 * 5. GPIO 信号管理（复位/唤醒/安全状态）
 *
 * IPC 状态机（IPC-ARCH-001 §5）：
 *   OFFLINE → HANDSHAKE → NORMAL → ERROR → RESET → OFFLINE
 *             ↓ 成功                ↑ 故障         ↑ 复位完成
 */

#ifndef APP_SWC_IPC_BRIDGE_SERVICE_H
#define APP_SWC_IPC_BRIDGE_SERVICE_H

#include "ara/com/types.h"
#include "ara/com/someip_binding.h"
#include "ara/nm/network_management.h"
#include "ara/exec/execution_client.h"
#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>
#include <chrono>

namespace app {
namespace swc {

// ============================================================
// IPC 连接状态机
// ============================================================

/**
 * @brief IPC 连接状态（IPC-ARCH-001 §5.1）
 */
enum class IpcConnectionState : uint8_t {
    kOffline     = 0,  ///< 未连接（SOC 启动中或 MCU 掉线）
    kHandshake   = 1,  ///< 握手中（等待 MCU ACK）
    kNormal      = 2,  ///< 正常通信
    kError       = 3,  ///< 通信错误（超时/CRC 失败）
    kReset       = 4   ///< 复位恢复中
};

/**
 * @brief GPIO 信号定义（IPC-ARCH-001 §4 GPIO x8）
 */
enum class GpioSignal : uint8_t {
    kSocResetN   = 0,  ///< SOC_RESET_N：SOC 复位 MCU（低电平有效）
    kSocPwrEn    = 1,  ///< SOC_PWREN：SOC 电源使能（SOC→MCU）
    kSocReady    = 2,  ///< SOC_READY：SOC 就绪信号（SOC→MCU）
    kWdgKick     = 3,  ///< WDG_KICK：软件踢狗（SOC→MCU，1ms 周期）
    kWakeReq     = 4,  ///< WAKE_REQ：唤醒请求（双向）
    kSleepAck    = 5,  ///< SLEEP_ACK：休眠确认（MCU→SOC）
    kSafeState   = 6,  ///< SAFE_STATE：安全状态（MCU→SOC，高=ASIL-D 违反）
    kIrqSpi      = 7   ///< IRQ_SPI：SPI 数据就绪中断（MCU→SOC）
};

/**
 * @brief SPI MICROSAR.IPC 帧结构（IPC-ARCH-001 §3.2）
 */
struct SpiIpcFrame {
    uint8_t  sof;          ///< 帧起始字节 = 0xA5
    uint8_t  frameCtrl;    ///< 帧控制（类型/优先级）
    uint16_t length;       ///< Payload 长度
    std::vector<uint8_t> payload; ///< 数据载荷
    uint16_t crc16;        ///< CRC-16/CCITT 校验

    static constexpr uint8_t kSof = 0xA5;

    /**
     * @brief 计算帧 CRC16（CRC-16/CCITT-FALSE）
     */
    static uint16_t ComputeCRC16(const std::vector<uint8_t>& data);
};

// ============================================================
// IPCBridgeServiceSWC — 主 SWC 类
// ============================================================

/**
 * @brief IPCBridgeService SWC
 *
 * 生命周期：Init() → Run()（事件驱动） → Shutdown()
 */
class IPCBridgeServiceSWC {
public:
    using IpcStateCallback = std::function<void(IpcConnectionState oldState, IpcConnectionState newState)>;
    using GpioChangeCallback = std::function<void(GpioSignal signal, bool level)>;
    using SpiRxCallback = std::function<void(const SpiIpcFrame& frame)>;

    IPCBridgeServiceSWC();
    ~IPCBridgeServiceSWC();

    /**
     * @brief 初始化 IPC 通道
     */
    bool Init();

    /**
     * @brief 主循环（事件驱动，1ms 精度）
     */
    void Run();

    /**
     * @brief 关闭
     */
    void Shutdown();

    /**
     * @brief 获取 IPC 连接状态
     */
    IpcConnectionState GetConnectionState() const { return connectionState_.load(); }

    /**
     * @brief 发送 SOME/IP 消息到 MCU（ETH 通道）
     * @param msg  SOME/IP 消息
     * @return     是否加入发送队列
     */
    bool SendSomeIpMessage(const ara::com::someip::SomeIpMessage& msg);

    /**
     * @brief 发送 SPI IPC 帧（低时延控制命令）
     * @param frame  SPI 帧（CRC 自动填充）
     * @return       是否发送成功
     */
    bool SendSpiFrame(SpiIpcFrame& frame);

    /**
     * @brief 驱动 GPIO 信号
     */
    void SetGpioSignal(GpioSignal signal, bool level);

    /**
     * @brief 读取 GPIO 信号状态
     */
    bool GetGpioSignal(GpioSignal signal) const;

    // 回调注册
    void SetIpcStateCallback(IpcStateCallback cb);
    void SetSpiRxCallback(SpiRxCallback cb);
    void SetGpioChangeCallback(GpioChangeCallback cb);

    bool IsRunning() const { return running_.load(); }

private:
    void TransitionState(IpcConnectionState newState);
    void HandleHandshake();
    void HandleNormalOperation();
    void HandleError();

    // SOME/IP SD 服务注册（IPC 就绪后 Offer 所有服务）
    void OfferIpcServices();
    void StopIpcServices();

    ara::exec::ExecutionClient execClient_;
    std::unique_ptr<ara::nm::NetworkHandle> networkHandle_;

    mutable std::mutex mutex_;
    std::atomic<IpcConnectionState> connectionState_{IpcConnectionState::kOffline};
    std::atomic<bool> running_{false};

    // GPIO 状态表（8 路）
    bool gpioState_[8] = {false};

    // SPI 发送队列
    std::queue<SpiIpcFrame> spiTxQueue_;

    // ETH 发送队列
    std::queue<ara::com::someip::SomeIpMessage> ethTxQueue_;

    // 回调
    IpcStateCallback ipcStateCallback_;
    SpiRxCallback    spiRxCallback_;
    GpioChangeCallback gpioCallback_;

    // 握手超时追踪
    std::chrono::steady_clock::time_point handshakeStartTime_;
    static constexpr uint32_t kHandshakeTimeoutMs = 2000; ///< 握手超时 2s

    // 连接保活追踪
    std::chrono::steady_clock::time_point lastHeartbeatTime_;
    static constexpr uint32_t kHeartbeatIntervalMs = 100; ///< 心跳 100ms
    static constexpr uint32_t kHeartbeatTimeoutMs  = 500; ///< 心跳超时 500ms
};

} // namespace swc
} // namespace app

#endif // APP_SWC_IPC_BRIDGE_SERVICE_H
