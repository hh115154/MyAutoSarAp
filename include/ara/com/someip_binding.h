/**
 * @file someip_binding.h
 * @brief ara::com SOME/IP 绑定层
 *
 * 符合 AUTOSAR AP R25-11 规范
 * 实现 SOME/IP 协议绑定：
 * - 服务发现（SOME/IP SD，UDP 30490）
 * - 事件通知（Pub/Sub，UDP 30500-30599）
 * - 方法调用（Request/Response，TCP 30600-30699）
 * - E2E Profile 2 保护
 *
 * 对应 IPC-ARCH-001 §2 SOME/IP 服务目录（Service ID 0x1001~0x1007）
 * 本实现为模拟绑定层（无真实 Socket），接口与生产实现对齐。
 */

#ifndef ARA_COM_SOMEIP_BINDING_H
#define ARA_COM_SOMEIP_BINDING_H

#include "ara/com/types.h"
#include "ara/com/proxy_base.h"
#include "ara/com/skeleton_base.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace ara {
namespace com {
namespace someip {

// ============================================================
// SOME/IP 头部（AUTOSAR PRS_SOMEIP §4.1.1）
// ============================================================

/**
 * @brief SOME/IP 消息类型（Message Type）
 */
enum class MessageType : uint8_t {
    kRequest             = 0x00,  ///< 方法调用请求（期待响应）
    kRequestNoReturn     = 0x01,  ///< 方法调用（不期待响应）
    kNotification        = 0x02,  ///< 事件通知
    kResponse            = 0x80,  ///< 方法调用响应
    kError               = 0x81   ///< 错误响应
};

/**
 * @brief SOME/IP 返回码
 */
enum class ReturnCode : uint8_t {
    kOk                  = 0x00,
    kNotOk               = 0x01,
    kUnknownService      = 0x02,
    kUnknownMethod       = 0x03,
    kNotReady            = 0x04,
    kNotReachable        = 0x05,
    kTimeout             = 0x06,
    kWrongProtocolVersion = 0x07,
    kWrongInterfaceVersion = 0x08,
    kMalformedMessage    = 0x09,
    kWrongMessageType    = 0x0A,
    kE2eRepeated         = 0x0B,  ///< E2E 重复
    kE2eWrongSequence    = 0x0C   ///< E2E 序号错误
};

/**
 * @brief SOME/IP 消息头（16 字节，AUTOSAR 标准格式）
 */
struct SomeIpHeader {
    uint16_t serviceId;    ///< Service ID（2 bytes）
    uint16_t methodId;     ///< Method/Event ID（2 bytes）
    uint32_t length;       ///< Payload 长度（4 bytes，不含 Header 前 8B）
    uint16_t clientId;     ///< 客户端 ID（2 bytes）
    uint16_t sessionId;    ///< 会话 ID（2 bytes）
    uint8_t  protoVersion; ///< 协议版本 = 0x01
    uint8_t  ifaceVersion; ///< 接口版本
    MessageType msgType;   ///< 消息类型
    ReturnCode  returnCode;///< 返回码
};

/**
 * @brief 完整 SOME/IP 消息
 */
struct SomeIpMessage {
    SomeIpHeader header;
    std::vector<uint8_t> payload;

    /**
     * @brief 序列化为字节流
     */
    std::vector<uint8_t> Serialize() const;

    /**
     * @brief 从字节流反序列化
     * @return 解析是否成功
     */
    static bool Deserialize(const std::vector<uint8_t>& data, SomeIpMessage& msg);
};

// ============================================================
// SOME/IP 服务描述符（来自 IPC-ARCH-001 §2.4）
// ============================================================

/**
 * @brief SOME/IP 服务目录条目
 */
struct ServiceDescriptor {
    uint16_t    serviceId;       ///< Service ID
    uint8_t     instanceId;      ///< Instance ID
    uint8_t     majorVersion;    ///< Major 版本
    uint32_t    minorVersion;    ///< Minor 版本
    std::string serviceName;     ///< 服务名称
    uint16_t    eventgroupId;    ///< EventGroup ID（订阅用）
    bool        tcpBinding;      ///< true=TCP, false=UDP
    uint16_t    port;            ///< 端口号
};

/// IPC-ARCH-001 §2.4 定义的 7 个 SOME/IP 服务
static const ServiceDescriptor kServiceCatalog[] = {
    {0x1001, 0x01, 1, 0, "VehicleSignalService",   0x01, false, 30501},
    {0x1002, 0x01, 1, 0, "SafetyStatusService",    0x02, false, 30502},
    {0x1003, 0x01, 1, 0, "PowerModeService",       0x03, true,  30601},
    {0x1004, 0x01, 1, 0, "DiagnosticProxyService", 0x04, true,  30602},
    {0x1005, 0x01, 1, 0, "OTATransferService",     0x05, true,  30701},
    {0x1006, 0x01, 1, 0, "HMICommandService",      0x06, true,  30603},
    {0x1007, 0x01, 1, 0, "NetworkStatusService",   0x07, false, 30503},
};

// ============================================================
// SomeIpServiceDiscovery — SD 模块（SOME/IP SD）
// ============================================================

/**
 * @brief SOME/IP SD 服务发现
 *
 * 对应 IPC-ARCH-001 §2.3：UDP 30490 服务发现端口
 * 实现 OfferService / FindService / Subscribe 握手
 */
class SomeIpServiceDiscovery {
public:
    using ServiceAvailableCallback = std::function<void(const ServiceDescriptor& svc, bool available)>;

