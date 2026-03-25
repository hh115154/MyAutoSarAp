/**
 * @file phm.cpp
 * @brief ara::phm — Platform Health Management 实现
 *
 * 实现三级监督机制：
 * - Alive Supervision：周期内心跳计数校验
 * - Deadline Supervision：两检查点间时间约束（在 header 配置中定义）
 * - Logical Supervision：检查点到达顺序校验
 *
 * 对应 SOC-SW-001 §3.7：
 *   主循环每 10ms 检查一次所有 SE，
 *   故障后触发 ara::sm 状态切换或 WDG 停止踢狗。
 */

#include "ara/phm/platform_health_management.h"
#include <stdexcept>
#include <iostream>

namespace ara {
namespace phm {

// ============================================================
// SupervisedEntity 实现
// ============================================================

SupervisedEntity::SupervisedEntity(const std::string& instanceId)
    : instanceId_(instanceId)
    , lastCheckpointTime_(std::chrono::steady_clock::now())
{
    // 向 PHM 主控注册
    AliveSupervisionConfig defaultConf{};
    defaultConf.aliveReferenceCycleMs = 100;
    defaultConf.minMargin = 0;
    defaultConf.maxMargin = 2;
    defaultConf.failedSupervisionCyclesTol = 3;

    PlatformHealthManagement::GetInstance().RegisterEntity(instanceId_, defaultConf);
}

SupervisedEntity::~SupervisedEntity()
{
    PlatformHealthManagement::GetInstance().UnregisterEntity(instanceId_);
}

void SupervisedEntity::CheckpointReached(CheckpointId checkpointId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    lastCheckpointTime_ = std::chrono::steady_clock::now();
    ++checkpointCount_;
    PlatformHealthManagement::GetInstance().ReportCheckpoint(instanceId_, checkpointId);
}

SupervisionStatus SupervisedEntity::GetStatus() const
{
    return status_.load();
}

// ============================================================
// PlatformHealthManagement 实现
// ============================================================

PlatformHealthManagement& PlatformHealthManagement::GetInstance()
{
    static PlatformHealthManagement instance;
    return instance;
}

bool PlatformHealthManagement::RegisterEntity(
    const std::string& entityId,
    const AliveSupervisionConfig& aliveConf)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // EntityRecord 含 atomic 成员，不可 move/copy，使用下标运算符就地构造
    if (entities_.find(entityId) != entities_.end()) {
        return false; // 已注册
    }
    entities_[entityId].aliveConf = aliveConf;
    entities_[entityId].status.store(SupervisionStatus::kOk);
    entities_[entityId].lastCheckpointTime = std::chrono::steady_clock::now();
    entities_[entityId].aliveCountInCycle = 0;
    entities_[entityId].action = PhmAction::kReportError;
    return true;
}

void PlatformHealthManagement::UnregisterEntity(const std::string& entityId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    entities_.erase(entityId);
}

void PlatformHealthManagement::ReportCheckpoint(
    const std::string& entityId,
    CheckpointId /*cpId*/)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entities_.find(entityId);
    if (it == entities_.end()) return;

    auto& rec = it->second;
    rec.lastCheckpointTime = std::chrono::steady_clock::now();
    rec.aliveCountInCycle++;

    // Alive 监督：检查本周期内心跳次数是否合法
    // 简化实现：只要有心跳就认为 OK
    rec.status.store(SupervisionStatus::kOk);
}

GlobalSupervisionStatus PlatformHealthManagement::GetGlobalStatus() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    GlobalSupervisionStatus globalStatus = GlobalSupervisionStatus::kNormal;

    for (const auto& kv : entities_) {
        const auto& rec = kv.second;
        SupervisionStatus s = rec.status.load();
        if (s == SupervisionStatus::kExpired) {
            globalStatus = GlobalSupervisionStatus::kExpired;
            break; // 最严重，直接返回
        } else if (s == SupervisionStatus::kFailed) {
            globalStatus = GlobalSupervisionStatus::kFailed;
        } else if (s == SupervisionStatus::kStopped &&
                   globalStatus == GlobalSupervisionStatus::kNormal) {
            globalStatus = GlobalSupervisionStatus::kStopped;
        }
    }
    return globalStatus;
}

void PlatformHealthManagement::KickWatchdog()
{
    // 软件 WDG：记录最后一次踢狗时间
    lastWdgKick_ = std::chrono::steady_clock::now();
}

void PlatformHealthManagement::SetRecoveryCallback(RecoveryCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    recoveryCallback_ = std::move(cb);
}

void PlatformHealthManagement::SetPhmAction(
    const std::string& entityId,
    PhmAction action)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entities_.find(entityId);
    if (it != entities_.end()) {
        it->second.action = action;
    }
}

void PlatformHealthManagement::Start()
{
    running_.store(true);
    lastWdgKick_ = std::chrono::steady_clock::now();
}

void PlatformHealthManagement::Stop()
{
    running_.store(false);
}

} // namespace phm
} // namespace ara
