/**
 * @file    vehicle_signal_proxy.h
 * @brief   ara::com — VehicleSignalService Proxy（SOME/IP Consumer）
 *
 * 对应 IPC-ARCH-001 §2.4 Service ID = 0x1001
 * 运行在 SOC（MyAutoSarAp 进程），订阅 MCU（MyAutoSarCP 进程）发布的
 * VehicleSignalService Event（UDP 127.0.0.1:30501）
 *
 * 使用方式：
 *   1. VehicleSignalProxy::FindService()
 *   2. proxy.VehicleSignal.Subscribe(maxSamples)
 *   3. proxy.VehicleSignal.SetReceiveHandler(cb)  或循环 GetNewSamples()
 *   4. proxy.StopFindService() / 析构
 */

#ifndef ARA_COM_VEHICLE_SIGNAL_PROXY_H
#define ARA_COM_VEHICLE_SIGNAL_PROXY_H

#include "ara/com/types.h"
#include "ara/com/proxy_base.h"
#include "someip_proto.h"   /* 共享协议常量 */

#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <string>

namespace ara {
namespace com {
namespace vehicle {

/**
 * @brief VehicleSignalService Event 数据（ara::com 层 sample 类型）
 *
 * 对应 VehicleSignalPayload_t，供应用层使用（C++ 友好版本）
 */
struct VehicleSignalSample {
    float    vehicleSpeedKmh;     ///< 车速 km/h
    float    engineRpm;           ///< 转速 RPM
    bool     brakePedal;          ///< 制动踏板
    float    steeringAngleDeg;    ///< 方向盘转角
    uint8_t  doorStatus;          ///< 车门状态 bitmask
    float    fuelLevelPct;        ///< 燃油液位 %
    uint8_t  e2eCrc;              ///< E2E CRC8
    uint8_t  e2eCounter;          ///< E2E 计数器
    uint16_t sessionId;           ///< SOME/IP 会话 ID
};

// ============================================================
// VehicleSignalEvent — Event 描述符（类似 ara::com 生成代码）
// ============================================================

class VehicleSignalProxy; // forward

class VehicleSignalEvent {
public:
    using SampleType      = VehicleSignalSample;
    using SamplePtr       = std::shared_ptr<SampleType>;
    using ReceiveHandler  = std::function<void(SamplePtr)>;

    VehicleSignalEvent() = default;

    /** 订阅事件（SWS_CM_00040）*/
    void Subscribe(std::size_t maxSamples = 16);

    /** 取消订阅 */
    void Unsubscribe();

    /** 当前订阅状态 */
    SubscriptionState GetSubscriptionState() const;

    /** 设置新样本回调（Push 模式）*/
    void SetReceiveHandler(ReceiveHandler handler);

    /** 清空回调 */
    void UnsetReceiveHandler();

    /** 拉取样本（Pull 模式）*/
    std::size_t GetNewSamples(std::function<void(SamplePtr)> f,
                              std::size_t maxCount = 1);

    // 内部：由 Proxy 的接收线程推送样本
    void PushSample(SamplePtr sample);

private:
    std::atomic<SubscriptionState> state_{SubscriptionState::kNotSubscribed};
    std::size_t maxSamples_{16};

    mutable std::mutex      queueMtx_;
    std::queue<SamplePtr>   sampleQueue_;
    ReceiveHandler          receiveHandler_;
};

// ============================================================
// VehicleSignalProxy — Proxy 类（SOME/IP Consumer）
// ============================================================

class VehicleSignalProxy : public ProxyBase {
public:
    /** 对外暴露的 Event 成员（与 AUTOSAR 生成代码约定一致）*/
    VehicleSignalEvent VehicleSignal;

    /**
     * @brief 构造：绑定 UDP 接收端口，启动接收线程
     * @param localPort  本地监听端口（默认 30501）
     */
    explicit VehicleSignalProxy(uint16_t localPort = VEHICLE_SIGNAL_SVC_PORT);

    ~VehicleSignalProxy() override;

    // 禁止拷贝
    VehicleSignalProxy(const VehicleSignalProxy&)            = delete;
    VehicleSignalProxy& operator=(const VehicleSignalProxy&) = delete;

    /**
     * @brief 查找并返回可用的 VehicleSignalProxy 实例
     *
     * 简化版 FindService：创建并返回 Proxy（真实实现中先做 SD 握手）
     */
    static std::shared_ptr<VehicleSignalProxy> FindService(
        uint16_t localPort = VEHICLE_SIGNAL_SVC_PORT);

    /**
     * @brief 获取最近一次接收到的原始 SOME/IP 统计
     */
    struct Stats {
        uint64_t rxPackets{0};    ///< 收到的 SOME/IP 帧数
        uint64_t rxBytes{0};      ///< 收到的字节数
        uint64_t e2eErrors{0};    ///< E2E CRC 校验失败次数
        uint64_t parseErrors{0};  ///< 协议解析失败次数
    };
    Stats GetStats() const;

private:
    int                      sockFd_{-1};
    std::thread              rxThread_;
    std::atomic<bool>        running_{false};

    mutable std::mutex       statsMtx_;
    Stats                    stats_;

    void RxThreadFunc();

    /**
     * @brief 解析 SOME/IP 帧，验证 E2E，推送 sample
     * @return true 解析成功
     */
    bool ParseAndDispatch(const uint8_t* buf, std::size_t len);
};

} // namespace vehicle
} // namespace com
} // namespace ara

#endif // ARA_COM_VEHICLE_SIGNAL_PROXY_H
