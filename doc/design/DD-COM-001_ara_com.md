# ara::com — Communication Management 详细设计文档

**文档编号**：DD-COM-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_CommunicationManagement  
**关联架构文档**：SOC-SW-001 §3.3、IPC-ARCH-001 §2  
**功能安全等级**：QM（通信数据安全由 E2E Profile 2 保障）

---

## 1. 功能概述

`ara::com` 是 AUTOSAR AP R25-11 的通信管理功能簇，为 AA（Adaptive Application）提供**服务化通信**的统一 C++ 接口，屏蔽底层通信绑定细节。

### 1.1 核心职责

| 功能 | 描述 | 本项目实现 |
|------|------|-----------|
| 服务发现（SD） | SOME/IP SD，UDP 30490 广播 | `SomeIpServiceDiscovery` 单例 |
| 事件发布/订阅 | Pub/Sub，UDP 30500-30599 | `EventPublisher<T>` / `EventSubscriber<T>` |
| 方法调用 | Request/Response，TCP 30600-30699 | `MethodResult<T>` Future 模式 |
| Field 访问 | Get/Set/Notify，可带持久化 | 基于 Event + `ara::per` |
| E2E 保护 | Profile 2（HMAC-SHA256 + 序号） | `E2ECheckStatus` 枚举 |
| 服务生命周期 | OfferService / StopOfferService | `SkeletonBase` |

### 1.2 SOME/IP 服务目录（IPC-ARCH-001 §2.4）

| 服务名 | Service ID | 模式 | 方向 | 端口 |
|--------|-----------|------|------|------|
| VehicleSignalService | 0x1001 | Event/Field | MCU→SOC | UDP 30501 |
| SafetyStatusService | 0x1002 | Event | MCU→SOC | UDP 30502 |
| PowerModeService | 0x1003 | Method+Event | 双向 | TCP 30601 |
| DiagnosticProxyService | 0x1004 | Method | 双向 | TCP 30602 |
| OTATransferService | 0x1005 | Method(TCP) | SOC→MCU | TCP 30701 |
| HMICommandService | 0x1006 | Method | SOC→MCU | TCP 30603 |
| NetworkStatusService | 0x1007 | Event | MCU→SOC | UDP 30503 |

---

## 2. 架构设计

### 2.1 分层架构

```
┌──────────────────────────────────────────────────────┐
│               Adaptive Application (AA)               │
│   VehicleSignalService SWC │ SafetyMonitor SWC        │
│   IPCBridge SWC            │ OTA Client SWC           │
└──────────┬──────────────────────────────┬─────────────┘
           │  ara::com C++ API             │
┌──────────▼──────────────────────────────▼─────────────┐
│              ara::com (Communication Management)       │
│  ┌────────────────┐  ┌─────────────────────────────┐  │
│  │  ServiceFinder  │  │  SomeIpServiceDiscovery     │  │
│  │  (FindService)  │  │  (SD, UDP 30490)            │  │
│  └────────────────┘  └─────────────────────────────┘  │
│  ┌────────────────┐  ┌─────────────────────────────┐  │
│  │  ProxyBase     │  │  SkeletonBase               │  │
│  │  (Consumer)    │  │  (Provider)                 │  │
│  └────────────────┘  └─────────────────────────────┘  │
│  ┌────────────────────────────────────────────────┐    │
│  │         SOME/IP Binding Layer                  │    │
│  │  EventPublisher<T>  │  EventSubscriber<T>       │    │
│  │  MethodResult<T>    │  SomeIpMessage 序列化      │    │
│  └────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────┘
           │
┌──────────▼──────────────────────────────────────────┐
│         Linux TCP/IP Stack（UDP/TCP）                  │
│         SOC ETH IPC: 192.168.100.1 ↔ MCU: .100.2     │
└──────────────────────────────────────────────────────┘
```

### 2.2 SOME/IP 消息帧格式

```
┌──────────┬──────────┬────────┬──────────┬──────────────┐
│ ServiceID│ MethodID │ Length │ ClientID │  Payload     │
│ 2 bytes  │ 2 bytes  │ 4 bytes│ SessionID│  N bytes     │
│          │          │        │ 4 bytes  │              │
│          │          │        │ ProtoVer │              │
│          │          │        │ IfaceVer │              │
│          │          │        │ MsgType  │              │
│          │          │        │ RetCode  │              │
│          │          │        │ （共 8B） │              │
└──────────┴──────────┴────────┴──────────┴──────────────┘
        Header = 16 bytes 固定
```

