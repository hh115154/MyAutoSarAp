/**
 * @file    vehicle_signal_swc.h
 * @brief   AP SWC — VehicleSignalService Consumer（SOME/IP 接收侧）
 *
 * 对应 SOC-SW-001 §4.1 VehicleSignalService SWC
 * 通过 ara::com VehicleSignalProxy 订阅 MCU 发布的整车信号
 */

#pragma once

#include <memory>

namespace ara {
namespace app {

class VehicleSignalSwc {
public:
    VehicleSignalSwc();
    ~VehicleSignalSwc();

    /** @brief 启动 SWC（订阅事件，启动接收） */
    void Start();

    /** @brief 停止 SWC */
    void Stop();

    /** @brief 100ms 主函数（拉取并打印最新样本统计） */
    void MainFunction_100ms();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace app
} // namespace ara
