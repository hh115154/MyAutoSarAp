/**
 * @file identity_access_management.h
 * @brief ara::iam — Identity and Access Management
 *
 * 符合 AUTOSAR AP R25-11 规范（R24 引入）
 * Functional Cluster: Identity and Access Management
 * Ref: AUTOSAR_SWS_IdentityAndAccessManagement (R25-11)
 *
 * 设计依据：SOC-SW-001 §3.12 身份与访问管理
 * 提供：服务访问策略、证书绑定身份验证、角色权限控制
 */

#ifndef ARA_IAM_IDENTITY_ACCESS_MANAGEMENT_H
#define ARA_IAM_IDENTITY_ACCESS_MANAGEMENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ara {
namespace iam {

// ============================================================
// 枚举和基础类型
// ============================================================

/**
 * @brief IAM 操作结果
 */
enum class IamResult : uint8_t {
    kGranted       = 0,  ///< 访问授权
    kDenied        = 1,  ///< 访问拒绝
    kInvalidToken  = 2,  ///< Token 无效或已过期
    kUnknownEntity = 3,  ///< 未知实体
    kError         = 4
};

/**
 * @brief 访问权限级别
 */
enum class AccessLevel : uint8_t {
    kNone     = 0,  ///< 无权限
    kRead     = 1,  ///< 只读
    kWrite    = 2,  ///< 读写
    kAdmin    = 3,  ///< 管理员（完全控制）
    kSecurity = 4   ///< 安全关键（ASIL-B 隔离）
};

/**
 * @brief 身份令牌（Identity Token）
 * 基于 X.509 证书绑定的访问凭证
 */
struct IdentityToken {
    std::string subjectId;     ///< 主体 ID（证书 CN）
    std::string issuer;        ///< 签发机构（OEM CA）
    int64_t     notBefore;     ///< 有效期开始（Unix timestamp）
    int64_t     notAfter;      ///< 有效期结束
    std::vector<std::string> roles; ///< 授予角色列表
    std::vector<uint8_t> signature; ///< 数字签名（ECDSA P-256）

    bool IsValid() const;
};

/**
 * @brief 访问策略条目
 */
struct AccessPolicy {
    std::string resourceId;    ///< 资源 ID（服务名/端点）
    std::string role;          ///< 允许访问的角色
    AccessLevel level;         ///< 访问级别
    bool        allowRemote;   ///< 是否允许远程访问
};

// ============================================================
// IdentityAccessManagement — IAM 主控类
// ============================================================

/**
 * @brief IAM 主控类（单例）
 *
 * 职责：
 * 1. 管理 AA 和服务的身份注册
 * 2. 基于策略的访问控制（ABAC/RBAC）
 * 3. 证书绑定与 Token 验证
 * 4. 与 ara::crypto 集成（签名验证）
 * 5. 与 ara::idsm 联动（异常访问上报）
 *
 * 对应 SOC-SW-001 §3.12：
 *   - 服务访问基于角色策略控制
 *   - OEM CA 签发的 X.509 证书绑定
 *   - 非法访问尝试上报 IDSM
 */
class IdentityAccessManagement {
public:
    /// 访问审计回调（用于记录/上报异常访问）
    using AuditCallback = std::function<void(
        const std::string& subjectId,
        const std::string& resourceId,
        IamResult result)>;

    /**
     * @brief 获取 IAM 单例
     */
    static IdentityAccessManagement& GetInstance();

    IdentityAccessManagement(const IdentityAccessManagement&) = delete;
    IdentityAccessManagement& operator=(const IdentityAccessManagement&) = delete;

    /**
     * @brief 初始化 IAM（加载策略配置）
     * @return 是否成功
     */
    bool Initialize();

    /**
     * @brief 注册访问策略
     * @param policy  访问策略条目
     */
    void RegisterPolicy(const AccessPolicy& policy);

    /**
     * @brief 验证身份令牌（SWS_IAM_00030）
     * @param token       身份令牌
     * @param certChain   证书链（DER 格式）
     * @return            验证结果
     */
    IamResult VerifyIdentity(
        const IdentityToken& token,
        const std::vector<std::vector<uint8_t>>& certChain);

    /**
     * @brief 检查访问权限（SWS_IAM_00040）
     * @param token       已验证的身份令牌
     * @param resourceId  目标资源 ID（服务/端点）
     * @param level       请求的访问级别
     * @return            授权结果
     */
    IamResult CheckAccess(
        const IdentityToken& token,
        const std::string& resourceId,
        AccessLevel level);

    /**
     * @brief 为 AA 进程颁发运行时令牌（由 Execution Management 调用）
     * @param appId       应用 ID（Process Name）
     * @param roles       授予角色
     * @return            运行时令牌（有效期内）
     */
    IdentityToken IssueRuntimeToken(
        const std::string& appId,
        const std::vector<std::string>& roles);

    /**
     * @brief 吊销令牌
     */
    void RevokeToken(const std::string& subjectId);

    /**
     * @brief 注册审计回调
     */
    void SetAuditCallback(AuditCallback cb);

    /**
     * @brief 查询主体拥有的角色
     */
    std::vector<std::string> GetRoles(const std::string& subjectId) const;

private:
    IdentityAccessManagement() = default;

    struct SubjectRecord {
        std::vector<std::string> roles;
        int64_t tokenExpiry{0};
        bool revoked{false};
    };

    std::unordered_map<std::string, SubjectRecord> subjects_;
    std::vector<AccessPolicy> policies_;
    AuditCallback auditCallback_;
    mutable std::mutex mutex_;
};

} // namespace iam
} // namespace ara

#endif // ARA_IAM_IDENTITY_ACCESS_MANAGEMENT_H
