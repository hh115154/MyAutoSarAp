/**
 * @file logger.h
 * @brief ara::log 日志接口
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Logging and Tracing
 * Ref: AUTOSAR_SWS_LogAndTrace (R25-11)
 */

#ifndef ARA_LOG_LOGGER_H
#define ARA_LOG_LOGGER_H

#include <string>
#include <sstream>
#include <cstdint>

namespace ara {
namespace log {

/**
 * @brief 日志级别
 */
enum class LogLevel : uint8_t {
    kOff     = 0x00,  ///< 关闭日志
    kFatal   = 0x01,  ///< 致命错误
    kError   = 0x02,  ///< 错误
    kWarn    = 0x03,  ///< 警告
    kInfo    = 0x04,  ///< 信息
    kDebug   = 0x05,  ///< 调试
    kVerbose = 0x06   ///< 详细
};

/**
 * @brief 日志模式
 */
enum class LogMode : uint8_t {
    kRemote  = 0x01,  ///< DLT 远程日志
    kFile    = 0x02,  ///< 文件日志
    kConsole = 0x04   ///< 控制台输出
};

/**
 * @brief 日志流（LogStream）
 *
 * 通过流式操作符拼装日志内容
 */
class LogStream {
public:
    explicit LogStream(LogLevel level, const std::string& ctx_id)
        : level_(level), ctx_id_(ctx_id) {}

    ~LogStream();  // 析构时输出日志

    template <typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::string ctx_id_;
    std::ostringstream stream_;
};

/**
 * @brief Logger 类
 *
 * 每个应用/模块创建各自的 Logger 实例
 *
 * @code
 * auto& logger = ara::log::CreateLogger("APP1", "Main application logger");
 * logger.LogInfo() << "Application started";
 * logger.LogError() << "Error code: " << error_code;
 * @endcode
 */
class Logger {
public:
    Logger(const std::string& ctx_id, const std::string& ctx_desc,
           LogLevel level = LogLevel::kInfo);

    LogStream LogFatal() { return LogStream(LogLevel::kFatal, ctx_id_); }
    LogStream LogError() { return LogStream(LogLevel::kError, ctx_id_); }
    LogStream LogWarn()  { return LogStream(LogLevel::kWarn,  ctx_id_); }
    LogStream LogInfo()  { return LogStream(LogLevel::kInfo,  ctx_id_); }
    LogStream LogDebug() { return LogStream(LogLevel::kDebug, ctx_id_); }
    LogStream LogVerbose() { return LogStream(LogLevel::kVerbose, ctx_id_); }

    bool IsEnabled(LogLevel level) const { return level <= level_; }

private:
    std::string ctx_id_;
    std::string ctx_desc_;
    LogLevel level_;
};

/**
 * @brief 初始化日志系统
 * @param app_id      应用标识（4字符）
 * @param app_desc    应用描述
 * @param level       默认日志级别
 * @param log_mode    日志输出模式
 */
void InitLogging(const std::string& app_id,
                 const std::string& app_desc,
                 LogLevel level = LogLevel::kInfo,
                 LogMode log_mode = LogMode::kConsole);

/**
 * @brief 创建 Logger 实例
 * @param ctx_id   上下文标识（4字符）
 * @param ctx_desc 上下文描述
 * @param level    日志级别
 * @return Logger 引用
 */
Logger& CreateLogger(const std::string& ctx_id,
                     const std::string& ctx_desc,
                     LogLevel level = LogLevel::kVerbose);

} // namespace log
} // namespace ara

#endif // ARA_LOG_LOGGER_H
