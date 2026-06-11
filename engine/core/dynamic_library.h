/**
 * @file dynamic_library.h
 * @brief 跨平台动态库加载器，用于在运行时加载插件模块 (.dll / .so / .dylib)
 */

#pragma once

#include "engine/core/dse_export.h"
#include <string>

namespace dse {
namespace core {

/**
 * @class DynamicLibrary
 * @brief 封装了 LoadLibrary / dlopen 等底层系统调用
 */
class DSE_EXPORT DynamicLibrary {
public:
    DynamicLibrary();
    ~DynamicLibrary();

    // 禁用拷贝
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // 允许移动
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    /**
     * @brief 加载动态库
     * @param library_name 库名称或路径 (不需要加 .dll 或 .so 后缀，内部会自动处理)
     * @return 成功返回 true
     */
    bool Load(const std::string& library_name);

    /**
     * @brief 卸载动态库
     */
    void Unload();

    /**
     * @brief 获取库中导出的符号地址
     * @param symbol_name 符号名称
     * @return 符号地址，如果未找到返回 nullptr
     */
    void* GetSymbol(const std::string& symbol_name) const;

    /**
     * @brief 检查库是否已加载
     */
    bool IsLoaded() const { return handle_ != nullptr; }

private:
    void* handle_ = nullptr;
    std::string path_;
};

} // namespace core
} // namespace dse
