# ara::iam — Identity and Access Management 详细设计文档

**文档编号**：DD-IAM-001  
**版本**：V1.0  
**日期**：2026-03-25  
**标准基线**：AUTOSAR AP R25-11 SWS_IdentityAndAccessManagement（R24 引入）  
**关联架构文档**：SOC-SW-001 §3.12  
**功能安全等级**：ASIL-B（防止越权访问安全关键服务）

---

## 1. 功能概述

`ara::iam` 为 AUTOSAR AP R25-11（R24+）引入的**身份与访问管理**功能簇，实现：
- 基于 X.509 证书的身份绑定
- RBAC（基于角色的访问控制）
- 服务访问策略管理
- 与 `ara::idsm`（入侵检测）联动的审计日志

---

## 2. 架构设计

### 2.1 IAM 工作流

```
AA 进程启动
      │
      ▼ (ara::exec 调用)
IssueRuntimeToken(appId, roles)
      │
      ▼ 返回 IdentityToken（有效期 1h，OEM-CA 签发）
      │
      ▼ AA 访问服务前
CheckAccess(token, "VehicleSignalService", kRead)
      │
      ├── 遍历策略表：主体角色 × 资源 × 级别
      │
      ├── 命中策略 → kGranted + 审计日志
      │
      └── 未命中 → kDenied + 审计日志 → 通知 idsm
```

### 2.2 访问策略表（SOC-SW-001 §3.12 默认配置）

| 资源（Service） | 角色 | 访问级别 | 允许远程 |
|---------------|------|---------|---------|
| VehicleSignalService | DriveAssist | kRead | false |
| VehicleSignalService | SafetyMonitor | kRead | false |
| SafetyStatusService | SafetyMonitor | kRead | false |
| DiagnosticProxyService | Diagnostic | kWrite | true |
| OTATransferService | OTAClient | kWrite | true |
| HMICommandService | HMIController | kWrite | false |

---

## 3. 接口定义

### 3.1 IdentityToken

```cpp
struct IdentityToken {
    string subjectId;           // 主体 ID（证书 CN）
    string issuer;              // OEM-CA
    int64_t notBefore;          // 有效期开始（Unix timestamp）
    int64_t notAfter;           // 有效期结束（默认 notBefore + 3600s）
    vector<string> roles;       // 角色列表
    vector<uint8_t> signature;  // ECDSA-P256 签名

    bool IsValid() const;  // 检查当前时间在 [notBefore, notAfter]
};
```

### 3.2 IdentityAccessManagement（单例）

```cpp
class IdentityAccessManagement {
    bool Initialize();          // 加载默认策略

    IamResult VerifyIdentity(token, certChain);  // 证书链验签
    IamResult CheckAccess(token, resourceId, level);  // 权限检查
    IdentityToken IssueRuntimeToken(appId, roles);  // 颁发运行时 Token

    void RevokeToken(const string& subjectId);  // 吊销
    void SetAuditCallback(AuditCallback);       // 审计回调
    vector<string> GetRoles(const string& subjectId) const;
};
```

---

## 4. Token 生命周期

```
ara::exec 启动 AA →
  IssueRuntimeToken("VehicleSignalService_AA", {"DriveAssist"})
    → Token 有效期 1h

AA 访问资源 →
  CheckAccess(token, "VehicleSignalService", kRead)
    → kGranted（角色匹配策略）

Token 过期 →
  CheckAccess 返回 kInvalidToken
  → AA 需重新请求 Token（调用 IssueRuntimeToken）

异常访问 →
  CheckAccess 返回 kDenied
  → AuditCallback 触发 → 上报 ara::idsm
```

---

## 5. 与其他模块集成

```
ara::exec → IssueRuntimeToken()（进程启动时颁发）
ara::crypto → VerifyIdentity() 使用 ECDSA-P256 验证证书签名
ara::idsm → AuditCallback 上报拒绝访问事件
ara::log → 所有访问结果记录到 DLT 日志
```
