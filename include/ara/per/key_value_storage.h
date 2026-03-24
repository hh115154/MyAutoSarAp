/**
 * @file key_value_storage.h
 * @brief ara::per 持久化存储 - KV 存储
 *
 * 符合 AUTOSAR AP R25-11 规范
 * Functional Cluster: Persistency
 * Ref: AUTOSAR_SWS_Persistency (R25-11)
 */

#ifndef ARA_PER_KEY_VALUE_STORAGE_H
#define ARA_PER_KEY_VALUE_STORAGE_H

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace ara {
namespace per {

/**
 * @brief 持久化操作结果
 */
enum class PerErrc : uint8_t {
    kSuccess         = 0,
    kStorageNotFound = 1,
    kKeyNotFound     = 2,
    kPhysicalStorageError = 3,
    kIntegrityError  = 4,
    kEncryptionError = 5
};

/**
 * @brief KV 存储接口
 *
 * 提供键值对的持久化读写能力
 *
 * @code
 * auto kvs = ara::per::OpenKeyValueStorage("CalibrationData");
 * kvs.SetValue("sensor_offset", 0.5f);
 * kvs.SyncToStorage();
 *
 * auto val = kvs.GetValue<float>("sensor_offset");
 * @endcode
 */
class KeyValueStorage {
public:
    explicit KeyValueStorage(const std::string& storage_id)
        : storage_id_(storage_id) {}
    ~KeyValueStorage() = default;

    /**
     * @brief 写入键值对
     */
    template <typename T>
    PerErrc SetValue(const std::string& key, const T& value) noexcept;

    /**
     * @brief 读取键值对
     * @return 值（不存在返回 nullopt）
     */
    template <typename T>
    std::optional<T> GetValue(const std::string& key) const noexcept;

    /**
     * @brief 删除键
     */
    PerErrc RemoveKey(const std::string& key) noexcept;

    /**
     * @brief 获取所有键
     */
    std::vector<std::string> GetAllKeys() const noexcept;

    /**
     * @brief 同步数据到物理存储
     */
    PerErrc SyncToStorage() noexcept;

    /**
     * @brief 恢复出厂默认值
     */
    PerErrc RecoverKeyValueStorage() noexcept;

private:
    std::string storage_id_;
};

/**
 * @brief 打开 KV 存储
 * @param storage_id 存储标识符（在 ARXML 中定义）
 * @return KeyValueStorage 实例
 */
KeyValueStorage OpenKeyValueStorage(const std::string& storage_id);

} // namespace per
} // namespace ara

#endif // ARA_PER_KEY_VALUE_STORAGE_H
