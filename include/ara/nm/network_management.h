/**
 * @file network_management.h
 * @brief ara::nm — Network Management
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Network Management
 * Ref: AUTOSAR_SWS_NetworkManagement (R25-11)
 *
 * 设计依据：SOC-SW-001 §3.5 + IPC-ARCH-001 §2
 * 管理 SOC 以太网网络生命周期，与 MCU 侧 ComM/CanNm 协同
 */

#ifndef ARA_NM_NETWORK_MANAGEMENT_H
#define ARA_NM_NETWORK_MANAGEMENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace ara {
namespace nm {

// ============================================================
// 枚举定义
// ============================================================

/**
 * @brief 网络状态（对应 AUTOSAR NM 状态机）
 */
enum class NetworkState : uint8_t {
    kBusSleep      = 0,  ///< 总线睡眠（关闭）
    kPrepareBusSleep = 1,///< 准备睡眠（等待活动结束）
    kReady         = 2,  ///< 就绪等待（NM 报文周期中）
    kNormal        = 3,  ///< 正常通信（Network Active）
    kRepeat        = 4,  ///< 重复报文模式（稳定前）
    kError         = 5   ///< 错误
};

/**
 * @brief 网络请求结果
 */
enum class NmRequestResult : uint8_t {
    kSuccess       = 0,
    kAlreadyRequested = 1, ///< 已在请求状态
    kNetworkUnknown= 2,    ///< 未知网络 ID
    kError         = 3
};

/**
 * @brief 网络 ID 定义（对应 SOC 的物理/逻辑网络）
 * 参考 IPC-ARCH-001 §2.3 IP 地址规划
 */
enum class NetworkId : uint16_t {
    kEthIPC        = 0x0001,  ///< SOC↔MCU 芯片间以太网（192.168.100.0/30）
    kEthExternal   = 0x0002,  ///< 外部以太网（ADAS/Telematics）
    kEthDoIP       = 0x0003,  ///< DoIP 诊断网络（端口 13400）
    kLoopback      = 0x00FF   ///< 回环（测试用）
};

// ============================================================
// NetworkHandle — 网络请求句柄
// ============================================================

/**
 * @brief 网络请求句柄（RAII）
 *
 * 构造时调用 RequestNetwork()，析构时自动 ReleaseNetwork()，
 * 防止忘记释放导致网络无法进入睡眠。
 */
class NetworkHandle {
public:
    explicit NetworkHandle(NetworkId networkId);
    ~NetworkHandle();

    NetworkHandle(const NetworkHandle&) = delete;
    NetworkHandle& operator=(const NetworkHandle&) = delete;
    NetworkHandle(NetworkHandle&& other) noexcept;

    bool IsValid() const { return valid_; }
    NetworkId GetNetworkId() const { return networkId_; }

private:
    NetworkId networkId_;
    bool valid_{false};
};

// ============================================================
// NetworkManagement — NM 主控类
// ============================================================

/**
 * @brief 网络管理主控类（单例）
 *
 * 职责：
 * 1. 管理逻辑网络的激活 / 释放
 * 2. 监控网络状态变化并通知订阅者
 * 3. 与 ara::com SOME/IP SD 协同（网络激活后才能服务发现）
 * 4. 协同 MCU 侧 ComM 进行 CAN/ETH 网络管理
 *
 * 对应 IPC-ARCH-001 §2：
 *   SOC ETH IPC 网络（192.168.100.1/30）在 SOC 启动后激活，
 *   进入 NORMAL 模式后 SOME/IP SD 开始工作。
 */
class NetworkManagement {
public:
    /// 网络状态变化回调
    using NetworkStateCallback = std::function<void(NetworkId netId, NetworkState oldState, NetworkState newState)>;

    /**
     * @brief 获取 NM 单例
     */
    static NetworkManagement& GetInstance();

    NetworkManagement(const NetworkManagement&) = delete;
    NetworkManagement& operator=(const NetworkManagement&) = delete;

    /**
     * @brief 请求激活网络（SWS_NM_00030）
     * @param networkId  逻辑网络 ID
     * @return           请求结果
     *
     * 激活后网络进入 Normal 模式，允许 SOME/IP SD 和数据通信。
     * 内部维护引用计数，多方请求时网络保持激活直到全部释放。
     */
    NmRequestResult RequestNetwork(NetworkId networkId);

    /**
     * @brief 释放网络（SWS_NM_00035）
     * @param networkId  逻辑网络 ID
     *
     * 引用计数归零后，网络进入 PrepareBusSleep → BusSleep 流程。
     */
    NmRequestResult ReleaseNetwork(NetworkId networkId);

    /**
     * @brief 获取网络当前状态
     */
    NetworkState GetNetworkState(NetworkId networkId) const;

    /**
     * @brief 注册网络状态变化通知
     */
    void RegisterNetworkStateCallback(NetworkStateCallback cb);

    /**
     * @brief 初始化 NM（配置网络列表、IP 等）
     */
    bool Initialize();

    /**
     * @brief 关闭 NM
     */
    void Shutdown();

    /**
     * @brief 查询当前活跃网络列表
     */
    std::vector<NetworkId> GetActiveNetworks() const;

private:
    NetworkManagement() = default;

    struct NetworkRecord {
        NetworkId id;
        std::atomic<NetworkState> state{NetworkState::kBusSleep};
        std::atomic<uint32_t> refCount{0};
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint16_t, NetworkRecord> networks_;
    NetworkStateCallback stateCallback_;
};

} // namespace nm
} // namespace ara

#endif // ARA_NM_NETWORK_MANAGEMENT_H
