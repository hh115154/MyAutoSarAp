/**
 * @file    vehicle_signal_proxy.cpp
 * @brief   ara::com — VehicleSignalService Proxy 实现（SOME/IP over UDP）
 *
 * 规范：IPC-ARCH-001 §2.4 / AUTOSAR_PRS_SOMEIPProtocol
 *
 * 实现要点：
 *   1. 绑定 UDP socket 至 127.0.0.1:30501（来自 CP 进程）
 *   2. 后台接收线程（select 超时 100ms）持续接收 SOME/IP 帧
 *   3. 解析 SOME/IP Header（16B Big-Endian），提取 VehicleSignalPayload_t
 *   4. E2E Profile 2 CRC8 校验
 *   5. 推送 sample 到 VehicleSignalEvent 队列，触发回调
 */

#include "ara/com/vehicle_signal_proxy.h"
#include "ara/log/logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <chrono>
#include <sys/select.h>

// 使用标准 htons/ntohs，避免与 SOMEIP_HTONS 宏冲突
#ifdef htons
#undef htons
#endif

namespace ara {
namespace com {
namespace vehicle {

// ============================================================
// VehicleSignalEvent
// ============================================================

void VehicleSignalEvent::Subscribe(std::size_t maxSamples)
{
    maxSamples_ = maxSamples;
    state_.store(SubscriptionState::kSubscribed);
}

void VehicleSignalEvent::Unsubscribe()
{
    state_.store(SubscriptionState::kNotSubscribed);
}

SubscriptionState VehicleSignalEvent::GetSubscriptionState() const
{
    return state_.load();
}

void VehicleSignalEvent::SetReceiveHandler(ReceiveHandler handler)
{
    std::lock_guard<std::mutex> lk(queueMtx_);
    receiveHandler_ = std::move(handler);
}

void VehicleSignalEvent::UnsetReceiveHandler()
{
    std::lock_guard<std::mutex> lk(queueMtx_);
    receiveHandler_ = nullptr;
}

std::size_t VehicleSignalEvent::GetNewSamples(
    std::function<void(SamplePtr)> f, std::size_t maxCount)
{
    std::lock_guard<std::mutex> lk(queueMtx_);
    std::size_t count = 0;
    while (!sampleQueue_.empty() && count < maxCount) {
        f(sampleQueue_.front());
        sampleQueue_.pop();
        ++count;
    }
    return count;
}

void VehicleSignalEvent::PushSample(SamplePtr sample)
{
    ReceiveHandler cb;
    {
        std::lock_guard<std::mutex> lk(queueMtx_);
        if (state_.load() != SubscriptionState::kSubscribed) return;

        // 满了就丢最旧的（ring-buffer 语义）
        if (sampleQueue_.size() >= maxSamples_) {
            sampleQueue_.pop();
        }
        sampleQueue_.push(sample);
        cb = receiveHandler_;
    }
    if (cb) cb(sample);
}

// ============================================================
// VehicleSignalProxy
// ============================================================

VehicleSignalProxy::VehicleSignalProxy(uint16_t localPort)
    : ProxyBase(InstanceIdentifier("VehicleSignalService.0x01"))
{
    // 创建 UDP socket
    sockFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd_ < 0) {
        ara::log::CreateLogger("COM", "ara.com").LogError()
            << "[VehicleSignalProxy] socket() failed: " << ::strerror(errno);
        return;
    }

