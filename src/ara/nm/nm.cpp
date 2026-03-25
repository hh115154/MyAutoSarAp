/**
 * @file nm.cpp
 * @brief ara::nm — Network Management 实现
 */

#include "ara/nm/network_management.h"
#include <stdexcept>

namespace ara {
namespace nm {

// ============================================================
// NetworkHandle 实现
// ============================================================

NetworkHandle::NetworkHandle(NetworkId networkId)
    : networkId_(networkId)
    , valid_(false)
{
    auto result = NetworkManagement::GetInstance().RequestNetwork(networkId_);
    if (result == NmRequestResult::kSuccess || result == NmRequestResult::kAlreadyRequested) {
        valid_ = true;
    }
}

NetworkHandle::~NetworkHandle()
{
    if (valid_) {
        NetworkManagement::GetInstance().ReleaseNetwork(networkId_);
        valid_ = false;
    }
}

NetworkHandle::NetworkHandle(NetworkHandle&& other) noexcept
    : networkId_(other.networkId_)
    , valid_(other.valid_)
{
    other.valid_ = false;
}

// ============================================================
// NetworkManagement 实现
// ============================================================

NetworkManagement& NetworkManagement::GetInstance()
{
    static NetworkManagement instance;
    return instance;
}

NmRequestResult NetworkManagement::RequestNetwork(NetworkId networkId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    uint16_t key = static_cast<uint16_t>(networkId);
    auto it = networks_.find(key);
    if (it == networks_.end()) {
        return NmRequestResult::kNetworkUnknown;
    }

    auto& rec = it->second;
    uint32_t prev = rec.refCount.fetch_add(1);

    if (prev == 0) {
        // 第一次请求，激活网络
        NetworkState oldState = rec.state.exchange(NetworkState::kNormal);
        if (stateCallback_) {
            stateCallback_(networkId, oldState, NetworkState::kNormal);
        }
        return NmRequestResult::kSuccess;
    } else {
        return NmRequestResult::kAlreadyRequested;
    }
}

NmRequestResult NetworkManagement::ReleaseNetwork(NetworkId networkId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    uint16_t key = static_cast<uint16_t>(networkId);
    auto it = networks_.find(key);
    if (it == networks_.end()) {
        return NmRequestResult::kNetworkUnknown;
    }

    auto& rec = it->second;
    if (rec.refCount.load() == 0) {
        return NmRequestResult::kError; // 未请求就释放
    }

    uint32_t prev = rec.refCount.fetch_sub(1);
    if (prev == 1) {
        // 最后一个请求者释放，关闭网络
        NetworkState oldState = rec.state.exchange(NetworkState::kBusSleep);
        if (stateCallback_) {
            stateCallback_(networkId, oldState, NetworkState::kBusSleep);
        }
    }
    return NmRequestResult::kSuccess;
}

NetworkState NetworkManagement::GetNetworkState(NetworkId networkId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint16_t key = static_cast<uint16_t>(networkId);
    auto it = networks_.find(key);
    if (it == networks_.end()) {
        return NetworkState::kError;
    }
    return it->second.state.load();
}

void NetworkManagement::RegisterNetworkStateCallback(NetworkStateCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    stateCallback_ = std::move(cb);
}

bool NetworkManagement::Initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 注册 SOC 上的逻辑网络（对应 IPC-ARCH-001 §2.3）
    // 注意：NetworkRecord 含 atomic 成员，不可 move/copy，使用下标运算符就地构造
    auto addNetwork = [this](NetworkId id) {
        uint16_t key = static_cast<uint16_t>(id);
        if (networks_.find(key) == networks_.end()) {
            networks_[key].id = id;
            networks_[key].state.store(NetworkState::kBusSleep);
            networks_[key].refCount.store(0);
        }
    };

    addNetwork(NetworkId::kEthIPC);
    addNetwork(NetworkId::kEthExternal);
    addNetwork(NetworkId::kEthDoIP);
    addNetwork(NetworkId::kLoopback);

    return true;
}

void NetworkManagement::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : networks_) {
        kv.second.refCount.store(0);
        kv.second.state.store(NetworkState::kBusSleep);
    }
}

std::vector<NetworkId> NetworkManagement::GetActiveNetworks() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NetworkId> active;
    for (const auto& kv : networks_) {
        if (kv.second.state.load() == NetworkState::kNormal) {
            active.push_back(kv.second.id);
        }
    }
    return active;
}

} // namespace nm
} // namespace ara
