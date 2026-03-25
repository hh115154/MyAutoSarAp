# ara::nm — Network Management 详细设计文档

**文档编号**：DD-NM-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_NetworkManagement  
**关联架构文档**：SOC-SW-001 §3.5、IPC-ARCH-001 §2.3  
**功能安全等级**：QM（网络可用性，不直接影响安全状态）

---

## 1. 功能概述

`ara::nm` 管理 SOC 侧**逻辑网络**的生命周期，控制以太网接口的激活/休眠，与 SOME/IP SD 服务发现协同，确保网络就绪后通信才开始。

### 1.1 SOC 网络规划（IPC-ARCH-001 §2.3）

| 网络 ID | 描述 | IP 地址 | 用途 |
|--------|------|---------|------|
| kEthIPC | SOC↔MCU 芯片间以太网 | 192.168.100.1/30 | SOME/IP IPC 通信 |
| kEthExternal | 外部以太网 | DHCP | ADAS/Telematics |
| kEthDoIP | 诊断以太网 | 192.168.100.1:13400 | OBD/DoIP 诊断 |
| kLoopback | 回环 | 127.0.0.1 | 内部测试 |

---

## 2. 架构设计

### 2.1 NM 与 SOME/IP 协同

```
AA 进程请求通信
      │
      ▼
NetworkHandle(kEthIPC)  ← RAII，构造时 RequestNetwork()
      │
      ▼
NetworkManagement::RequestNetwork(kEthIPC)
      │ refCount: 0→1
      ▼
网络状态 BusSleep → Normal
      │
      ▼
SOME/IP SD 可以开始工作（IP 已就绪）
      │
      ▼
SomeIpServiceDiscovery::OfferService / FindService

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
AA 进程退出
      │
      ▼
~NetworkHandle()  ← 析构时 ReleaseNetwork()
      │ refCount: 1→0
      ▼
网络状态 Normal → BusSleep
```

### 2.2 引用计数策略

多个 AA 可以同时持有同一网络的 `NetworkHandle`。  
只要引用计数 > 0，网络保持激活。  
最后一个 `NetworkHandle` 析构后，网络进入 BusSleep。

---

## 3. 接口定义

### 3.1 NetworkHandle（RAII）

```cpp
class NetworkHandle {
    explicit NetworkHandle(NetworkId networkId);  // 构造：RequestNetwork
    ~NetworkHandle();                              // 析构：ReleaseNetwork
    bool IsValid() const;
    NetworkId GetNetworkId() const;
};
```

### 3.2 NetworkManagement（单例）

```cpp
class NetworkManagement {
    static NetworkManagement& GetInstance();

    NmRequestResult RequestNetwork(NetworkId networkId);
    NmRequestResult ReleaseNetwork(NetworkId networkId);
    NetworkState GetNetworkState(NetworkId networkId) const;
    void RegisterNetworkStateCallback(NetworkStateCallback cb);
    bool Initialize();
    void Shutdown();
    vector<NetworkId> GetActiveNetworks() const;
};
```

---

## 4. 状态机

```
BusSleep ──→ RequestNetwork() ──→ Normal
Normal   ──→ ReleaseNetwork() (refCount=0) ──→ PrepareBusSleep ──→ BusSleep
Normal   ──→ 链路故障 ──→ Error
Error    ──→ 恢复 ──→ BusSleep（重新初始化）
```

---

## 5. 与架构文档映射

| IPC-ARCH-001 | 章节 | NM 对应实现 |
|-------------|------|-----------|
| ETH IP 规划 | §2.3 | `NetworkId::kEthIPC` (192.168.100.1/30) |
| SD 端口 30490 | §2.3 | SOME/IP SD 在 kNormal 状态后可用 |
| DoIP 端口 13400 | §2.3 | `NetworkId::kEthDoIP` |