---

## 3. 接口定义

### 3.1 类型（`types.h`）

```cpp
class InstanceIdentifier;           // 服务实例标识符
enum class FindServiceResult;       // 服务发现结果
enum class SubscriptionState;       // 订阅状态
enum class E2ECheckStatus;          // E2E 检查结果
using ServiceHandleId = uint32_t;   // 服务句柄
```

### 3.2 ProxyBase（`proxy_base.h`）

```cpp
class ProxyBase {
    explicit ProxyBase(const InstanceIdentifier& instance);
    const InstanceIdentifier& GetInstanceId() const;
};

template<typename T>
class ServiceFinder {
    static vector<HandleType> FindService(const InstanceIdentifier& instance);
    static ServiceHandleId StartFindService(FindServiceCallback, const InstanceIdentifier&);
    static void StopFindService(ServiceHandleId);
};
```

### 3.3 SkeletonBase（`skeleton_base.h`）

```cpp
class SkeletonBase {
    explicit SkeletonBase(const InstanceIdentifier& instance);
    virtual void OfferService();
    virtual void StopOfferService();
    bool IsOffered() const;
};
```

### 3.4 SomeIpServiceDiscovery（`someip_binding.h`）

```cpp
class SomeIpServiceDiscovery {  // 单例
    void OfferService(const ServiceDescriptor&);
    void StopOfferService(uint16_t serviceId);
    vector<ServiceDescriptor> FindServices(uint16_t serviceId) const;
    void SubscribeServiceAvailability(uint16_t serviceId, Callback);
};
```

---

## 4. 状态机

### 4.1 服务发现状态机

```
       OfferService()
            │
    ┌───────▼────────┐
    │   SD Offering  │ ──── StopOfferService() ──→ Offline
    └───────┬────────┘
            │ FindService() by Consumer
    ┌───────▼────────┐
    │   Subscribed   │ ──── Unsubscribe() ──────→ Not Subscribed
    └────────────────┘
```

### 4.2 EventSubscriber 订阅状态

```
kNotSubscribed → Subscribe() → kSubscriptionPending → (SD 握手完成) → kSubscribed
kSubscribed    → Unsubscribe() → kNotSubscribed
kSubscribed    → 服务下线 → kNotSubscribed（并触发 callback）
```

---

## 5. 与架构文档映射

| 设计文档 | 章节 | 对应实现 |
|---------|------|---------|
| IPC-ARCH-001 | §2.1 SOME/IP 方案 | `someip_binding.h/cpp` |
| IPC-ARCH-001 | §2.3 IP/端口规划 | `kServiceCatalog[]` 静态表 |
| IPC-ARCH-001 | §2.5 消息帧格式 | `SomeIpHeader` / `SomeIpMessage` |
| SOC-SW-001 | §3.3 ara::com | `ProxyBase` / `SkeletonBase` |
| SOC-SW-001 | §4 SWC | `VehicleSignalServiceProxy` |

---

## 6. 单元测试

测试文件：`tests/unit/test_ara_com.cpp`

| 测试用例 | 验证点 |
|---------|--------|
| `InstanceIdentifier_Equality` | `operator==` / `operator!=` |
| `InstanceIdentifier_Copy` | 拷贝构造不共享状态 |
| `ProxyBase_GetInstanceId` | 返回正确实例 ID |
| `SkeletonBase_OfferStop` | OfferService / StopOfferService 切换 |
| `SomeIpMessage_Serialize` | 16 字节头序列化 |
| `SomeIpMessage_Deserialize` | 反序列化一致性 |
| `SomeIpSD_OfferFind` | Offer 后 Find 能查到 |
| `SomeIpSD_StopOffer` | StopOffer 后 Find 为空 |
| `SubscriptionState_Enum` | 枚举值正确 |
| `E2ECheckStatus_Enum` | 枚举值正确 |

---

## 7. 已知限制与未来工作

1. **无真实 Socket 实现**：当前为模拟绑定，不发送/接收真实 UDP/TCP 报文
2. **E2E 完整实现**：Profile 2 CRC 计算应集成 `ara::crypto` HMAC-SHA256
3. **TLS 绑定**：SOME/IP over TLS 需 `ara::crypto` TLS/DTLS 层
4. **SD 超时重试**：生产实现需添加 SD 重试机制和超时计时器
