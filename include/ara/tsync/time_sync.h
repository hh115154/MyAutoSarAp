/**
 * @file time_sync.h
 * @brief ara::tsync — Time Synchronization
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Time Synchronization
 * Ref: AUTOSAR_SWS_TimeSync (R25-11)
 *
 * 设计依据：SOC-SW-001 §3.4
 * 提供全局时基（TSP: Time Synchronization Protocol）
 * SOC 侧通过 ara::tsync 与 MCU 侧 StbM/GptSrv 时钟同步
 */

#ifndef ARA_TSYNC_TIME_SYNC_H
#define ARA_TSYNC_TIME_SYNC_H

#include <cstdint>
#include <string>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>

namespace ara {
namespace tsync {

// ============================================================
// 时间类型定义
// ============================================================

/**
 * @brief 高精度时间点（AUTOSAR 时间戳格式）
 * 精度：纳秒级
 * 参考：SWS_TS_00010
 */
struct TimePoint {
    int64_t  seconds;      ///< 自 Epoch 以来的秒数
    uint32_t nanoseconds;  ///< 秒内纳秒偏移（0 ~ 999_999_999）

    bool operator==(const TimePoint& other) const {
        return seconds == other.seconds && nanoseconds == other.nanoseconds;
    }
    bool operator<(const TimePoint& other) const {
        if (seconds != other.seconds) return seconds < other.seconds;
        return nanoseconds < other.nanoseconds;
    }
    bool operator<=(const TimePoint& other) const {
        return !(other < *this);
    }
};

/**
 * @brief 时间差（ΔT）
 */
struct TimeDiff {
    int64_t  nanosecondsTotal;  ///< 总差值（纳秒，可负）
};

/**
 * @brief 时基 ID（与 ARXML 配置对应）
 */
enum class TimeBaseId : uint8_t {
    kSystemTime    = 0,  ///< 系统时基（PTP/gPTP 同步）
    kVehicleTime   = 1,  ///< 整车时基（由 MCU/BCM 提供）
    kGpsTime       = 2,  ///< GPS 时基（由导航模块提供）
    kLocalMono     = 3   ///< 本地单调时钟（不同步，仅用于相对时间）
};

/**
 * @brief 时钟同步状态
 */
enum class SyncStatus : uint8_t {
    kSynchronized  = 0,  ///< 时钟已同步，时间可信
    kSyncing       = 1,  ///< 同步进行中
    kLostSync      = 2,  ///< 失去同步（时间不可信）
    kNotSupported  = 3   ///< 该时基不支持
};

// ============================================================
// SynchronizedTimeBaseConsumer — 时基消费接口
// ============================================================

/**
 * @brief 时基消费者接口（SWS_TS_00030）
 *
 * AA 通过此类获取全局时间：
 *   - GetCurrentTime()  获取当前时间点
 *   - GetTimeDiff()     计算两个时间点的差值
 *   - GetSyncStatus()   查询同步状态
 */
class SynchronizedTimeBaseConsumer {
public:
    /**
     * @brief 构造时基消费者
     * @param timeBaseId 要消费的时基 ID
     */
    explicit SynchronizedTimeBaseConsumer(TimeBaseId timeBaseId);
    ~SynchronizedTimeBaseConsumer() = default;

    /**
     * @brief 获取当前时间（SWS_TS_00050）
     * @return 当前时间点（UTC + 纳秒精度）
     *
     * 若同步状态为 kLostSync，返回的时间戳仍可用，
     * 但精度无法保证（漂移可能超过 1ms）。
     */
    TimePoint GetCurrentTime() const;

    /**
     * @brief 计算两个时间点之差（SWS_TS_00055）
     * @param t1  较早时间
     * @param t2  较晚时间
     * @return    t2 - t1 的差值（纳秒）
     */
    static TimeDiff GetTimeDiff(const TimePoint& t1, const TimePoint& t2);

    /**
     * @brief 获取时基同步状态
     */
    SyncStatus GetSyncStatus() const;

    /**
     * @brief 获取时基 ID
     */
    TimeBaseId GetTimeBaseId() const { return timeBaseId_; }

private:
    TimeBaseId timeBaseId_;
};

// ============================================================
// SynchronizedTimeBaseProvider — 时基提供接口（可选）
// ============================================================

/**
 * @brief 时基提供者接口（SWS_TS_00060）
 *
 * 用于时基主节点（Master）设置时间，
 * 在本系统中 SOC 接收外部 PTP/gPTP 时钟，由平台实现自动更新。
 */
class SynchronizedTimeBaseProvider {
public:
    explicit SynchronizedTimeBaseProvider(TimeBaseId timeBaseId);
    ~SynchronizedTimeBaseProvider() = default;

    /**
     * @brief 设置当前时间（由时钟驱动调用）
     */
    void SetTime(const TimePoint& timePoint);

    /**
     * @brief 设置同步状态
     */
    void SetSyncStatus(SyncStatus status);

    /**
     * @brief 应用时间校正（ΔT 补偿，SWS_TS_00080）
     * @param correction 校正量（纳秒，可正可负）
     *
     * 对应 SOC-SW-001：TSP ΔT 校正，
     * 与 MCU StbM 时基偏差 ≤ 1ms
     */
    void ApplyCorrection(int64_t correctionNs);

private:
    TimeBaseId timeBaseId_;
};

// ============================================================
// 全局便利函数
// ============================================================

/**
 * @brief 获取系统时基当前时间（便利函数）
 * 等价于 SynchronizedTimeBaseConsumer(kSystemTime).GetCurrentTime()
 */
TimePoint GetCurrentTime();

/**
 * @brief 将 TimePoint 转换为 std::chrono::system_clock::time_point
 */
std::chrono::system_clock::time_point ToChronoTimePoint(const TimePoint& tp);

/**
 * @brief 将 std::chrono::system_clock::time_point 转换为 TimePoint
 */
TimePoint FromChronoTimePoint(const std::chrono::system_clock::time_point& tp);

} // namespace tsync
} // namespace ara

#endif // ARA_TSYNC_TIME_SYNC_H
