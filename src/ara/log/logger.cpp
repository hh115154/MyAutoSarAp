/**
 * @file logger.cpp
 * @brief ara::log 日志系统实现
 */

#include "ara/log/logger.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <cstdio>
#include <ctime>

namespace ara {
namespace log {

// -------------------------------------------------------
// 全局日志注册表
// -------------------------------------------------------
static std::unordered_map<std::string, Logger*> g_logger_registry;
static std::mutex g_registry_mutex;
static std::string g_app_id;

// -------------------------------------------------------
// LogStream 析构时输出日志
// 格式：[SOC_LOG] {"level":"INFO","ctx":"VSIG","msg":"..."}
// -------------------------------------------------------
LogStream::~LogStream() {
    const char* level_str = "INFO";
    switch (level_) {
        case LogLevel::kFatal:   level_str = "FATAL";   break;
        case LogLevel::kError:   level_str = "ERROR";   break;
        case LogLevel::kWarn:    level_str = "WARN";    break;
        case LogLevel::kInfo:    level_str = "INFO";    break;
        case LogLevel::kDebug:   level_str = "DEBUG";   break;
        case LogLevel::kVerbose: level_str = "VERBOSE"; break;
        default:                 level_str = "INFO";    break;
    }

    // 对消息中的双引号转义
    std::string msg = stream_.str();
    std::string escaped;
    escaped.reserve(msg.size() + 8);
    for (char c : msg) {
        if (c == '"')  { escaped += "\\\""; }
        else if (c == '\\') { escaped += "\\\\"; }
        else           { escaped += c; }
    }

    // 结构化 JSON 输出，monitor_server 通过 [SOC_LOG] 前缀识别
    printf("[SOC_LOG] {\"level\":\"%s\",\"ctx\":\"%s\",\"msg\":\"%s\"}\n",
           level_str, ctx_id_.c_str(), escaped.c_str());
    fflush(stdout);
}

// -------------------------------------------------------
// Logger 实现
// -------------------------------------------------------
Logger::Logger(const std::string& ctx_id,
               const std::string& ctx_desc,
               LogLevel level)
    : ctx_id_(ctx_id), ctx_desc_(ctx_desc), level_(level) {}

// -------------------------------------------------------
// 全局函数实现
// -------------------------------------------------------
void InitLogging(const std::string& app_id,
                 const std::string& app_desc,
                 LogLevel level,
                 LogMode log_mode) {
    (void)app_desc;
    (void)level;
    (void)log_mode;
    g_app_id = app_id;
    printf("[SOC_LOG] {\"level\":\"INFO\",\"ctx\":\"SYS\","
           "\"msg\":\"ara::log initialized\",\"app_id\":\"%s\"}\n",
           app_id.c_str());
    fflush(stdout);
}

Logger& CreateLogger(const std::string& ctx_id,
                     const std::string& ctx_desc,
                     LogLevel level) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto it = g_logger_registry.find(ctx_id);
    if (it != g_logger_registry.end()) {
        return *it->second;
    }
    auto* logger = new Logger(ctx_id, ctx_desc, level);
    g_logger_registry[ctx_id] = logger;
    return *logger;
}

} // namespace log
} // namespace ara
