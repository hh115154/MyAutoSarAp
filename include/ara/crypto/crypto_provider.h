/**
 * @file crypto_provider.h
 * @brief ara::crypto — Cryptography Service
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Cryptography
 * Ref: AUTOSAR_SWS_CryptographyAPI (R25-11)
 *
 * 设计依据：SOC-SW-001 §3.9 加密服务
 * 提供：AES-128/256（GCM/CBC）、RSA-2048、ECC P-256、
 *       HMAC-SHA256、随机数生成、密钥管理 KMS
 */

#ifndef ARA_CRYPTO_CRYPTO_PROVIDER_H
#define ARA_CRYPTO_CRYPTO_PROVIDER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ara {
namespace crypto {

// ============================================================
// 类型别名和基础类型
// ============================================================

using ByteVector = std::vector<uint8_t>;

/**
 * @brief 加密算法 ID（参考 AUTOSAR SWS_CRYPTO AlgorithmID）
 */
enum class AlgorithmId : uint32_t {
    // 对称加密
    kAES128_CBC    = 0x0001,  ///< AES-128 CBC 模式
    kAES256_CBC    = 0x0002,  ///< AES-256 CBC 模式
    kAES128_GCM    = 0x0003,  ///< AES-128 GCM 认证加密（推荐）
    kAES256_GCM    = 0x0004,  ///< AES-256 GCM（OTA 数据保护）
    // 非对称加密
    kRSA2048_OAEP  = 0x0101,  ///< RSA-2048 OAEP 加密
    kECDSA_P256    = 0x0201,  ///< ECDSA P-256 签名/验证
    kECDH_P256     = 0x0202,  ///< ECDH P-256 密钥协商
    // 哈希/MAC
    kSHA256        = 0x0301,  ///< SHA-256 哈希
    kSHA384        = 0x0302,  ///< SHA-384 哈希
    kHMAC_SHA256   = 0x0401,  ///< HMAC-SHA256（E2E 保护）
    // 随机数
    kCTR_DRBG      = 0x0501   ///< NIST SP 800-90A CTR_DRBG
};

/**
 * @brief 密钥用途标志（可位或）
 */
enum class KeyUsage : uint32_t {
    kEncrypt       = 0x01,
    kDecrypt       = 0x02,
    kSign          = 0x04,
    kVerify        = 0x08,
    kKeyDerivation = 0x10,
    kWrap          = 0x20,
    kUnwrap        = 0x40
};

inline KeyUsage operator|(KeyUsage a, KeyUsage b) {
    return static_cast<KeyUsage>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/**
 * @brief 密钥存储位置
 */
enum class KeySlotType : uint8_t {
    kHsm           = 0,  ///< 硬件安全模块（SOC HSM）
    kProtectedRam  = 1,  ///< 受保护内存（ASIL-B 分区）
    kVolatile      = 2   ///< 易失性内存（运行时临时密钥）
};

/**
 * @brief 加密操作结果
 */
enum class CryptoResult : uint8_t {
    kSuccess          = 0,
    kInvalidKey       = 1,
    kInvalidAlgorithm = 2,
    kInvalidInput     = 3,
    kBufferTooSmall   = 4,
    kAuthFailed       = 5,  ///< GCM/CCM 认证标签验证失败
    kKeyNotFound      = 6,
    kHsmError         = 7,
    kError            = 255
};

// ============================================================
// CryptoKey — 密钥句柄
// ============================================================

/**
 * @brief 密钥句柄（不透明引用）
 *
 * 密钥值不离开 Crypto Provider 范围，
 * 应用层通过 CryptoKey 引用密钥，不直接接触密钥材料。
 */
class CryptoKey {
public:
    using KeyId = uint32_t;

    CryptoKey() = default;
    explicit CryptoKey(KeyId id, AlgorithmId algId, KeyUsage usage, KeySlotType slot)
        : id_(id), algId_(algId), usage_(usage), slot_(slot), valid_(true) {}

    bool IsValid() const { return valid_; }
    KeyId GetId() const { return id_; }
    AlgorithmId GetAlgorithmId() const { return algId_; }
    KeySlotType GetSlotType() const { return slot_; }

private:
    KeyId id_{0};
    AlgorithmId algId_{AlgorithmId::kAES256_GCM};
    KeyUsage usage_{KeyUsage::kEncrypt};
    KeySlotType slot_{KeySlotType::kVolatile};
    bool valid_{false};
};

// ============================================================
// SymmetricCipher — 对称加密接口
// ============================================================

/**
 * @brief 对称加密/解密接口（SWS_CRYPTO_00070）
 */
class SymmetricCipher {
public:
    virtual ~SymmetricCipher() = default;

    /**
     * @brief 加密（AES-GCM 认证加密，含 AAD）
     * @param key        加密密钥句柄
     * @param iv         初始化向量（GCM: 12 bytes）
     * @param plaintext  明文数据
     * @param aad        附加认证数据（可为空）
     * @param ciphertext [out] 密文输出（含 GCM Auth Tag 16B）
     * @return           操作结果
     */
    virtual CryptoResult Encrypt(
        const CryptoKey& key,
        const ByteVector& iv,
        const ByteVector& plaintext,
        const ByteVector& aad,
        ByteVector& ciphertext) = 0;

    /**
     * @brief 解密（AES-GCM 认证解密，验证 Auth Tag）
     * @param key        解密密钥句柄
     * @param iv         初始化向量
     * @param ciphertext 密文（含 GCM Auth Tag 16B）
     * @param aad        附加认证数据
     * @param plaintext  [out] 明文输出
     * @return           kAuthFailed 表示数据被篡改
     */
    virtual CryptoResult Decrypt(
        const CryptoKey& key,
        const ByteVector& iv,
        const ByteVector& ciphertext,
        const ByteVector& aad,
        ByteVector& plaintext) = 0;
};

// ============================================================
// MessageAuthCode — HMAC 接口
// ============================================================

/**
 * @brief 消息认证码接口（SWS_CRYPTO_00080）
 *
 * 用于 E2E Profile 2 保护（HMAC-SHA256），
 * 对应 IPC-ARCH-001 §3 SPI 信号帧保护。
 */
class MessageAuthCode {
public:
    virtual ~MessageAuthCode() = default;

    /**
     * @brief 生成 MAC
     * @param key    HMAC 密钥句柄
     * @param data   输入数据
     * @param mac    [out] MAC 值（HMAC-SHA256: 32 bytes）
     */
    virtual CryptoResult Generate(
        const CryptoKey& key,
        const ByteVector& data,
        ByteVector& mac) = 0;

    /**
     * @brief 验证 MAC
     * @return kAuthFailed 表示 MAC 不匹配
     */
    virtual CryptoResult Verify(
        const CryptoKey& key,
        const ByteVector& data,
        const ByteVector& expectedMac) = 0;
};

// ============================================================
// CryptoProvider — 加密服务主入口
// ============================================================

/**
 * @brief 加密服务提供者（单例工厂）
 *
 * 对应 SOC-SW-001 §3.9：
 *   - SOC HSM 硬件安全模块集成
 *   - OTA 固件签名校验（ECDSA P-256）
 *   - SOME/IP TLS 握手（AES-256-GCM + ECDH P-256）
 *   - ara::iam 证书绑定（X.509）
 */
class CryptoProvider {
public:
    /**
     * @brief 获取 CryptoProvider 单例
     */
    static CryptoProvider& GetInstance();

    CryptoProvider(const CryptoProvider&) = delete;
    CryptoProvider& operator=(const CryptoProvider&) = delete;

    /**
     * @brief 加载密钥（从 HSM 或 KMS 读取）
     * @param keyId      密钥 ID（KMS 注册）
     * @param algId      算法 ID
     * @param usage      密钥用途
     * @return           密钥句柄（IsValid() = false 表示加载失败）
     */
    CryptoKey LoadKey(CryptoKey::KeyId keyId, AlgorithmId algId, KeyUsage usage);

    /**
     * @brief 生成随机密钥
     */
    CryptoKey GenerateKey(AlgorithmId algId, KeyUsage usage, KeySlotType slot = KeySlotType::kVolatile);

    /**
     * @brief 生成随机字节序列（NIST CTR_DRBG）
     * @param length     字节数
     * @param output     [out] 随机字节
     */
    CryptoResult GenerateRandom(size_t length, ByteVector& output);

    /**
     * @brief 创建对称加密器
     */
    std::unique_ptr<SymmetricCipher> CreateSymmetricCipher(AlgorithmId algId);

    /**
     * @brief 创建 MAC 计算器
     */
    std::unique_ptr<MessageAuthCode> CreateMessageAuthCode(AlgorithmId algId);

    /**
     * @brief 验证数字签名（ECDSA P-256，用于 OTA 固件校验）
     * @param publicKey  公钥句柄
     * @param data       被签名的数据
     * @param signature  签名值（DER 编码）
     * @return           验证结果
     */
    CryptoResult VerifySignature(
        const CryptoKey& publicKey,
        const ByteVector& data,
        const ByteVector& signature);

    /**
     * @brief 计算哈希值
     */
    CryptoResult Hash(AlgorithmId algId, const ByteVector& data, ByteVector& digest);

private:
    CryptoProvider() = default;
};

} // namespace crypto
} // namespace ara

#endif // ARA_CRYPTO_CRYPTO_PROVIDER_H
