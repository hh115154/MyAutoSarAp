/**
 * @file crypto.cpp
 * @brief ara::crypto — Cryptography Service 实现
 *
 * 模拟实现（无真实 HSM 调用）：
 * - AES-GCM 使用简化的 XOR 加密（演示接口，生产需接 HSM SDK）
 * - HMAC-SHA256 使用 std::hash 模拟（演示接口）
 * - 随机数生成使用 std::random_device
 *
 * 生产环境应替换为 OpenSSL 3.x / mbedTLS / HSM SDK 实现。
 */

#include "ara/crypto/crypto_provider.h"
#include <random>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace ara {
namespace crypto {

// ============================================================
// 内部密钥存储（模拟 KMS）
// ============================================================

namespace {

struct KeyRecord {
    CryptoKey::KeyId id;
    AlgorithmId algId;
    KeyUsage usage;
    KeySlotType slot;
    ByteVector material; // 密钥材料
};

static std::mutex g_keyStoreMutex;
static std::unordered_map<CryptoKey::KeyId, KeyRecord> g_keyStore;
static std::atomic<CryptoKey::KeyId> g_nextKeyId{1};

static std::random_device g_randDev;
static std::mt19937_64 g_rng(g_randDev());

} // anonymous namespace

// ============================================================
// SymmetricCipher 实现（AES-GCM 模拟）
// ============================================================

class SymmetricCipherImpl : public SymmetricCipher {
public:
    explicit SymmetricCipherImpl(AlgorithmId algId) : algId_(algId) {}

    CryptoResult Encrypt(
        const CryptoKey& key,
        const ByteVector& iv,
        const ByteVector& plaintext,
        const ByteVector& /*aad*/,
        ByteVector& ciphertext) override
    {
        if (!key.IsValid()) return CryptoResult::kInvalidKey;
        if (iv.empty() || plaintext.empty()) return CryptoResult::kInvalidInput;

        std::lock_guard<std::mutex> lock(g_keyStoreMutex);
        auto it = g_keyStore.find(key.GetId());
        if (it == g_keyStore.end()) return CryptoResult::kKeyNotFound;

        const ByteVector& keyMat = it->second.material;
        // 模拟加密：XOR with key material + IV（演示用）
        ciphertext.resize(plaintext.size() + 16); // +16 for auth tag simulation
        for (size_t i = 0; i < plaintext.size(); ++i) {
            ciphertext[i] = plaintext[i] ^ keyMat[i % keyMat.size()] ^ iv[i % iv.size()];
        }
        // 模拟 16 字节 Auth Tag（全 0xAB 演示）
        std::fill(ciphertext.begin() + plaintext.size(), ciphertext.end(), 0xAB);

        return CryptoResult::kSuccess;
    }

    CryptoResult Decrypt(
        const CryptoKey& key,
        const ByteVector& iv,
        const ByteVector& ciphertext,
        const ByteVector& /*aad*/,
        ByteVector& plaintext) override
    {
        if (!key.IsValid()) return CryptoResult::kInvalidKey;
        if (iv.empty() || ciphertext.size() < 16) return CryptoResult::kInvalidInput;

        // 验证 Auth Tag（模拟：检查最后 16 字节是否全为 0xAB）
        for (size_t i = ciphertext.size() - 16; i < ciphertext.size(); ++i) {
            if (ciphertext[i] != 0xAB) return CryptoResult::kAuthFailed;
        }

        std::lock_guard<std::mutex> lock(g_keyStoreMutex);
        auto it = g_keyStore.find(key.GetId());
        if (it == g_keyStore.end()) return CryptoResult::kKeyNotFound;

        const ByteVector& keyMat = it->second.material;
        size_t dataLen = ciphertext.size() - 16;
        plaintext.resize(dataLen);
        for (size_t i = 0; i < dataLen; ++i) {
            plaintext[i] = ciphertext[i] ^ keyMat[i % keyMat.size()] ^ iv[i % iv.size()];
        }
        return CryptoResult::kSuccess;
    }

private:
    AlgorithmId algId_;
};

// ============================================================
// MessageAuthCode 实现（HMAC-SHA256 模拟）
// ============================================================

class MessageAuthCodeImpl : public MessageAuthCode {
public:
    CryptoResult Generate(
        const CryptoKey& key,
        const ByteVector& data,
        ByteVector& mac) override
    {
        if (!key.IsValid()) return CryptoResult::kInvalidKey;

        std::lock_guard<std::mutex> lock(g_keyStoreMutex);
        auto it = g_keyStore.find(key.GetId());
        if (it == g_keyStore.end()) return CryptoResult::kKeyNotFound;

        // 模拟 HMAC：简单哈希（生产需替换 OpenSSL HMAC_SHA256）
        size_t h = 0x12345678;
        for (uint8_t b : data) {
            h = h * 31 + b;
        }
        for (uint8_t b : it->second.material) {
            h = h * 37 + b;
        }
        // 输出 32 字节模拟 MAC
        mac.resize(32, 0);
        for (size_t i = 0; i < 8; ++i) {
            mac[i * 4 + 0] = static_cast<uint8_t>((h >> 56) & 0xFF);
            mac[i * 4 + 1] = static_cast<uint8_t>((h >> 48) & 0xFF);
            mac[i * 4 + 2] = static_cast<uint8_t>((h >> 40) & 0xFF);
            mac[i * 4 + 3] = static_cast<uint8_t>((h >> 32) & 0xFF);
            h = h * 6364136223846793005ULL + 1442695040888963407ULL; // LCG
        }
        return CryptoResult::kSuccess;
    }

