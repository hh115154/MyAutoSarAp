# ara::crypto — Cryptography Service 详细设计文档

**文档编号**：DD-CRYPTO-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_CryptographyAPI  
**关联架构文档**：SOC-SW-001 §3.9  
**功能安全等级**：ASIL-B（密钥保护、OTA 完整性校验）

---

## 1. 功能概述

`ara::crypto` 提供**密码学服务**，集成 SOC HSM（硬件安全模块），支持：

| 算法 | 用途 | 密钥长度 |
|------|------|---------|
| AES-128/256-GCM | OTA 数据加密、SOME/IP 载荷加密 | 128/256 bit |
| AES-128/256-CBC | 历史兼容 | 128/256 bit |
| RSA-2048-OAEP | 密钥封装 | 2048 bit |
| ECDSA-P256 | OTA 固件签名校验 | 256 bit |
| ECDH-P256 | TLS 密钥协商 | 256 bit |
| HMAC-SHA256 | E2E Profile 2、消息认证 | 256 bit |
| SHA-256/384 | 哈希/完整性校验 | - |
| CTR-DRBG | 随机数生成（NIST SP 800-90A） | - |

---

## 2. 架构设计

### 2.1 加密服务分层

```
┌──────────────────────────────────────────────────────┐
│                     AA / SWC 层                       │
│  OTA Client SWC: VerifySignature(ECDSA-P256)         │
│  IPCBridge SWC:  MAC.Generate(HMAC-SHA256)           │
│  VehicleSignal:  CryptoProvider.Hash(SHA256)         │
└──────────────────────┬───────────────────────────────┘
                       │ ara::crypto C++ API
┌──────────────────────▼───────────────────────────────┐
│              CryptoProvider（单例工厂）                │
│  ┌────────────────┐  ┌────────────────────────────┐  │
│  │ SymmetricCipher│  │    MessageAuthCode          │  │
│  │ AES-GCM/CBC    │  │    HMAC-SHA256              │  │
│  └────────────────┘  └────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐     │
│  │        Key Management Store (KMS)            │     │
│  │  HSM 存储 (kHsm) │ 受保护内存 │ 易失性密钥    │     │
│  └──────────────────────────────────────────────┘     │
└──────────────────────┬───────────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────────┐
│          SOC 硬件安全模块 (HSM)                         │
│  BL1→BL2→BL3 Secure Boot │ ECC Key 生成               │
│  AES 硬件加速 │ TRNG 真随机数                          │
└──────────────────────────────────────────────────────┘
```

---

## 3. 接口定义

### 3.1 CryptoKey（密钥句柄）

```cpp
class CryptoKey {
    // 不透明引用，密钥材料不离开 CryptoProvider 作用域
    bool IsValid() const;
    KeyId GetId() const;
    AlgorithmId GetAlgorithmId() const;
    KeySlotType GetSlotType() const;  // kHsm / kProtectedRam / kVolatile
};
```

### 3.2 CryptoProvider（单例）

```cpp
class CryptoProvider {
    CryptoKey LoadKey(KeyId, AlgorithmId, KeyUsage);        // 从 HSM/KMS 加载
    CryptoKey GenerateKey(AlgorithmId, KeyUsage, KeySlotType); // 生成新密钥

    CryptoResult GenerateRandom(size_t length, ByteVector& output);

    unique_ptr<SymmetricCipher>    CreateSymmetricCipher(AlgorithmId);
    unique_ptr<MessageAuthCode>    CreateMessageAuthCode(AlgorithmId);

    CryptoResult VerifySignature(const CryptoKey& pub, const ByteVector& data, const ByteVector& sig);
    CryptoResult Hash(AlgorithmId, const ByteVector& data, ByteVector& digest);
};
```

### 3.3 SymmetricCipher（AES-GCM）

```cpp
class SymmetricCipher {
    CryptoResult Encrypt(key, iv, plaintext, aad, ciphertext); // GCM: 输出含 16B Auth Tag
    CryptoResult Decrypt(key, iv, ciphertext, aad, plaintext); // GCM: 验证 Auth Tag
};
```

---

## 4. 密钥管理策略（SOC-SW-001 §3.9）

| 密钥类型 | 存储位置 | 生命周期 | 用途 |
|---------|---------|---------|------|
| OEM Root CA 公钥 | HSM（固化） | 永久 | OTA 固件签名验证 |
| SOME/IP TLS 会话密钥 | kVolatile | 连接期间 | TLS 1.3 加密 |
| E2E HMAC 密钥 | kProtectedRam | 运行期间 | SPI 帧认证 |
| OTA 对称密钥 | kHsm | OTA 期间 | AES-256-GCM 加密 |

---

## 5. E2E Profile 2 集成（IPC-ARCH-001 §3.4）

```
发送方（SOC IPCBridge）：
  1. 准备 SPI IPC 帧 payload
  2. MAC = HMAC-SHA256(key, payload ++ counter)
  3. 帧：[SOF][FrameCtrl][Len][Payload][Counter][MAC-4B][CRC16]

接收方（MCU AUTOSAR CP）：
  1. 校验 CRC16
  2. 校验 MAC（AUTOSAR E2E Profile 2 规范 CRC+Counter 模式）
  3. 检查 Counter 连续性（防重放）
```

---

## 6. 当前实现说明

> **注意**：当前实现为**模拟实现**（测试用），未链接真实 HSM SDK：
> - AES-GCM：XOR 模拟（不安全，仅演示接口）
> - HMAC-SHA256：多项式哈希模拟
> - 随机数：`std::random_device` + Mersenne Twister
>
> **生产部署**：需替换为 OpenSSL 3.x / mbedTLS 4.x + SOC HSM SDK。
