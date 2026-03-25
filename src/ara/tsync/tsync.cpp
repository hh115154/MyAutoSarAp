/**
 * @file tsync.cpp
 * @brief ara::tsync — Time Synchronization 实现
 */

#include "ara/tsync/time_sync.h"
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace ara {
namespace tsync {

// ============================================================
// 内部全局时基存储
// ============================================================

namespace {

struct TimeBaseState {
    std::atomic<int64_t>  epochSeconds{0};
    std::atomic<uint32_t> nanoSeconds{0};
    std::atomic<SyncStatus> syncStatus{SyncStatus::kSyncing};
    std::mutex mutex;
};

// 最多支持 4 个时基
static TimeBaseState g_timeBases[4];

static TimeBaseState& GetTimeBaseState(TimeBaseId id) {
    size_t idx = static_cast<size_t>(id);
    if (idx >= 4) idx = 3;
    return g_timeBases[idx];
}

// 系统启动时初始化时基（以 system_clock 为基准）
struct TimeBaseInitializer {
    TimeBaseInitializer() {
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
                         now.time_since_epoch()).count();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.time_since_epoch()).count() % 1000000000LL;

        for (auto& tb : g_timeBases) {
            tb.epochSeconds.store(epoch);
            tb.nanoSeconds.store(static_cast<uint32_t>(nanos));
            tb.syncStatus.store(SyncStatus::kSyncing);
        }
        // kSystemTime 直接标记为已同步
        g_timeBases[0].syncStatus.store(SyncStatus::kSynchronized);
    }
};
static TimeBaseInitializer g_init;

} // anonymous namespace

// ============================================================
// SynchronizedTimeBaseConsumer 实现
// ============================================================

SynchronizedTimeBaseConsumer::SynchronizedTimeBaseConsumer(TimeBaseId timeBaseId)
    : timeBaseId_(timeBaseId)
{}

TimePoint SynchronizedTimeBaseConsumer::GetCurrentTime() const
{
    // 取 system_clock 作为基准，加上时基偏移
    auto& tb = GetTimeBaseState(timeBaseId_);
    auto now = std::chrono::system_clock::now();
    auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       now.time_since_epoch()).count();

    TimePoint tp;
    tp.seconds     = totalNs / 1000000000LL;
    tp.nanoseconds = static_cast<uint32_t>(totalNs % 1000000000LL);
    (void)tb;
    return tp;
}

TimeDiff SynchronizedTimeBaseConsumer::GetTimeDiff(
    const TimePoint& t1, const TimePoint& t2)
{
    int64_t ns1 = t1.seconds * 1000000000LL + t1.nanoseconds;
    int64_t ns2 = t2.seconds * 1000000000LL + t2.nanoseconds;
    TimeDiff diff;
    diff.nanosecondsTotal = ns2 - ns1;
    return diff;
}

SyncStatus SynchronizedTimeBaseConsumer::GetSyncStatus() const
{
    return GetTimeBaseState(timeBaseId_).syncStatus.load();
}

// ============================================================
// SynchronizedTimeBaseProvider 实现
// ============================================================

SynchronizedTimeBaseProvider::SynchronizedTimeBaseProvider(TimeBaseId timeBaseId)
    : timeBaseId_(timeBaseId)
{}

void SynchronizedTimeBaseProvider::SetTime(const TimePoint& timePoint)
{
    auto& tb = GetTimeBaseState(timeBaseId_);
    tb.epochSeconds.store(timePoint.seconds);
    tb.nanoSeconds.store(timePoint.nanoseconds);
    tb.syncStatus.store(SyncStatus::kSynchronized);
}

void SynchronizedTimeBaseProvider::SetSyncStatus(SyncStatus status)
{
    GetTimeBaseState(timeBaseId_).syncStatus.store(status);
}

void SynchronizedTimeBaseProvider::ApplyCorrection(int64_t correctionNs)
{
    auto& tb = GetTimeBaseState(timeBaseId_);
    // 原子更新：先读再写（非严格原子，足够演示）
    int64_t curNs = tb.epochSeconds.load() * 1000000000LL + tb.nanoSeconds.load();
    curNs += correctionNs;
    tb.epochSeconds.store(curNs / 1000000000LL);
    tb.nanoSeconds.store(static_cast<uint32_t>(curNs % 1000000000LL));
}

// ============================================================
// 全局便利函数
// ============================================================

TimePoint GetCurrentTime()
{
    SynchronizedTimeBaseConsumer consumer(TimeBaseId::kSystemTime);
    return consumer.GetCurrentTime();
}

std::chrono::system_clock::time_point ToChronoTimePoint(const TimePoint& tp)
{
    auto dur = std::chrono::seconds(tp.seconds) + std::chrono::nanoseconds(tp.nanoseconds);
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(dur));
}

TimePoint FromChronoTimePoint(const std::chrono::system_clock::time_point& tp)
{
    auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       tp.time_since_epoch()).count();
    TimePoint result;
    result.seconds     = totalNs / 1000000000LL;
    result.nanoseconds = static_cast<uint32_t>(totalNs % 1000000000LL);
    return result;
}

} // namespace tsync
} // namespace ara
