/**
 * @file test_ara_com.cpp
 * @brief ara::com 单元测试
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Ref: AUTOSAR_SWS_CommunicationManagement (R25-11)
 */

#include <gtest/gtest.h>
#include "ara/com/types.h"
#include "ara/com/proxy_base.h"
#include "ara/com/skeleton_base.h"

// -------------------------------------------------------
// InstanceIdentifier 测试
// -------------------------------------------------------
TEST(AraComInstanceIdentifierTest, GetValueReturnsConstructedString) {
    ara::com::InstanceIdentifier id("service_instance_1");
    EXPECT_EQ(id.GetValue(), "service_instance_1");
}

TEST(AraComInstanceIdentifierTest, EqualityOperatorWorks) {
    ara::com::InstanceIdentifier id1("instance_A");
    ara::com::InstanceIdentifier id2("instance_A");
    ara::com::InstanceIdentifier id3("instance_B");

    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
}

TEST(AraComInstanceIdentifierTest, EmptyStringIsValid) {
    ara::com::InstanceIdentifier id("");
    EXPECT_TRUE(id.GetValue().empty());
}

// -------------------------------------------------------
// FindServiceResult 枚举测试
// -------------------------------------------------------
TEST(AraComFindServiceResultTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::com::FindServiceResult::kSuccess),        0u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::FindServiceResult::kServiceNotFound),1u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::FindServiceResult::kError),          2u);
}

// -------------------------------------------------------
// SubscriptionState 枚举测试
// -------------------------------------------------------
TEST(AraComSubscriptionStateTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::com::SubscriptionState::kNotSubscribed),     0u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::SubscriptionState::kSubscribed),        1u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::SubscriptionState::kSubscriptionPending),2u);
}

// -------------------------------------------------------
// E2ECheckStatus 枚举测试
// -------------------------------------------------------
TEST(AraComE2ECheckStatusTest, EnumValuesAreCorrect) {
    EXPECT_EQ(static_cast<uint8_t>(ara::com::E2ECheckStatus::kOk),           0u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::E2ECheckStatus::kRepeated),     1u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::E2ECheckStatus::kWrongSequence),2u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::E2ECheckStatus::kError),        3u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::E2ECheckStatus::kNotAvailable), 4u);
    EXPECT_EQ(static_cast<uint8_t>(ara::com::E2ECheckStatus::kNoNewData),    5u);
}

// -------------------------------------------------------
// ProxyBase 测试（通过具体子类）
// -------------------------------------------------------
class ConcreteProxy : public ara::com::ProxyBase {
public:
    explicit ConcreteProxy(const ara::com::InstanceIdentifier& id)
        : ara::com::ProxyBase(id) {}
};

TEST(AraComProxyBaseTest, GetInstanceIdReturnsCorrectValue) {
    ara::com::InstanceIdentifier id("proxy_instance");
    ConcreteProxy proxy(id);
    EXPECT_EQ(proxy.GetInstanceId().GetValue(), "proxy_instance");
}

TEST(AraComProxyBaseTest, MoveConstructorWorks) {
    ara::com::InstanceIdentifier id("move_test");
    ConcreteProxy proxy1(id);
    ConcreteProxy proxy2(std::move(proxy1));
    EXPECT_EQ(proxy2.GetInstanceId().GetValue(), "move_test");
}

TEST(AraComProxyBaseTest, CopyIsDisabled) {
    // 编译期检查：ProxyBase 不可拷贝
    static_assert(!std::is_copy_constructible<ConcreteProxy>::value,
                  "ProxyBase must not be copy-constructible");
    SUCCEED();
}

// -------------------------------------------------------
// SkeletonBase 测试（通过具体子类）
// -------------------------------------------------------
class ConcreteSkeleton : public ara::com::SkeletonBase {
public:
    explicit ConcreteSkeleton(const ara::com::InstanceIdentifier& id)
        : ara::com::SkeletonBase(id) {}
};

TEST(AraComSkeletonBaseTest, InitiallyNotOffered) {
    ara::com::InstanceIdentifier id("skeleton_instance");
    ConcreteSkeleton skeleton(id);
    EXPECT_FALSE(skeleton.IsOffered());
}

TEST(AraComSkeletonBaseTest, OfferServiceSetsOfferedTrue) {
    ara::com::InstanceIdentifier id("skeleton_offer");
    ConcreteSkeleton skeleton(id);
    skeleton.OfferService();
    EXPECT_TRUE(skeleton.IsOffered());
}

TEST(AraComSkeletonBaseTest, StopOfferServiceSetsOfferedFalse) {
    ara::com::InstanceIdentifier id("skeleton_stop");
    ConcreteSkeleton skeleton(id);
    skeleton.OfferService();
    skeleton.StopOfferService();
    EXPECT_FALSE(skeleton.IsOffered());
}

TEST(AraComSkeletonBaseTest, GetInstanceIdReturnsCorrectValue) {
    ara::com::InstanceIdentifier id("skel_id_test");
    ConcreteSkeleton skeleton(id);
    EXPECT_EQ(skeleton.GetInstanceId().GetValue(), "skel_id_test");
}

TEST(AraComSkeletonBaseTest, DestructorStopsOfferAutomatically) {
    // 析构时若 is_offered_ 为 true，会自动调用 StopOfferService —— 不应崩溃
    EXPECT_NO_THROW({
        ara::com::InstanceIdentifier id("dtor_test");
        ConcreteSkeleton skeleton(id);
        skeleton.OfferService();
        // skeleton 析构时自动调用 StopOfferService
    });
}
