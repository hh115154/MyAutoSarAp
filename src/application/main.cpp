/**
 * @file main.cpp
 * @brief Adaptive Application 入口
 *
 * AutoSAR AP 应用标准启动流程：
 *   1. 初始化日志系统
 *   2. 初始化通信中间件
 *   3. 上报 ExecutionState::kRunning
 *   4. 进入主循环
 *   5. 捕获终止信号，优雅退出
 */

#include "ara/log/logger.h"
#include "ara/exec/execution_client.h"

#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

/// 全局退出标志
static std::atomic<bool> g_running{true};

/**
 * @brief 信号处理器（SIGTERM / SIGINT）
 */
static void SignalHandler(int signal) {
    (void)signal;
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // =========================================================
    // Step 1: 初始化日志系统
    // =========================================================
    ara::log::InitLogging(
        "MSAP",                          // App ID（4字符）
        "MyAutoSarAp Main Application",  // App 描述
        ara::log::LogLevel::kDebug,      // 默认日志级别
        ara::log::LogMode::kConsole      // 日志输出模式
    );

    auto& logger = ara::log::CreateLogger("MAIN", "Main context");
    logger.LogInfo() << "MyAutoSarAp starting...";

    // =========================================================
    // Step 2: 注册信号处理
    // =========================================================
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGINT,  SignalHandler);

    // =========================================================
    // Step 3: 初始化各功能模块（在此处添加）
    // =========================================================
    // TODO: 初始化 ara::com 通信
    // TODO: 初始化应用业务模块

    // =========================================================
    // Step 4: 上报 ExecutionState::kRunning
    //         通知 Execution Management 应用已就绪
    // =========================================================
    ara::exec::ExecutionClient exec_client;
    auto result = exec_client.ReportExecutionState(
        ara::exec::ExecutionState::kRunning);

    if (result != ara::exec::ExecErrc::kSuccess) {
        logger.LogError() << "Failed to report ExecutionState::kRunning";
        return -1;
    }

    logger.LogInfo() << "Application running. Waiting for termination signal...";

    // =========================================================
    // Step 5: 主循环
    // =========================================================
    while (g_running.load()) {
        // TODO: 处理业务逻辑
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // =========================================================
    // Step 6: 优雅退出
    // =========================================================
    logger.LogInfo() << "Termination signal received. Shutting down...";

    // TODO: 停止 ara::com 服务
    // TODO: 清理资源

    logger.LogInfo() << "MyAutoSarAp stopped.";
    return 0;
}
