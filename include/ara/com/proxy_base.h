/**
 * @file proxy_base.h
 * @brief ara::com Proxy 基类
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Service Consumer 端基类，所有 Proxy 类从此继承
 * Ref: AUTOSAR_SWS_CommunicationManagement (R25-11)
 */

#ifndef ARA_COM_PROXY_BASE_H
#define ARA_COM_PROXY_BASE_H

#include "ara/com/types.h"
#include <memory>
#include <vector>
#include <functional>

namespace ara {
namespace com {

/**
 * @brief Proxy 基类
 * 
 * 所有由 arxml 生成的 Proxy 类应继承此基类
 */
class ProxyBase {
public:
    explicit ProxyBase(const InstanceIdentifier& instance)
        : instance_(instance) {}

    virtual ~ProxyBase() = default;

    /// 禁止拷贝
    ProxyBase(const ProxyBase&) = delete;
    ProxyBase& operator=(const ProxyBase&) = delete;

    /// 允许移动
    ProxyBase(ProxyBase&&) = default;
    ProxyBase& operator=(ProxyBase&&) = default;

    /**
     * @brief 获取实例标识符
     * @return InstanceIdentifier
     */
    const InstanceIdentifier& GetInstanceId() const { return instance_; }

protected:
    InstanceIdentifier instance_;
};

/**
 * @brief 服务发现处理器
 */
template <typename ProxyType>
class ServiceFinder {
public:
    using HandleType = typename ProxyType::HandleType;
    using FindServiceCallback = std::function<void(std::vector<HandleType>)>;

    /**
     * @brief 查找服务（同步）
     * @param instance 实例标识符
     * @return 可用的服务句柄列表
     */
    static std::vector<HandleType> FindService(
        const InstanceIdentifier& instance);

    /**
     * @brief 查找服务（异步，持续监听）
     * @param handler 服务发现回调
     * @param instance 实例标识符
     * @return 注册句柄（用于停止监听）
     */
    static ServiceHandleId StartFindService(
        FindServiceCallback handler,
        const InstanceIdentifier& instance);

    /**
     * @brief 停止服务发现
     * @param handle StartFindService 返回的句柄
     */
    static void StopFindService(ServiceHandleId handle);
};

} // namespace com
} // namespace ara

#endif // ARA_COM_PROXY_BASE_H
