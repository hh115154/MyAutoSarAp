/**
 * @file skeleton_base.h
 * @brief ara::com Skeleton 基类
 *
 * 符合 AUTOSAR AP R21-11 规范
 * Service Provider 端基类，所有 Skeleton 类从此继承
 */

#ifndef ARA_COM_SKELETON_BASE_H
#define ARA_COM_SKELETON_BASE_H

#include "ara/com/types.h"
#include <memory>

namespace ara {
namespace com {

/**
 * @brief Skeleton 基类
 *
 * 所有由 arxml 生成的 Skeleton 类应继承此基类
 */
class SkeletonBase {
public:
    explicit SkeletonBase(const InstanceIdentifier& instance)
        : instance_(instance), is_offered_(false) {}

    virtual ~SkeletonBase() {
        if (is_offered_) {
            StopOfferService();
        }
    }

    /// 禁止拷贝
    SkeletonBase(const SkeletonBase&) = delete;
    SkeletonBase& operator=(const SkeletonBase&) = delete;

    /**
     * @brief 发布服务（使服务对 Consumer 可见）
     */
    virtual void OfferService() {
        is_offered_ = true;
    }

    /**
     * @brief 停止发布服务
     */
    virtual void StopOfferService() {
        is_offered_ = false;
    }

    /**
     * @brief 获取实例标识符
     */
    const InstanceIdentifier& GetInstanceId() const { return instance_; }

    /**
     * @brief 服务是否已发布
     */
    bool IsOffered() const { return is_offered_; }

protected:
    InstanceIdentifier instance_;
    bool is_offered_;
};

} // namespace com
} // namespace ara

#endif // ARA_COM_SKELETON_BASE_H
