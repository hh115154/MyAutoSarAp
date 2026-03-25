/**
 * @file test_ara_diag.cpp
 * @brief ara::diag 单元测试
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Ref: AUTOSAR_SWS_Diagnostics (R25-11)
 */

#include <gtest/gtest.h>
#include "ara/diag/diag_error_domain.h"

// -------------------------------------------------------
// SessionControlType 枚举测试
// -------------------------------------------------------
TEST(AraDiagSessionControlTypeTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::SessionControlType::kDefaultSession),           0x01u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::SessionControlType::kProgrammingSession),       0x02u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::SessionControlType::kExtendedDiagnosticSession),0x03u);
}

// -------------------------------------------------------
// DtcStatusBit 枚举测试
// -------------------------------------------------------
TEST(AraDiagDtcStatusBitTest, BitMasksArePowersOfTwo) {
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kTestFailed),                        0x01u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kTestFailedThisMonitoringCycle),     0x02u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kPendingDTC),                        0x04u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kConfirmedDTC),                      0x08u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kTestNotCompletedSinceLastClear),    0x10u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kTestFailedSinceLastClear),          0x20u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kTestNotCompletedThisMonitoringCycle),0x40u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DtcStatusBit::kWarningIndicatorRequested),         0x80u);
}

// -------------------------------------------------------
// DiagErrc 枚举测试（UDS NRC 值校验）
// -------------------------------------------------------
TEST(AraDiagErrcTest, UdsNrcValuesMatchIso14229) {
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DiagErrc::kSuccess),                   0x00u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DiagErrc::kConditionsNotCorrect),      0x22u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DiagErrc::kRequestOutOfRange),         0x31u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DiagErrc::kSecurityAccessDenied),      0x33u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::DiagErrc::kGeneralProgrammingFailure), 0x72u);
}

// -------------------------------------------------------
// MonitorAction 枚举测试
// -------------------------------------------------------
TEST(AraDiagMonitorActionTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::MonitorAction::kPassed),    0x00u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::MonitorAction::kFailed),    0x01u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::MonitorAction::kPrepassed), 0x02u);
    EXPECT_EQ(static_cast<uint8_t>(ara::diag::MonitorAction::kPrefailed), 0x03u);
}

// -------------------------------------------------------
// DiagnosticMonitor 测试
// -------------------------------------------------------
TEST(AraDiagMonitorTest, ConstructDoesNotThrow) {
    EXPECT_NO_THROW({
        ara::diag::DiagnosticMonitor monitor("TestEvent_DTC");
    });
}

TEST(AraDiagMonitorTest, ReportPassedIsNoexcept) {
    ara::diag::DiagnosticMonitor monitor("TestEvent_DTC");
    static_assert(
        noexcept(monitor.ReportMonitorAction(ara::diag::MonitorAction::kPassed)),
        "ReportMonitorAction must be noexcept per AUTOSAR SWS_Diag"
    );
    SUCCEED();
}
