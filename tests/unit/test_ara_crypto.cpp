/**
 * @file test_ara_crypto.cpp
 * @brief ara::crypto 单元测试
 */

#include <gtest/gtest.h>
#include "ara/crypto/crypto_provider.h"

using namespace ara::crypto;

// ============================================================
// CryptoProvider 单例
// ============================================================

TEST(AraCrypto, SingletonIdentity) {
    auto& a = CryptoProvider::GetInstance();
    auto& b = CryptoProvider::GetInstance();
    EXPECT_EQ(&a, &b);
}

// ============================================================
// 随机数生成
// ============================================================

TEST(AraCrypto, GenerateRandom16Bytes) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector output;
    auto result = cp.GenerateRandom(16, output);
    EXPECT_EQ(result, CryptoResult::kSuccess);
    EXPECT_EQ(output.size(), 16u);
}

TEST(AraCrypto, GenerateRandom32Bytes) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector output;
    auto result = cp.GenerateRandom(32, output);
    EXPECT_EQ(result, CryptoResult::kSuccess);
    EXPECT_EQ(output.size(), 32u);
}

TEST(AraCrypto, RandomOutputNotAllZero) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector output;
    cp.GenerateRandom(32, output);
    bool allZero = true;
    for (uint8_t b : output) {
        if (b != 0) { allZero = false; break; }
    }
    EXPECT_FALSE(allZero);
}

TEST(AraCrypto, TwoRandomOutputsDiffer) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector r1, r2;
    cp.GenerateRandom(32, r1);
    cp.GenerateRandom(32, r2);
    EXPECT_NE(r1, r2);
}

// ============================================================
// 密钥生成
// ============================================================

TEST(AraCrypto, GenerateAES256GCMKey) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kAES256_GCM, KeyUsage::kEncrypt | KeyUsage::kDecrypt);
    EXPECT_TRUE(key.IsValid());
    EXPECT_EQ(key.GetAlgorithmId(), AlgorithmId::kAES256_GCM);
}

TEST(AraCrypto, GenerateAES128GCMKey) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kAES128_GCM, KeyUsage::kEncrypt);
    EXPECT_TRUE(key.IsValid());
}

TEST(AraCrypto, GenerateHMACKey) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kHMAC_SHA256, KeyUsage::kSign | KeyUsage::kVerify);
    EXPECT_TRUE(key.IsValid());
}

TEST(AraCrypto, LoadNonExistentKeyReturnsInvalid) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.LoadKey(0xDEADBEEF, AlgorithmId::kAES256_GCM, KeyUsage::kEncrypt);
    EXPECT_FALSE(key.IsValid());
}

// ============================================================
// AES-GCM 加密/解密
// ============================================================

TEST(AraCrypto, SymmetricEncryptDecryptRoundTrip) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kAES256_GCM,
                              KeyUsage::kEncrypt | KeyUsage::kDecrypt);
    auto cipher = cp.CreateSymmetricCipher(AlgorithmId::kAES256_GCM);

    ByteVector iv(12, 0xAA);
    ByteVector plaintext = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    ByteVector aad = {0x01, 0x02};
    ByteVector ciphertext, decrypted;

    auto encResult = cipher->Encrypt(key, iv, plaintext, aad, ciphertext);
    EXPECT_EQ(encResult, CryptoResult::kSuccess);
    EXPECT_GT(ciphertext.size(), plaintext.size()); // +16 bytes auth tag

    auto decResult = cipher->Decrypt(key, iv, ciphertext, aad, decrypted);
    EXPECT_EQ(decResult, CryptoResult::kSuccess);
    EXPECT_EQ(decrypted, plaintext);
}

TEST(AraCrypto, DecryptTamperedDataFails) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kAES256_GCM,
                              KeyUsage::kEncrypt | KeyUsage::kDecrypt);
    auto cipher = cp.CreateSymmetricCipher(AlgorithmId::kAES256_GCM);

    ByteVector iv(12, 0xBB);
    ByteVector plaintext = {0x54, 0x65, 0x73, 0x74}; // "Test"
    ByteVector aad;
    ByteVector ciphertext, decrypted;
    cipher->Encrypt(key, iv, plaintext, aad, ciphertext);

    // 篡改 Auth Tag（最后 16 字节）
    ciphertext.back() ^= 0xFF;
    auto result = cipher->Decrypt(key, iv, ciphertext, aad, decrypted);
    EXPECT_EQ(result, CryptoResult::kAuthFailed);
}

