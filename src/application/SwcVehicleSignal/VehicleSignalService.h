/**
 * @file VehicleSignalService.h
 * @brief VehicleSignalService SWC — 整车信号服务
 *
 * 对应 SOC-SW-001 §4.1 VehicleSignalService SWC
 * Service ID: 0x1001（SOME/IP，MCU→SOC，Event/Field 模式）
 *
 * 功能：
 * 1. 订阅来自 MCU 的整车信号（车速/转速/档位/电压等）
 * 2. 将信号以 ara::com Event 形式向内部其他 SWC 发布
 * 3. 信号有效性校验（E2E Profile 2）
 * 4. 向 ara::per 持久化关键信号统计
 *
 * 通信路径：
 *   MCU ComM → SoAd → SOME/IP → ETH → ara::com → VehicleSignalService SWC
 */

#ifndef APP_SWC_VEHICLE_SIGNAL_SERVICE_H
#define APP_SWC_VEHICLE_SIGNAL_SERVICE_H

#include "ara/com/types.h"
#include "ara/com/someip_binding.h"
#include "ara/exec/execution_client.h"
#include "ara/log/logger.h"
#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace app {
namespace swc {

// ============================================================
// 整车信号数据结构
// ============================================================

/**
 * @brief 整车信号快照（对应 MCU COM Signal Group）
 * 对应 IPC-ARCH-001§2.4 VehicleSignalService (0x1001)
 */
struct VehicleSignals {
    float    vehicleSpeedKph;       ///< 车速（km/h），范围 0~300
    uint16_t engineRpm;             ///< 发动机转速（rpm），范围 0~8000
    uint8_t  gearPosition;          ///< 档位（0=P,1=R,2=N,3=D,4~8=手动挡）
    float    batteryVoltage;        ///< 蓄电池电压（V）
    float    batteryCurrentA;       ///< 蓄电池电流（A，正=放电）
    bool     ignitionOn;            ///< 点火开关状态
    bool     brakePressed;          ///< 制动踏板状态
    uint32_t odometer;              ///< 里程计（km）
    uint32_t timestampMs;           ///< 信号时间戳（ms，来自 MCU StbM）
    uint8_t  e2eCounterValue;       ///< E2E Profile 2 计数器
    uint8_t  e2eCrcValue;           ///< E2E Profile 2 CRC

    VehicleSignals()
        : vehicleSpeedKph(0.0f), engineRpm(0), gearPosition(0)
        , batteryVoltage(12.0f), batteryCurrentA(0.0f)
        , ignitionOn(false), brakePressed(false)
        , odometer(0), timestampMs(0)
        , e2eCounterValue(0), e2eCrcValue(0)
    {}
};

// ============================================================
// VehicleSignalServiceProxy — Consumer 侧（订阅来自 MCU 的信号）
// ============================================================

/**
 * @brief VehicleSignalService Proxy（ara::com Consumer）
 *
 * 对应 IPC-ARCH-001 §2.4 Service ID 0x1001
 * 使用 SOME/IP Event 模式订阅，EventGroup 0x01
 */
class VehicleSignalServiceProxy : public ara::com::ProxyBase {
public:
    explicit VehicleSignalServiceProxy(const ara::com::InstanceIdentifier& instance)
        : ara::com::ProxyBase(instance) {}

    /// 信号更新回调
    using SignalUpdateCallback = std::function<void(const VehicleSignals& signals)>;

    /**
     * @brief 订阅整车信号事件（SWS_CM_00040）
     */
    void SubscribeVehicleSignals(SignalUpdateCallback cb);

    /**
     * @brief 取消订阅
     */
    void UnsubscribeVehicleSignals();

    /**
     * @brief 获取最新信号快照（Field 模式）
     */
    VehicleSignals GetCurrentSignals() const;

private:
    mutable std::mutex mutex_;
    VehicleSignals latestSignals_;
    SignalUpdateCallback callback_;
    ara::com::SubscriptionState subscriptionState_{ara::com::SubscriptionState::kNotSubscribed};
};

// ============================================================
// VehicleSignalServiceSWC — 主 SWC 类
// ============================================================

/**
 * @brief VehicleSignalService SWC
 *
 * 生命周期：
 *   Init() → Run()（事件循环）→ Shutdown()
 *
 * 对应 SOC-SW-001 §4.1：
 *   - 整车信号订阅与缓存
 *   - E2E 校验失败时向 ara::phm 上报
 *   - 持久化统计数据到 ara::per
 */
class VehicleSignalServiceSWC {
public:
    VehicleSignalServiceSWC();
    ~VehicleSignalServiceSWC();

    /**
     * @brief 初始化（注册 PHM、订阅 SOME/IP 服务）
     */
    bool Init();

    /**
     * @brief 主循环（在独立线程或事件调度器中调用）
     */
    void Run();

    /**
     * @brief 关闭
     */
    void Shutdown();

    /**
     * @brief 获取最新整车信号（线程安全）
     */
    VehicleSignals GetLatestSignals() const;

    /**
     * @brief 注册信号变化通知（内部 SWC 间通信）
     */
    void SetSignalChangeCallback(VehicleSignalServiceProxy::SignalUpdateCallback cb);

    bool IsRunning() const { return running_.load(); }

private:
    void OnSignalReceived(const VehicleSignals& signals);
    bool ValidateE2E(const VehicleSignals& signals);

    std::unique_ptr<VehicleSignalServiceProxy> proxy_;
    ara::exec::ExecutionClient execClient_;

    mutable std::mutex mutex_;
    VehicleSignals latestSignals_;
    std::atomic<bool> running_{false};
    VehicleSignalServiceProxy::SignalUpdateCallback externalCallback_;

    // E2E 状态跟踪
    uint8_t lastE2ECounter_{0};
    uint32_t e2eErrorCount_{0};
};

} // namespace swc
} // namespace app

#endif // APP_SWC_VEHICLE_SIGNAL_SERVICE_H