    CryptoResult Verify(
        const CryptoKey& key,
        const ByteVector& data,
        const ByteVector& expectedMac) override
    {
        ByteVector computedMac;
        CryptoResult r = Generate(key, data, computedMac);
        if (r != CryptoResult::kSuccess) return r;
        if (computedMac != expectedMac) return CryptoResult::kAuthFailed;
        return CryptoResult::kSuccess;
    }
};

// ============================================================
// CryptoProvider 实现
// ============================================================

CryptoProvider& CryptoProvider::GetInstance()
{
    static CryptoProvider instance;
    return instance;
}

CryptoKey CryptoProvider::LoadKey(
    CryptoKey::KeyId keyId,
    AlgorithmId algId,
    KeyUsage usage)
{
    std::lock_guard<std::mutex> lock(g_keyStoreMutex);
    auto it = g_keyStore.find(keyId);
    if (it != g_keyStore.end()) {
        return CryptoKey(keyId, algId, usage, it->second.slot);
    }
    return CryptoKey{}; // 未找到，返回无效密钥
}

CryptoKey CryptoProvider::GenerateKey(
    AlgorithmId algId,
    KeyUsage usage,
    KeySlotType slot)
{
    size_t keyLen = 32; // AES-256 / ECDH-P256 默认 32 字节
    if (algId == AlgorithmId::kAES128_CBC || algId == AlgorithmId::kAES128_GCM) {
        keyLen = 16;
    }

    ByteVector material(keyLen);
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    for (auto& b : material) {
        b = static_cast<uint8_t>(dist(g_rng));
    }

    CryptoKey::KeyId newId = g_nextKeyId.fetch_add(1);

    {
        std::lock_guard<std::mutex> lock(g_keyStoreMutex);
        KeyRecord rec{newId, algId, usage, slot, std::move(material)};
        g_keyStore.emplace(newId, std::move(rec));
    }

    return CryptoKey(newId, algId, usage, slot);
}

CryptoResult CryptoProvider::GenerateRandom(size_t length, ByteVector& output)
{
    output.resize(length);
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    for (auto& b : output) {
        b = static_cast<uint8_t>(dist(g_rng));
    }
    return CryptoResult::kSuccess;
}

std::unique_ptr<SymmetricCipher> CryptoProvider::CreateSymmetricCipher(AlgorithmId algId)
{
    return std::make_unique<SymmetricCipherImpl>(algId);
}

std::unique_ptr<MessageAuthCode> CryptoProvider::CreateMessageAuthCode(AlgorithmId /*algId*/)
{
    return std::make_unique<MessageAuthCodeImpl>();
}

CryptoResult CryptoProvider::VerifySignature(
    const CryptoKey& publicKey,
    const ByteVector& /*data*/,
    const ByteVector& /*signature*/)
{
    // 模拟：有效公钥始终验签成功（生产需替换 OpenSSL ECDSA 验签）
    if (!publicKey.IsValid()) return CryptoResult::kInvalidKey;
    return CryptoResult::kSuccess;
}

CryptoResult CryptoProvider::Hash(
    AlgorithmId /*algId*/,
    const ByteVector& data,
    ByteVector& digest)
{
    // 模拟 SHA-256：输出 32 字节（生产替换 OpenSSL SHA256）
    digest.resize(32, 0);
    size_t h = 0xDEADBEEF;
    for (uint8_t b : data) {
        h = h * 31 + b;
    }
    for (size_t i = 0; i < 4; ++i) {
        digest[i * 8 + 0] = static_cast<uint8_t>((h >> 56) & 0xFF);
        digest[i * 8 + 1] = static_cast<uint8_t>((h >> 48) & 0xFF);
        digest[i * 8 + 2] = static_cast<uint8_t>((h >> 40) & 0xFF);
        digest[i * 8 + 3] = static_cast<uint8_t>((h >> 32) & 0xFF);
        digest[i * 8 + 4] = static_cast<uint8_t>((h >> 24) & 0xFF);
        digest[i * 8 + 5] = static_cast<uint8_t>((h >> 16) & 0xFF);
        digest[i * 8 + 6] = static_cast<uint8_t>((h >>  8) & 0xFF);
        digest[i * 8 + 7] = static_cast<uint8_t>((h >>  0) & 0xFF);
        h = h * 6364136223846793005ULL + 1442695040888963407ULL;
    }
    return CryptoResult::kSuccess;
}

} // namespace crypto
} // namespace ara