    // 允许端口复用
    int opt = 1;
    ::setsockopt(sockFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(sockFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // 绑定本地端口
    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
    // htons 可能被宏覆盖，使用 __builtin 安全等价
    addr.sin_port        = static_cast<uint16_t>(
        ((localPort & 0xFFu) << 8) | ((localPort >> 8) & 0xFFu));

    if (::bind(sockFd_, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        ara::log::CreateLogger("COM", "ara.com").LogError()
            << "[VehicleSignalProxy] bind(:" << localPort
            << ") failed: " << ::strerror(errno);
        ::close(sockFd_);
        sockFd_ = -1;
        return;
    }

    // 启动接收线程
    running_.store(true);
    rxThread_ = std::thread(&VehicleSignalProxy::RxThreadFunc, this);

    ara::log::CreateLogger("COM", "ara.com").LogInfo()
        << "[VehicleSignalProxy] Listening on 127.0.0.1:" << localPort
        << "  (VehicleSignalService 0x1001)";
}

VehicleSignalProxy::~VehicleSignalProxy()
{
    running_.store(false);
    if (sockFd_ >= 0) {
        ::shutdown(sockFd_, SHUT_RDWR);
        ::close(sockFd_);
        sockFd_ = -1;
    }
    if (rxThread_.joinable()) {
        rxThread_.join();
    }
}

/*static*/
std::shared_ptr<VehicleSignalProxy> VehicleSignalProxy::FindService(
    uint16_t localPort)
{
    // 简化版：直接创建 Proxy（生产实现中先发 SD FindService，等待 OfferService）
    auto proxy = std::make_shared<VehicleSignalProxy>(localPort);
    if (proxy->sockFd_ < 0) return nullptr;
    return proxy;
}

VehicleSignalProxy::Stats VehicleSignalProxy::GetStats() const
{
    std::lock_guard<std::mutex> lk(statsMtx_);
    return stats_;
}

// ----------------------------------------------------------------
// 接收线程
// ----------------------------------------------------------------

void VehicleSignalProxy::RxThreadFunc()
{
    constexpr std::size_t kBufSize = 1024u;
    uint8_t buf[kBufSize];

    while (running_.load()) {
        if (sockFd_ < 0) break;

        // select 超时 100ms，支持优雅退出
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sockFd_, &rfds);
        struct timeval tv{ 0, 100000 };  // 100ms

        int ret = ::select(sockFd_ + 1, &rfds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        ssize_t n = ::recv(sockFd_, buf, kBufSize, 0);
        if (n <= 0) continue;

        {
            std::lock_guard<std::mutex> lk(statsMtx_);
            stats_.rxPackets++;
            stats_.rxBytes += static_cast<uint64_t>(n);

            /* 每 10 帧输出一条原始接收日志，记录收到 MCU 发来的 SOME/IP 帧 */
            if (stats_.rxPackets % 10u == 1u) {
                printf("[SOC_LOG] {\"level\":\"INFO\",\"ctx\":\"COM\","
                       "\"msg\":\"SOMEIP_RAW_RX\","
                       "\"src\":\"127.0.0.1:40501\",\"dst_port\":30501,"
                       "\"proto\":\"SOME/IP over UDP\","
                       "\"frame_bytes\":%d,\"rx_total\":%llu}\n",
                       (int)n,
                       (unsigned long long)stats_.rxPackets);
                fflush(stdout);
            }
        }

        if (!ParseAndDispatch(buf, static_cast<std::size_t>(n))) {
            std::lock_guard<std::mutex> lk(statsMtx_);
            stats_.parseErrors++;
        }
    }
}

// ----------------------------------------------------------------
// 解析 + E2E 校验 + dispatch
// ----------------------------------------------------------------

bool VehicleSignalProxy::ParseAndDispatch(const uint8_t* buf, std::size_t len)
{
    // 最小帧：16B Header + 20B Payload
    constexpr std::size_t kMinLen = 16u + sizeof(VehicleSignalPayload_t);
    if (len < kMinLen) return false;

    // 解析 SOME/IP Header（Big-Endian）
    auto rd2 = [&](std::size_t off) -> uint16_t {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(buf[off]) << 8) | buf[off + 1]);
    };
    auto rd4 = [&](std::size_t off) -> uint32_t {
        return (static_cast<uint32_t>(buf[off])     << 24) |
               (static_cast<uint32_t>(buf[off + 1]) << 16) |
               (static_cast<uint32_t>(buf[off + 2]) <<  8) |
               (static_cast<uint32_t>(buf[off + 3]));
    };

    uint16_t serviceId = rd2(0);
    uint16_t methodId  = rd2(2);
    uint32_t length    = rd4(4);
    uint16_t sessionId = rd2(10);
    uint8_t  msgType   = buf[14];

    // 只处理 VehicleSignalService(0x1001) Event(0x8001) Notification
    if (serviceId != SVC_ID_VEHICLE_SIGNAL) return true; // 其他服务忽略
    if (methodId  != VEHICLE_SIGNAL_EVT_ID) return true;
    if (msgType   != SOMEIP_MSG_NOTIFICATION) return false;

    // Payload 长度校验
    uint32_t payloadLen = (length >= 8u) ? (length - 8u) : 0u;
    if (payloadLen < sizeof(VehicleSignalPayload_t)) return false;
    if (len < 16u + payloadLen) return false;

    // 提取 VehicleSignalPayload_t（注意字节序：float 在 embedded 侧是 native LE）
    VehicleSignalPayload_t raw;
    std::memcpy(&raw, buf + 16u, sizeof(raw));

    // E2E Profile 2 CRC8 校验
    uint8_t expectedCrc = e2e_crc8(
        reinterpret_cast<const uint8_t*>(&raw),
        static_cast<uint32_t>(sizeof(raw) - 2u));  // 最后 2B = crc + counter
    if (raw.e2e_crc != expectedCrc) {
        {
            std::lock_guard<std::mutex> lk(statsMtx_);
            stats_.e2eErrors++;
        }
        ara::log::CreateLogger("COM", "ara.com").LogWarn()
            << "[VehicleSignalProxy] E2E CRC mismatch: "
            << "expected=" << static_cast<int>(expectedCrc)
            << " got=" << static_cast<int>(raw.e2e_crc)
            << " counter=" << static_cast<int>(raw.e2e_counter);
        return false;
    }

    // 构造 C++ sample
    auto sample = std::make_shared<VehicleSignalSample>();
    sample->vehicleSpeedKmh   = raw.vehicle_speed_kmh;
    sample->engineRpm         = raw.engine_rpm;
    sample->brakePedal        = (raw.brake_pedal != 0u);
    sample->steeringAngleDeg  = raw.steering_angle_deg;
    sample->doorStatus        = raw.door_status;
    sample->fuelLevelPct      = raw.fuel_level_pct;
    sample->e2eCrc            = raw.e2e_crc;
    sample->e2eCounter        = raw.e2e_counter;
    sample->sessionId         = sessionId;

    // 推送到 Event 订阅者
    VehicleSignal.PushSample(sample);
    return true;
}

} // namespace vehicle
} // namespace com
} // namespace ara
