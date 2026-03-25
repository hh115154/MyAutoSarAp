/**
 * @file test_ara_log.cpp
 * @brief ara::log 单元测试
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Ref: AUTOSAR_SWS_LogAndTrace (R25-11)
 */

#include <gtest/gtest.h>
#include "ara/log/logger.h"

// -------------------------------------------------------
// LogLevel 枚举测试
// -------------------------------------------------------
TEST(AraLogLevelTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kOff),     0x00);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kFatal),   0x01);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kError),   0x02);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kWarn),    0x03);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kInfo),    0x04);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kDebug),   0x05);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogLevel::kVerbose), 0x06);
}

// -------------------------------------------------------
// LogMode 枚举测试
// -------------------------------------------------------
TEST(AraLogModeTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogMode::kRemote),  0x01);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogMode::kFile),    0x02);
    EXPECT_EQ(static_cast<uint8_t>(ara::log::LogMode::kConsole), 0x04);
}

// -------------------------------------------------------
// InitLogging 测试
// -------------------------------------------------------
TEST(AraLogInitTest, InitLoggingDoesNotThrow) {
    EXPECT_NO_THROW(
        ara::log::InitLogging(
            "TEST",
            "Test Application",
            ara::log::LogLevel::kDebug,
            ara::log::LogMode::kConsole
        )
    );
}

// -------------------------------------------------------
// CreateLogger 测试
// -------------------------------------------------------
TEST(AraLogCreateLoggerTest, ReturnsSameInstanceForSameCtxId) {
    ara::log::InitLogging("TEST", "Test", ara::log::LogLevel::kVerbose, ara::log::LogMode::kConsole);

    auto& logger1 = ara::log::CreateLogger("CTX1", "Context 1");
    auto& logger2 = ara::log::CreateLogger("CTX1", "Context 1");
    // 相同 ctx_id 应返回同一实例
    EXPECT_EQ(&logger1, &logger2);
}

TEST(AraLogCreateLoggerTest, DifferentCtxIdReturnsDifferentInstance) {
    ara::log::InitLogging("TEST", "Test", ara::log::LogLevel::kVerbose, ara::log::LogMode::kConsole);

    auto& loggerA = ara::log::CreateLogger("CTXA", "Context A");
    auto& loggerB = ara::log::CreateLogger("CTXB", "Context B");
    EXPECT_NE(&loggerA, &loggerB);
}

// -------------------------------------------------------
// Logger::IsEnabled 测试
// -------------------------------------------------------
TEST(AraLoggerIsEnabledTest, InfoEnabledWhenLevelIsInfo) {
    ara::log::Logger logger("ENAB", "enable test", ara::log::LogLevel::kInfo);
    EXPECT_TRUE(logger.IsEnabled(ara::log::LogLevel::kInfo));
    EXPECT_TRUE(logger.IsEnabled(ara::log::LogLevel::kError));
    EXPECT_TRUE(logger.IsEnabled(ara::log::LogLevel::kFatal));
}

TEST(AraLoggerIsEnabledTest, DebugDisabledWhenLevelIsInfo) {
    ara::log::Logger logger("DISB", "disable test", ara::log::LogLevel::kInfo);
    EXPECT_FALSE(logger.IsEnabled(ara::log::LogLevel::kDebug));
    EXPECT_FALSE(logger.IsEnabled(ara::log::LogLevel::kVerbose));
}

TEST(AraLoggerIsEnabledTest, AllEnabledWhenLevelIsVerbose) {
    ara::log::Logger logger("VERB", "verbose test", ara::log::LogLevel::kVerbose);
    EXPECT_TRUE(logger.IsEnabled(ara::log::LogLevel::kFatal));
    EXPECT_TRUE(logger.IsEnabled(ara::log::LogLevel::kVerbose));
}

// -------------------------------------------------------
// LogStream 流式输出测试（不崩溃即通过）
// -------------------------------------------------------
TEST(AraLogStreamTest, StreamOperatorDoesNotCrash) {
    ara::log::Logger logger("STRM", "stream test", ara::log::LogLevel::kVerbose);
    EXPECT_NO_THROW({
        logger.LogInfo()    << "Info message: "  << 42;
        logger.LogDebug()   << "Debug value: "   << 3.14;
        logger.LogWarn()    << "Warning string: " << "test";
        logger.LogError()   << "Error occurred";
        logger.LogVerbose() << "Verbose detail";
    });
}
