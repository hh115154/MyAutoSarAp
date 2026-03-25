/**
 * @file test_ara_nm.cpp
 * @brief ara::nm 单元测试
 */

#include <gtest/gtest.h>
#include "ara/nm/network_management.h"

using namespace ara::nm;

class AraNetworkManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& nm = NetworkManagement::GetInstance();
        nm.Initialize();
    }
};

// ============================================================
// 单例
// ============================================================

TEST_F(AraNetworkManagementTest, SingletonIdentity) {
    auto& a = NetworkManagement::GetInstance();
    auto& b = NetworkManagement::GetInstance();
    EXPECT_EQ(&a, &b);
}

// ============================================================
// 网络请求/释放
// ============================================================

TEST_F(AraNetworkManagementTest, RequestEthIPC) {
    auto& nm = NetworkManagement::GetInstance();
    auto result = nm.RequestNetwork(NetworkId::kEthIPC);
    EXPECT_TRUE(result == NmRequestResult::kSuccess ||
                result == NmRequestResult::kAlreadyRequested);

    // 验证状态变为 Normal
    auto state = nm.GetNetworkState(NetworkId::kEthIPC);
    EXPECT_EQ(state, NetworkState::kNormal);

    // 释放
    nm.ReleaseNetwork(NetworkId::kEthIPC);
}

TEST_F(AraNetworkManagementTest, RequestAndReleaseReturnsToSleep) {
    auto& nm = NetworkManagement::GetInstance();

    // 确保先释放到 0
    nm.ReleaseNetwork(NetworkId::kEthExternal);
    nm.ReleaseNetwork(NetworkId::kEthExternal);

    nm.RequestNetwork(NetworkId::kEthExternal);
    EXPECT_EQ(nm.GetNetworkState(NetworkId::kEthExternal), NetworkState::kNormal);

    nm.ReleaseNetwork(NetworkId::kEthExternal);
    EXPECT_EQ(nm.GetNetworkState(NetworkId::kEthExternal), NetworkState::kBusSleep);
}

TEST_F(AraNetworkManagementTest, ReferenceCountingMultipleRequests) {
    auto& nm = NetworkManagement::GetInstance();

    nm.RequestNetwork(NetworkId::kLoopback);
    nm.RequestNetwork(NetworkId::kLoopback);
    EXPECT_EQ(nm.GetNetworkState(NetworkId::kLoopback), NetworkState::kNormal);

    nm.ReleaseNetwork(NetworkId::kLoopback);
    // 还有一个引用，应仍为 Normal
    EXPECT_EQ(nm.GetNetworkState(NetworkId::kLoopback), NetworkState::kNormal);

    nm.ReleaseNetwork(NetworkId::kLoopback);
    EXPECT_EQ(nm.GetNetworkState(NetworkId::kLoopback), NetworkState::kBusSleep);
}

TEST_F(AraNetworkManagementTest, RequestUnknownNetwork) {
    auto& nm = NetworkManagement::GetInstance();
    auto result = nm.RequestNetwork(static_cast<NetworkId>(0xFFFF));
    EXPECT_EQ(result, NmRequestResult::kNetworkUnknown);
}

TEST_F(AraNetworkManagementTest, ReleaseUnrequested) {
    auto& nm = NetworkManagement::GetInstance();
    // 确保 kEthDoIP 未被请求（refCount=0）
    nm.ReleaseNetwork(NetworkId::kEthDoIP);
    nm.ReleaseNetwork(NetworkId::kEthDoIP);
    nm.ReleaseNetwork(NetworkId::kEthDoIP);
    auto result = nm.ReleaseNetwork(NetworkId::kEthDoIP);
    // 未请求就释放应返回 Error
    EXPECT_EQ(result, NmRequestResult::kError);
}

// ============================================================
// NetworkHandle RAII
// ============================================================

TEST_F(AraNetworkManagementTest, NetworkHandleRAII) {
    auto& nm = NetworkManagement::GetInstance();

    // 确保初始状态
    nm.ReleaseNetwork(NetworkId::kLoopback);
    nm.ReleaseNetwork(NetworkId::kLoopback);

    {
        NetworkHandle handle(NetworkId::kLoopback);
        EXPECT_TRUE(handle.IsValid());
        EXPECT_EQ(nm.GetNetworkState(NetworkId::kLoopback), NetworkState::kNormal);
    }
    // handle 析构 → 引用计数归零 → BusSleep
    EXPECT_EQ(nm.GetNetworkState(NetworkId::kLoopback), NetworkState::kBusSleep);
}

TEST_F(AraNetworkManagementTest, NetworkHandleMove) {
    NetworkHandle h1(NetworkId::kLoopback);
    EXPECT_TRUE(h1.IsValid());

    NetworkHandle h2(std::move(h1));
    EXPECT_FALSE(h1.IsValid());
    EXPECT_TRUE(h2.IsValid());
}

// ============================================================
// 状态变化回调
// ============================================================

TEST_F(AraNetworkManagementTest, StateChangeCallback) {
    auto& nm = NetworkManagement::GetInstance();

    NetworkId observedNet = NetworkId::kLoopback;
    NetworkState observedState = NetworkState::kBusSleep;

    nm.RegisterNetworkStateCallback(
        [&](NetworkId net, NetworkState /*old*/, NetworkState nw) {
            observedNet   = net;
            observedState = nw;
        });

    // 确保初始 BusSleep
    nm.ReleaseNetwork(NetworkId::kEthIPC);
    nm.ReleaseNetwork(NetworkId::kEthIPC);
    nm.RequestNetwork(NetworkId::kEthIPC);

    EXPECT_EQ(observedNet,   NetworkId::kEthIPC);
    EXPECT_EQ(observedState, NetworkState::kNormal);

    nm.ReleaseNetwork(NetworkId::kEthIPC);
    nm.RegisterNetworkStateCallback(nullptr); // 清除
}

// ============================================================
// 活跃网络列表
// ============================================================

TEST_F(AraNetworkManagementTest, GetActiveNetworks) {
    auto& nm = NetworkManagement::GetInstance();

    // 请求 Loopback
    nm.RequestNetwork(NetworkId::kLoopback);
    auto active = nm.GetActiveNetworks();
    bool found = false;
    for (auto id : active) {
        if (id == NetworkId::kLoopback) { found = true; break; }
    }
    EXPECT_TRUE(found);

    nm.ReleaseNetwork(NetworkId::kLoopback);
}

// ============================================================
// 枚举值
// ============================================================

TEST_F(AraNetworkManagementTest, NetworkStateEnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(NetworkState::kBusSleep),       0u);
    EXPECT_EQ(static_cast<uint8_t>(NetworkState::kPrepareBusSleep),1u);
    EXPECT_EQ(static_cast<uint8_t>(NetworkState::kReady),          2u);
    EXPECT_EQ(static_cast<uint8_t>(NetworkState::kNormal),         3u);
}

TEST_F(AraNetworkManagementTest, NmRequestResultValues) {
    EXPECT_EQ(static_cast<uint8_t>(NmRequestResult::kSuccess),          0u);
    EXPECT_EQ(static_cast<uint8_t>(NmRequestResult::kAlreadyRequested), 1u);
    EXPECT_EQ(static_cast<uint8_t>(NmRequestResult::kNetworkUnknown),   2u);
    EXPECT_EQ(static_cast<uint8_t>(NmRequestResult::kError),            3u);
}