TEST(AraCrypto, EncryptWithInvalidKeyFails) {
    auto cipher = CryptoProvider::GetInstance()
        .CreateSymmetricCipher(AlgorithmId::kAES256_GCM);
    CryptoKey invalidKey;
    ByteVector iv(12, 0), plain = {1, 2, 3}, aad, ct;
    auto result = cipher->Encrypt(invalidKey, iv, plain, aad, ct);
    EXPECT_EQ(result, CryptoResult::kInvalidKey);
}

// ============================================================
// HMAC-SHA256
// ============================================================

TEST(AraCrypto, HmacGenerateAndVerify) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kHMAC_SHA256,
                              KeyUsage::kSign | KeyUsage::kVerify);
    auto mac = cp.CreateMessageAuthCode(AlgorithmId::kHMAC_SHA256);

    ByteVector data = {0x01, 0x02, 0x03, 0x04, 0x05};
    ByteVector macValue;

    auto genResult = mac->Generate(key, data, macValue);
    EXPECT_EQ(genResult, CryptoResult::kSuccess);
    EXPECT_EQ(macValue.size(), 32u);

    auto verResult = mac->Verify(key, data, macValue);
    EXPECT_EQ(verResult, CryptoResult::kSuccess);
}

TEST(AraCrypto, HmacVerifyTamperedDataFails) {
    auto& cp = CryptoProvider::GetInstance();
    auto key = cp.GenerateKey(AlgorithmId::kHMAC_SHA256,
                              KeyUsage::kSign | KeyUsage::kVerify);
    auto mac = cp.CreateMessageAuthCode(AlgorithmId::kHMAC_SHA256);

    ByteVector data = {0x0A, 0x0B, 0x0C};
    ByteVector macValue;
    mac->Generate(key, data, macValue);

    // 篡改数据后验证失败
    data[0] ^= 0x01;
    auto result = mac->Verify(key, data, macValue);
    EXPECT_EQ(result, CryptoResult::kAuthFailed);
}

// ============================================================
// 哈希计算
// ============================================================

TEST(AraCrypto, HashSHA256OutputSize) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector data = {0x01, 0x02, 0x03};
    ByteVector digest;
    auto result = cp.Hash(AlgorithmId::kSHA256, data, digest);
    EXPECT_EQ(result, CryptoResult::kSuccess);
    EXPECT_EQ(digest.size(), 32u);
}

TEST(AraCrypto, HashDeterministic) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector data = {0xAB, 0xCD, 0xEF};
    ByteVector d1, d2;
    cp.Hash(AlgorithmId::kSHA256, data, d1);
    cp.Hash(AlgorithmId::kSHA256, data, d2);
    EXPECT_EQ(d1, d2);
}

TEST(AraCrypto, HashDifferentInputsDifferentOutputs) {
    auto& cp = CryptoProvider::GetInstance();
    ByteVector d1 = {0x01}, d2 = {0x02};
    ByteVector h1, h2;
    cp.Hash(AlgorithmId::kSHA256, d1, h1);
    cp.Hash(AlgorithmId::kSHA256, d2, h2);
    EXPECT_NE(h1, h2);
}

// ============================================================
// 签名验证
// ============================================================

TEST(AraCrypto, VerifySignatureWithValidKey) {
    auto& cp = CryptoProvider::GetInstance();
    auto pubKey = cp.GenerateKey(AlgorithmId::kECDSA_P256, KeyUsage::kVerify);
    ByteVector data = {0x01, 0x02, 0x03};
    ByteVector sig  = {0x30, 0x44}; // DER 编码模拟
    auto result = cp.VerifySignature(pubKey, data, sig);
    EXPECT_EQ(result, CryptoResult::kSuccess);
}

TEST(AraCrypto, VerifySignatureWithInvalidKey) {
    auto& cp = CryptoProvider::GetInstance();
    CryptoKey invalid;
    ByteVector data = {0x01}, sig = {0x02};
    auto result = cp.VerifySignature(invalid, data, sig);
    EXPECT_EQ(result, CryptoResult::kInvalidKey);
}

// ============================================================
// AlgorithmId 枚举值
// ============================================================

TEST(AraCrypto, AlgorithmIdValues) {
    EXPECT_EQ(static_cast<uint32_t>(AlgorithmId::kAES128_CBC),   0x0001u);
    EXPECT_EQ(static_cast<uint32_t>(AlgorithmId::kAES256_GCM),   0x0004u);
    EXPECT_EQ(static_cast<uint32_t>(AlgorithmId::kHMAC_SHA256),  0x0401u);
    EXPECT_EQ(static_cast<uint32_t>(AlgorithmId::kECDSA_P256),   0x0201u);
}
