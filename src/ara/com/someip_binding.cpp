/**
 * @file someip_binding.cpp
 * @brief ara::com SOME/IP 绑定层实现
 */

#include "ara/com/someip_binding.h"
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace ara {
namespace com {
namespace someip {

// ============================================================
// SomeIpMessage 序列化 / 反序列化
// ============================================================

std::vector<uint8_t> SomeIpMessage::Serialize() const
{
    // SOME/IP Header = 16 bytes（标准格式）
    std::vector<uint8_t> buf;
    buf.reserve(16 + payload.size());

    auto push2 = [&](uint16_t v) {
        buf.push_back(static_cast<uint8_t>(v >> 8));
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
    };
    auto push4 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >>  0) & 0xFF));
    };

    push2(header.serviceId);
    push2(header.methodId);
    push4(static_cast<uint32_t>(8 + payload.size())); // Length = 剩余字节数（含后 8 字节 Header）
    push2(header.clientId);
    push2(header.sessionId);
    buf.push_back(header.protoVersion);
    buf.push_back(header.ifaceVersion);
    buf.push_back(static_cast<uint8_t>(header.msgType));
    buf.push_back(static_cast<uint8_t>(header.returnCode));

    buf.insert(buf.end(), payload.begin(), payload.end());
    return buf;
}

bool SomeIpMessage::Deserialize(const std::vector<uint8_t>& data, SomeIpMessage& msg)
{
    if (data.size() < 16) return false;

    auto rd2 = [&](size_t off) -> uint16_t {
        return static_cast<uint16_t>(
            (static_cast<uint16_t>(data[off]) << 8) | data[off + 1]);
    };
    auto rd4 = [&](size_t off) -> uint32_t {
        return (static_cast<uint32_t>(data[off])     << 24) |
               (static_cast<uint32_t>(data[off + 1]) << 16) |
               (static_cast<uint32_t>(data[off + 2]) <<  8) |
               (static_cast<uint32_t>(data[off + 3]));
    };

    msg.header.serviceId    = rd2(0);
    msg.header.methodId     = rd2(2);
    uint32_t length         = rd4(4);
    msg.header.clientId     = rd2(8);
    msg.header.sessionId    = rd2(10);
    msg.header.protoVersion = data[12];
    msg.header.ifaceVersion = data[13];
    msg.header.msgType      = static_cast<MessageType>(data[14]);
    msg.header.returnCode   = static_cast<ReturnCode>(data[15]);

    // Payload
    uint32_t payloadLen = (length >= 8) ? (length - 8) : 0;
    if (data.size() < 16 + payloadLen) return false;
    msg.payload.assign(data.begin() + 16, data.begin() + 16 + payloadLen);
    return true;
}

// ============================================================
// SomeIpServiceDiscovery 实现
// ============================================================

SomeIpServiceDiscovery& SomeIpServiceDiscovery::GetInstance()
{
    static SomeIpServiceDiscovery instance;
    return instance;
}

void SomeIpServiceDiscovery::OfferService(const ServiceDescriptor& svc)
{
    std::lock_guard<std::mutex> lock(mutex_);
    offeredServices_[svc.serviceId] = svc;

    // 通知所有订阅者
    auto it = subscribers_.find(svc.serviceId);
    if (it != subscribers_.end()) {
        for (auto& cb : it->second) {
            cb(svc, true);
        }
    }
}

void SomeIpServiceDiscovery::StopOfferService(uint16_t serviceId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = offeredServices_.find(serviceId);
    if (it == offeredServices_.end()) return;

    ServiceDescriptor svc = it->second;
    offeredServices_.erase(it);

    // 通知下线
    auto sub = subscribers_.find(serviceId);
    if (sub != subscribers_.end()) {
        for (auto& cb : sub->second) {
            cb(svc, false);
        }
    }
}

std::vector<ServiceDescriptor> SomeIpServiceDiscovery::FindServices(uint16_t serviceId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServiceDescriptor> result;

    if (serviceId == 0xFFFF) {
        // 查找所有
        for (const auto& kv : offeredServices_) {
            result.push_back(kv.second);
        }
    } else {
        auto it = offeredServices_.find(serviceId);
        if (it != offeredServices_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

void SomeIpServiceDiscovery::SubscribeServiceAvailability(
    uint16_t serviceId,
    ServiceAvailableCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_[serviceId].push_back(std::move(cb));

    // 如果服务已经可用，立即通知
    auto it = offeredServices_.find(serviceId);
    if (it != offeredServices_.end()) {
        subscribers_[serviceId].back()(it->second, true);
    }
}

} // namespace someip
} // namespace com
} // namespace ara
