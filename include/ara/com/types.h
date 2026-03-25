/**
 * @file types.h
 * @brief ara::com 基础类型定义
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Communication Management
 * Ref: AUTOSAR_SWS_CommunicationManagement (R25-11)
 */

#ifndef ARA_COM_TYPES_H
#define ARA_COM_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace ara {
namespace com {

/// 实例标识符
class InstanceIdentifier {
public:
    explicit InstanceIdentifier(const std::string& value) : value_(value) {}
    const std::string& GetValue() const { return value_; }
    bool operator==(const InstanceIdentifier& other) const {
        return value_ == other.value_;
    }
    bool operator!=(const InstanceIdentifier& other) const {
        return value_ != other.value_;
    }

private:
    std::string value_;
};

/// 服务发现结果
enum class FindServiceResult : uint8_t {
    kSuccess = 0,
    kServiceNotFound = 1,
    kError = 2
};

/// 订阅状态
enum class SubscriptionState : uint8_t {
    kNotSubscribed = 0,
    kSubscribed = 1,
    kSubscriptionPending = 2
};

/// E2E 检查结果
enum class E2ECheckStatus : uint8_t {
    kOk = 0,
    kRepeated = 1,
    kWrongSequence = 2,
    kError = 3,
    kNotAvailable = 4,
    kNoNewData = 5
};

/// 服务句柄类型
using ServiceHandleId = uint32_t;

/// 回调函数类型
template <typename T>
using SampleAllocateePtr = std::shared_ptr<T>;

} // namespace com
} // namespace ara

#endif // ARA_COM_TYPES_H
