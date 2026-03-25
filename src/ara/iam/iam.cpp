/**
 * @file iam.cpp
 * @brief ara::iam — Identity and Access Management 实现
 */

#include "ara/iam/identity_access_management.h"
#include <chrono>
#include <mutex>
#include <iostream>

namespace ara {
namespace iam {

// ============================================================
// IdentityToken::IsValid 实现
// ============================================================

bool IdentityToken::IsValid() const
{
    if (subjectId.empty()) return false;
    auto now = std::chrono::system_clock::now();
    int64_t nowUnix = std::chrono::duration_cast<std::chrono::seconds>(
                          now.time_since_epoch()).count();
    return (nowUnix >= notBefore && nowUnix <= notAfter);
}

// ============================================================
// IdentityAccessManagement 实现
// ============================================================

IdentityAccessManagement& IdentityAccessManagement::GetInstance()
{
    static IdentityAccessManagement instance;
    return instance;
}

bool IdentityAccessManagement::Initialize()
{
    // 加载默认策略（与 SOC-SW-001 §3.12 对应）
    // VehicleSignalService 允许 DriveAssist 角色读取
    RegisterPolicy({"VehicleSignalService", "DriveAssist", AccessLevel::kRead, false});
    RegisterPolicy({"VehicleSignalService", "SafetyMonitor", AccessLevel::kRead, false});
    // SafetyStatusService 仅 SafetyMonitor 可读
    RegisterPolicy({"SafetyStatusService", "SafetyMonitor", AccessLevel::kRead, false});
    // DiagnosticProxyService 允许 Diagnostic 角色读写
    RegisterPolicy({"DiagnosticProxyService", "Diagnostic", AccessLevel::kWrite, true});
    // OTATransferService 仅 OTAClient 角色
    RegisterPolicy({"OTATransferService", "OTAClient", AccessLevel::kWrite, true});
    // HMICommandService SOC→MCU 需要 HMIController 角色
    RegisterPolicy({"HMICommandService", "HMIController", AccessLevel::kWrite, false});
    return true;
}

void IdentityAccessManagement::RegisterPolicy(const AccessPolicy& policy)
{
    std::lock_guard<std::mutex> lock(mutex_);
    policies_.push_back(policy);
}

IamResult IdentityAccessManagement::VerifyIdentity(
    const IdentityToken& token,
    const std::vector<std::vector<uint8_t>>& /*certChain*/)
{
    if (token.subjectId.empty()) return IamResult::kInvalidToken;
    if (!token.IsValid())        return IamResult::kInvalidToken;

    // 模拟证书链验证（生产替换为 X.509 解析 + ECDSA 验签）
    // 此处直接接受有效期内的 Token
    return IamResult::kGranted;
}

IamResult IdentityAccessManagement::CheckAccess(
    const IdentityToken& token,
    const std::string& resourceId,
    AccessLevel level)
{
    if (!token.IsValid()) return IamResult::kInvalidToken;

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否被吊销
    auto it = subjects_.find(token.subjectId);
    if (it != subjects_.end() && it->second.revoked) {
        if (auditCallback_) {
            auditCallback_(token.subjectId, resourceId, IamResult::kDenied);
        }
        return IamResult::kDenied;
    }

    // 遍历策略：主体的角色 × 资源 × 级别
    for (const auto& role : token.roles) {
        for (const auto& policy : policies_) {
            if (policy.resourceId == resourceId &&
                policy.role == role &&
                static_cast<uint8_t>(policy.level) >= static_cast<uint8_t>(level))
            {
                if (auditCallback_) {
                    auditCallback_(token.subjectId, resourceId, IamResult::kGranted);
                }
                return IamResult::kGranted;
            }
        }
    }

    if (auditCallback_) {
        auditCallback_(token.subjectId, resourceId, IamResult::kDenied);
    }
    return IamResult::kDenied;
}

IdentityToken IdentityAccessManagement::IssueRuntimeToken(
    const std::string& appId,
    const std::vector<std::string>& roles)
{
    IdentityToken token;
    token.subjectId = appId;
    token.issuer    = "OEM-CA";

    auto now = std::chrono::system_clock::now();
    token.notBefore = std::chrono::duration_cast<std::chrono::seconds>(
                          now.time_since_epoch()).count();
    token.notAfter  = token.notBefore + 3600; // 1 小时有效期
    token.roles     = roles;

    // 注册到主体记录
    {
        std::lock_guard<std::mutex> lock(mutex_);
        SubjectRecord rec;
        rec.roles = roles;
        rec.tokenExpiry = token.notAfter;
        rec.revoked = false;
        subjects_[appId] = rec;
    }

    return token;
}

void IdentityAccessManagement::RevokeToken(const std::string& subjectId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subjects_.find(subjectId);
    if (it != subjects_.end()) {
        it->second.revoked = true;
    }
}

void IdentityAccessManagement::SetAuditCallback(AuditCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auditCallback_ = std::move(cb);
}

std::vector<std::string> IdentityAccessManagement::GetRoles(const std::string& subjectId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subjects_.find(subjectId);
    if (it != subjects_.end()) {
        return it->second.roles;
    }
    return {};
}

} // namespace iam
} // namespace ara
