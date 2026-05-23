/**
 * @file dse_export.h
 * @brief 符号导出/导入宏定义
 *
 * 用法：
 *   class DSE_EXPORT MyClass { ... };
 *   DSE_EXPORT void MyFunction();
 *
 * 构建 dse_engine SHARED 时，CMake 定义 DSE_ENGINE_EXPORTS，
 * 此时 DSE_EXPORT 展开为 __declspec(dllexport)。
 * 消费端（apps/modules/tests）链接 dse_engine.dll 时，
 * DSE_ENGINE_EXPORTS 未定义，展开为 __declspec(dllimport)。
 * 静态构建时 DSE_EXPORT 为空。
 */

#ifndef DSE_EXPORT_H
#define DSE_EXPORT_H

#if defined(DSE_BUILD_STATIC)
    // 静态库：无需导出/导入
    #define DSE_EXPORT
#elif defined(_WIN32)
    #if defined(DSE_ENGINE_EXPORTS)
        #define DSE_EXPORT __declspec(dllexport)
    #else
        #define DSE_EXPORT __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define DSE_EXPORT __attribute__((visibility("default")))
#else
    #define DSE_EXPORT
#endif

#endif // DSE_EXPORT_H
