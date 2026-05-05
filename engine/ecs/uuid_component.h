/**
 * @file uuid_component.h
 * @brief UUID 组件，为 Entity 提供跨场景唯一标识
 */

#ifndef DSE_ECS_UUID_COMPONENT_H
#define DSE_ECS_UUID_COMPONENT_H

#include <cstdint>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>

/**
 * @struct UUIDComponent
 * @brief 跨场景唯一标识组件
 *
 * 每个需要跨场景引用的 Entity 都应附加此组件。
 * UUID 在序列化时保存，反序列化时恢复。
 */
struct UUIDComponent {
    uint64_t uuid = 0;

    /**
     * @brief 生成一个新的随机 UUID
     */
    static uint64_t Generate() {
        static std::mt19937_64 engine(std::random_device{}());
        static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
        return dist(engine);
    }

    /**
     * @brief 将 UUID 转为十六进制字符串
     */
    std::string ToString() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << uuid;
        return oss.str();
    }

    /**
     * @brief 从十六进制字符串解析 UUID
     */
    static uint64_t FromString(const std::string& str) {
        uint64_t val = 0;
        std::istringstream iss(str);
        iss >> std::hex >> val;
        return val;
    }
};

#endif // DSE_ECS_UUID_COMPONENT_H
