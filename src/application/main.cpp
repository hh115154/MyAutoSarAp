/**
 * @file main.cpp
 * @brief AUTOSAR AP R25-11 — Adaptive Application 入口
 *
 * 启动流程（SOC-SW-001 §5.2）：
 *   1. ara::log 初始化
 *   2. VehicleSignalSWC 启动（SOME/IP Consumer，订阅 MCU UDP:30501）
 *   3. ara::exec 上报 kRunning
 *   4. 100ms 主循环：SWC MainFunction
 *   5. 优雅退出（SIGINT/SIGTERM）
 *
 * 本机仿真架构：
 *   MyAutoSarCP(MCU进程) ─UDP:30501→ MyAutoSarAp(SOC进程)
 *   SOME/IP VehicleSignalService(0x1001) Notification 10ms 周期
 */

#include "ara/log/logger.h"
#include "ara/exec/execution_client.h"
#include "ara/app/vehicle_signal_swc.h"

#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>

/// 全局退出标志
static std::atomic<bool> g_running{true};

static void SignalHandler(int /*sig*/) {
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // =========================================================
    // Step 1: 初始化日志系统
    // =========================================================
    ara::log::InitLogging(
        "MSAP",
        "MyAutoSarAp — AUTOSAR AP R25-11 (SOME/IP Consumer)",
        ara::log::LogLevel::kDebug,
        ara::log::LogMode::kConsole
    );

    auto& logger = ara::log::CreateLogger("MAIN", "Main context");

    printf("==============================================\n");
    printf("  MyAutoSarAp  —  AUTOSAR AP R25-11\n");
    printf("  SOME/IP VehicleSignalService Consumer\n");
    printf("  Listening UDP 127.0.0.1:30501\n");
    printf("==============================================\n\n");

    logger.LogInfo() << "MyAutoSarAp starting...";

    // =========================================================
    // Step 2: 注册信号处理
    // =========================================================
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGINT,  SignalHandler);

    // =========================================================
    // Step 3: 启动 VehicleSignalSWC（ara::com Proxy）
    // =========================================================
    ara::app::VehicleSignalSwc vehicleSignalSwc;
    vehicleSignalSwc.Start();

    // =========================================================
    // Step 4: 上报 ExecutionState::kRunning
    // =========================================================
    ara::exec::ExecutionClient execClient;
    auto result = execClient.ReportExecutionState(
        ara::exec::ExecutionState::kRunning);

    if (result != ara::exec::ExecErrc::kSuccess) {
        logger.LogError() << "Failed to report ExecutionState::kRunning";
        return -1;
    }

    logger.LogInfo()
        << "Application running — waiting for SOME/IP data from MCU (Ctrl+C to stop)";

    // =========================================================
    // Step 5: 主循环（100ms）
    // =========================================================
    while (g_running.load()) {
        vehicleSignalSwc.MainFunction_100ms();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // =========================================================
    // Step 6: 优雅退出
    // =========================================================
    logger.LogInfo() << "Shutdown requested...";
    vehicleSignalSwc.Stop();
    logger.LogInfo() << "MyAutoSarAp stopped.";

    return 0;
}
