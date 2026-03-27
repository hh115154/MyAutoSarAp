/**
 * @file    vehicle_signal_swc.cpp
 * @brief   AP VehicleSignalService SWC 实现
 *
 * 功能：
 *   - FindService 获取 VehicleSignalProxy
 *   - 订阅 VehicleSignal Event（Push 模式）
 *   - 每收到新 sample，通过 ara::log 记录关键信号
 *   - 每 100ms 打印统计（RX 帧率、E2E 错误）
 */

#include "ara/app/vehicle_signal_swc.h"
#include "ara/com/vehicle_signal_proxy.h"
#include "ara/log/logger.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace ara {
namespace app {

// ----------------------------------------------------------------
// Pimpl
// ----------------------------------------------------------------
struct VehicleSignalSwc::Impl {
    std::shared_ptr<ara::com::vehicle::VehicleSignalProxy> proxy;
    ara::log::Logger logger;

    std::atomic<uint64_t> samplesReceived{0};
    float lastSpeedKmh{0.0f};
    float lastRpm{0.0f};
    float lastFuelPct{0.0f};
    uint8_t lastE2eCounter{0};

    std::mutex dataMtx;

    Impl()
        : logger(ara::log::CreateLogger("VSIG", "VehicleSignalSWC"))
    {}
};

// ----------------------------------------------------------------
// Constructor / Destructor
// ----------------------------------------------------------------

VehicleSignalSwc::VehicleSignalSwc()
    : impl_(std::make_unique<Impl>())
{}

VehicleSignalSwc::~VehicleSignalSwc()
{
    Stop();
}

// ----------------------------------------------------------------
// Start
// ----------------------------------------------------------------

void VehicleSignalSwc::Start()
{
    impl_->logger.LogInfo()
        << "[VehicleSignalSWC] Starting... FindService(0x1001)";

    impl_->proxy = ara::com::vehicle::VehicleSignalProxy::FindService(
        VEHICLE_SIGNAL_SVC_PORT);

    if (!impl_->proxy) {
        impl_->logger.LogError()
            << "[VehicleSignalSWC] FindService failed — socket bind error";
        printf("[SOC_LOG] {\"level\":\"ERROR\",\"ctx\":\"VSIG\","
               "\"msg\":\"FindService FAILED\",\"port\":%d}\n",
               VEHICLE_SIGNAL_SVC_PORT);
        fflush(stdout);
        return;
    }

    printf("[SOC_LOG] {\"level\":\"INFO\",\"ctx\":\"VSIG\","
           "\"msg\":\"FindService OK\",\"port\":%d}\n",
           VEHICLE_SIGNAL_SVC_PORT);
    fflush(stdout);

    // 订阅 Event（最多缓存 32 个 sample）
    impl_->proxy->VehicleSignal.Subscribe(32);

    printf("[SOC_LOG] {\"level\":\"INFO\",\"ctx\":\"VSIG\","
           "\"msg\":\"Subscribed to VehicleSignal Event\",\"port\":%d}\n",
           VEHICLE_SIGNAL_SVC_PORT);
    fflush(stdout);

    // 设置 Push 回调
    impl_->proxy->VehicleSignal.SetReceiveHandler(
        [this](std::shared_ptr<ara::com::vehicle::VehicleSignalSample> sample) {
            std::lock_guard<std::mutex> lk(impl_->dataMtx);
            impl_->lastSpeedKmh  = sample->vehicleSpeedKmh;
            impl_->lastRpm       = sample->engineRpm;
            impl_->lastFuelPct   = sample->fuelLevelPct;
            impl_->lastE2eCounter = sample->e2eCounter;
            impl_->samplesReceived.fetch_add(1u);

            /* ── 结构化 JSON 日志（每帧输出，monitor_server 解析此行采集数据）─ */
            /* 格式: [AP_SIGNAL_JSON] {"speed":xx,"rpm":xx,"brake":x,
             *        "steer":xx,"door":x,"fuel":xx,"e2e_ok":1,
             *        "e2e_crc":xx,"e2e_cnt":xx,"session":xx} */
            printf("[AP_SIGNAL_JSON] {"
                   "\"speed\":%.2f,"
                   "\"rpm\":%.1f,"
                   "\"brake\":%d,"
                   "\"steer\":%.2f,"
                   "\"door\":%d,"
                   "\"fuel\":%.2f,"
                   "\"e2e_ok\":1,"
                   "\"e2e_crc\":%d,"
                   "\"e2e_cnt\":%d,"
                   "\"session\":%d"
                   "}\n",
                   sample->vehicleSpeedKmh,
                   sample->engineRpm,
                   sample->brakePedal ? 1 : 0,
                   sample->steeringAngleDeg,
                   (int)sample->doorStatus,
                   sample->fuelLevelPct,
                   (int)sample->e2eCrc,
                   (int)sample->e2eCounter,
                   (int)sample->sessionId);
            fflush(stdout);

            // 每 100 帧输出一条 SOC_LOG（实时日志面板可见）
            uint64_t n = impl_->samplesReceived.load();
            if (n % 100u == 1u) {
                printf("[SOC_LOG] {\"level\":\"INFO\",\"ctx\":\"VSIG\","
                       "\"msg\":\"SOMEIP_RX\","
                       "\"session\":%d,\"speed\":%.1f,\"rpm\":%.0f,"
                       "\"steer\":%.1f,\"brake\":%d,\"door\":%d,"
                       "\"fuel\":%.1f,\"e2e_crc\":%d,\"e2e_cnt\":%d,"
                       "\"rx_total\":%llu}\n",
                       (int)sample->sessionId,
                       sample->vehicleSpeedKmh,
                       sample->engineRpm,
                       sample->steeringAngleDeg,
                       sample->brakePedal ? 1 : 0,
                       (int)sample->doorStatus,
                       sample->fuelLevelPct,
                       (int)sample->e2eCrc,
                       (int)sample->e2eCounter,
                       (unsigned long long)n);
                fflush(stdout);
            }

            // 每 100 帧 ara::log 详细打印一次（避免刷屏）
            if (n % 100u == 1u) {
                impl_->logger.LogInfo()
                    << "[SOMEIP RX] SID=0x1001"
                    << " session=" << sample->sessionId
                    << " speed=" << sample->vehicleSpeedKmh << "km/h"
                    << " rpm=" << sample->engineRpm
                    << " brake=" << (sample->brakePedal ? "ON" : "OFF")
                    << " fuel=" << sample->fuelLevelPct << "%"
                    << " E2E[crc=" << static_cast<int>(sample->e2eCrc)
                    << " cnt=" << static_cast<int>(sample->e2eCounter) << "] OK";
            }
        });

    impl_->logger.LogInfo()
        << "[VehicleSignalSWC] Subscribed to VehicleSignal Event (UDP 127.0.0.1:"
        << VEHICLE_SIGNAL_SVC_PORT << ")";
}

// ----------------------------------------------------------------
// Stop
// ----------------------------------------------------------------

void VehicleSignalSwc::Stop()
{
    if (impl_->proxy) {
        impl_->proxy->VehicleSignal.Unsubscribe();
        impl_->proxy.reset();
        impl_->logger.LogInfo() << "[VehicleSignalSWC] Stopped";
        printf("[SOC_LOG] {\"level\":\"INFO\",\"ctx\":\"VSIG\","
               "\"msg\":\"Stopped\",\"rx_total\":%llu}\n",
               (unsigned long long)impl_->samplesReceived.load());
        fflush(stdout);
    }
}

// ----------------------------------------------------------------
// 100ms 主函数
// ----------------------------------------------------------------

void VehicleSignalSwc::MainFunction_100ms()
{
    if (!impl_->proxy) return;

    auto stats = impl_->proxy->GetStats();

    float speed, rpm, fuel;
    {
        std::lock_guard<std::mutex> lk(impl_->dataMtx);
        speed = impl_->lastSpeedKmh;
        rpm   = impl_->lastRpm;
        fuel  = impl_->lastFuelPct;
    }

    // 每秒（10 * 100ms）打印一次统计行
    static uint32_t tick = 0u;
    if (++tick % 10u == 0u) {
        impl_->logger.LogInfo()
            << "─── VehicleSignal 统计 ───"
            << "  rx=" << stats.rxPackets
            << "  bytes=" << stats.rxBytes
            << "  e2eErr=" << stats.e2eErrors
            << "  speed=" << speed << "km/h"
            << "  rpm=" << rpm
            << "  fuel=" << fuel << "%";
    }
}

} // namespace app
} // namespace ara
