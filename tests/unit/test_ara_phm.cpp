/**
 * @file test_ara_phm.cpp
 * @brief ara::phm 单元测试
 */

#include <gtest/gtest.h>
#include "ara/phm/platform_health_management.h"

using namespace ara::phm;

// ============================================================
// PHM 单例基础测试
// ============================================================

TEST(AraPhm, GetInstanceSingleton) {
    auto& a = PlatformHealthManagement::GetInstance();
    auto& b = PlatformHealthManagement::GetInstance();
    EXPECT_EQ(&a, &b);
}

TEST(AraPhm, StartStop) {
    auto& phm = PlatformHealthManagement::GetInstance();
    phm.Start();
    EXPECT_TRUE(phm.IsRunning());
    phm.Stop();
    EXPECT_FALSE(phm.IsRunning());
}

// ============================================================
// SE 注册/注销
// ============================================================

TEST(AraPhm, RegisterEntitySuccess) {
    auto& phm = PlatformHealthManagement::GetInstance();
    AliveSupervisionConfig conf{100, 0, 2, 3};
    bool result = phm.RegisterEntity("TestEntity_1", conf);
    EXPECT_TRUE(result);
    phm.UnregisterEntity("TestEntity_1");
}

TEST(AraPhm, RegisterEntityDuplicate) {
    auto& phm = PlatformHealthManagement::GetInstance();
    AliveSupervisionConfig conf{100, 0, 2, 3};
    phm.RegisterEntity("TestEntity_Dup", conf);
    bool second = phm.RegisterEntity("TestEntity_Dup", conf);
    EXPECT_FALSE(second); // 重复注册
    phm.UnregisterEntity("TestEntity_Dup");
}

TEST(AraPhm, UnregisterNonExistent) {
    auto& phm = PlatformHealthManagement::GetInstance();
    // 注销不存在的实体不应崩溃
    EXPECT_NO_THROW(phm.UnregisterEntity("NonExistent_XYZ"));
}

// ============================================================
// 检查点上报
// ============================================================

TEST(AraPhm, ReportCheckpointNormal) {
    auto& phm = PlatformHealthManagement::GetInstance();
    AliveSupervisionConfig conf{100, 0, 2, 3};
    phm.RegisterEntity("TestEntity_Cp", conf);

    EXPECT_NO_THROW(phm.ReportCheckpoint("TestEntity_Cp", 0x01));
    EXPECT_NO_THROW(phm.ReportCheckpoint("TestEntity_Cp", 0x01));

    phm.UnregisterEntity("TestEntity_Cp");
}

TEST(AraPhm, ReportCheckpointUnknownEntity) {
    auto& phm = PlatformHealthManagement::GetInstance();
    // 未知实体上报不应崩溃
    EXPECT_NO_THROW(phm.ReportCheckpoint("Unknown_XYZ", 0x01));
}

// ============================================================
// 全局状态
// ============================================================

TEST(AraPhm, GlobalStatusNormalWhenNoEntities) {
    auto& phm = PlatformHealthManagement::GetInstance();
    // 清空后（或无注册实体）应为 Normal
    GlobalSupervisionStatus status = phm.GetGlobalStatus();
    EXPECT_EQ(status, GlobalSupervisionStatus::kNormal);
}

TEST(AraPhm, GlobalStatusNormalAfterCheckpoint) {
    auto& phm = PlatformHealthManagement::GetInstance();
    AliveSupervisionConfig conf{100, 0, 2, 3};
    phm.RegisterEntity("TestEntity_Status", conf);
    phm.ReportCheckpoint("TestEntity_Status", 0x01);

    GlobalSupervisionStatus status = phm.GetGlobalStatus();
    EXPECT_EQ(status, GlobalSupervisionStatus::kNormal);

    phm.UnregisterEntity("TestEntity_Status");
}

// ============================================================
// WDG 踢狗
// ============================================================

TEST(AraPhm, KickWatchdog) {
    auto& phm = PlatformHealthManagement::GetInstance();
    EXPECT_NO_THROW(phm.KickWatchdog());
    EXPECT_NO_THROW(phm.KickWatchdog());
}

// ============================================================
// 故障恢复动作配置
// ============================================================

TEST(AraPhm, SetPhmAction) {
    auto& phm = PlatformHealthManagement::GetInstance();
    AliveSupervisionConfig conf{100, 0, 2, 3};
    phm.RegisterEntity("TestEntity_Action", conf);
    EXPECT_NO_THROW(phm.SetPhmAction("TestEntity_Action", PhmAction::kTriggerSm));
    phm.UnregisterEntity("TestEntity_Action");
}

TEST(AraPhm, SetRecoveryCallback) {
    auto& phm = PlatformHealthManagement::GetInstance();
    bool callbackInvoked = false;
    phm.SetRecoveryCallback([&](const std::string& /*id*/, SupervisionStatus /*s*/) {
        callbackInvoked = true;
    });
    // 回调设置不应崩溃
    EXPECT_NO_THROW(phm.KickWatchdog());
    (void)callbackInvoked;
}

// ============================================================
// SupervisedEntity 类测试
// ============================================================

TEST(AraPhm, SupervisedEntityConstruct) {
    // SupervisedEntity 构造时自动注册，析构时自动注销
    {
        SupervisedEntity se("TestSE_Auto");
        EXPECT_EQ(se.GetInstanceId(), "TestSE_Auto");
        EXPECT_EQ(se.GetStatus(), SupervisionStatus::kOk);
        se.CheckpointReached(0x01);
        EXPECT_EQ(se.GetStatus(), SupervisionStatus::kOk);
    } // 析构时自动注销
}

TEST(AraPhm, SupervisedEntityMultipleCheckpoints) {
    SupervisedEntity se("TestSE_Multi");
    for (int i = 0; i < 10; ++i) {
        se.CheckpointReached(0x01);
    }
    EXPECT_EQ(se.GetStatus(), SupervisionStatus::kOk);
}

// ============================================================
// 枚举值校验
// ============================================================

TEST(AraPhm, SupervisionStatusValues) {
    EXPECT_EQ(static_cast<uint8_t>(SupervisionStatus::kOk),          0u);
    EXPECT_EQ(static_cast<uint8_t>(SupervisionStatus::kFailed),      1u);
    EXPECT_EQ(static_cast<uint8_t>(SupervisionStatus::kExpired),     2u);
    EXPECT_EQ(static_cast<uint8_t>(SupervisionStatus::kStopped),     3u);
    EXPECT_EQ(static_cast<uint8_t>(SupervisionStatus::kDeactivated), 4u);
}

TEST(AraPhm, GlobalSupervisionStatusValues) {
    EXPECT_EQ(static_cast<uint8_t>(GlobalSupervisionStatus::kNormal),  0u);
    EXPECT_EQ(static_cast<uint8_t>(GlobalSupervisionStatus::kFailed),  1u);
    EXPECT_EQ(static_cast<uint8_t>(GlobalSupervisionStatus::kExpired), 2u);
    EXPECT_EQ(static_cast<uint8_t>(GlobalSupervisionStatus::kStopped), 3u);
}

TEST(AraPhm, PhmActionValues) {
    EXPECT_EQ(static_cast<uint8_t>(PhmAction::kNone),        0u);
    EXPECT_EQ(static_cast<uint8_t>(PhmAction::kReportError), 1u);
    EXPECT_EQ(static_cast<uint8_t>(PhmAction::kRestartApp),  2u);
    EXPECT_EQ(static_cast<uint8_t>(PhmAction::kTriggerSm),   3u);
    EXPECT_EQ(static_cast<uint8_t>(PhmAction::kHardReset),   4u);
}
