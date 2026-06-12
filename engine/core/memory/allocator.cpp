/**
 * @file allocator.cpp
 * @brief 内存标签注册表实现。
 */

#include "engine/core/memory/allocator.h"

#include <mutex>
#include <vector>

namespace dse {
namespace core {

namespace {

const char* const kBuiltinTagNames[] = {
    "Default", "Render", "RHI", "Texture", "Mesh", "Material", "Shader",
    "Asset", "Audio", "Physics", "ECS", "Scene", "Scripting", "Net",
    "Navigation", "UI", "Editor", "Job", "FrameTemp",
};

constexpr uint16_t kBuiltinCount = static_cast<uint16_t>(MemoryTag::BuiltinCount);

static_assert(sizeof(kBuiltinTagNames) / sizeof(kBuiltinTagNames[0]) == kBuiltinCount,
              "kBuiltinTagNames must match MemoryTag enum");

struct TagRegistry {
    std::mutex mutex;
    std::vector<const char*> dynamic; // id == kBuiltinCount + index
};

TagRegistry& Registry() {
    static TagRegistry registry;
    return registry;
}

} // namespace

uint16_t RegisterMemoryTag(const char* name) {
    TagRegistry& reg = Registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    reg.dynamic.push_back(name ? name : "Unnamed");
    return static_cast<uint16_t>(kBuiltinCount + reg.dynamic.size() - 1);
}

const char* MemoryTagName(uint16_t tag) {
    if (tag < kBuiltinCount) {
        return kBuiltinTagNames[tag];
    }
    TagRegistry& reg = Registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    const size_t index = static_cast<size_t>(tag) - kBuiltinCount;
    if (index < reg.dynamic.size()) {
        return reg.dynamic[index];
    }
    return "Unknown";
}

uint16_t MemoryTagCount() {
    TagRegistry& reg = Registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    return static_cast<uint16_t>(kBuiltinCount + reg.dynamic.size());
}

} // namespace core
} // namespace dse