    static SomeIpServiceDiscovery& GetInstance();

    /**
     * @brief 广播 Offer Service（Provider 调用）
     */
    void OfferService(const ServiceDescriptor& svc);

    /**
     * @brief 撤回 Offer Service
     */
    void StopOfferService(uint16_t serviceId);

    /**
     * @brief 查询已发现的服务列表
     */
    std::vector<ServiceDescriptor> FindServices(uint16_t serviceId) const;

    /**
     * @brief 订阅服务可用性通知
     */
    void SubscribeServiceAvailability(uint16_t serviceId, ServiceAvailableCallback cb);

private:
    SomeIpServiceDiscovery() = default;
    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, ServiceDescriptor> offeredServices_;
    std::unordered_map<uint16_t, std::vector<ServiceAvailableCallback>> subscribers_;
};

// ============================================================
// EventPublisher / EventSubscriber — Pub/Sub 模式
// ============================================================

/**
 * @brief 事件发布者（Skeleton 侧）
 */
template <typename SampleType>
class EventPublisher {
public:
    explicit EventPublisher(uint16_t serviceId, uint16_t eventId)
        : serviceId_(serviceId), eventId_(eventId) {}

    /**
     * @brief 发布事件样本
     * @param sample   事件数据
     * @param e2eEnabled 是否启用 E2E Profile 2 保护
     */
    void Send(const SampleType& sample, bool e2eEnabled = false);

    /**
     * @brief 分配发送缓冲区
     */
    std::shared_ptr<SampleType> Allocate();

private:
    uint16_t serviceId_;
    uint16_t eventId_;
    uint16_t sequenceCounter_{0};
};

/**
 * @brief 事件订阅者（Proxy 侧）
 */
template <typename SampleType>
class EventSubscriber {
public:
    using SampleCallback = std::function<void(std::shared_ptr<SampleType> sample)>;

    explicit EventSubscriber(uint16_t serviceId, uint16_t eventId)
        : serviceId_(serviceId), eventId_(eventId) {}

    /**
     * @brief 订阅事件（SWS_CM_00040）
     * @param maxSamples 最大缓存样本数
     */
    void Subscribe(size_t maxSamples = 8);

    /**
     * @brief 取消订阅
     */
    void Unsubscribe();

    /**
     * @brief 获取订阅状态
     */
    SubscriptionState GetSubscriptionState() const { return state_.load(); }

    /**
     * @brief 设置接收回调（新样本到达时触发）
     */
    void SetReceiveHandler(SampleCallback cb);

    /**
     * @brief 拉取最新样本（Pull 模式）
     */
    size_t GetNewSamples(SampleCallback f, size_t maxCount = 1);

private:
    uint16_t serviceId_;
    uint16_t eventId_;
    std::atomic<SubscriptionState> state_{SubscriptionState::kNotSubscribed};
    SampleCallback receiveHandler_;
};

// ============================================================
// MethodCaller — Request/Response 模式
// ============================================================

/**
 * @brief 方法调用结果（Future 模式）
 */
template <typename ResponseType>
class MethodResult {
public:
    MethodResult() = default;
    explicit MethodResult(ResponseType val) : value_(std::move(val)), ready_(true) {}

    bool IsReady() const { return ready_; }
    const ResponseType& GetValue() const { return value_; }
    ReturnCode GetReturnCode() const { return returnCode_; }

    void SetValue(ResponseType val, ReturnCode rc = ReturnCode::kOk) {
        value_ = std::move(val);
        returnCode_ = rc;
        ready_ = true;
    }

private:
    ResponseType value_{};
    ReturnCode returnCode_{ReturnCode::kOk};
    bool ready_{false};
};

} // namespace someip
} // namespace com
} // namespace ara

#endif // ARA_COM_SOMEIP_BINDING_H
