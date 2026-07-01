# DSEngine HarmonyOS 原生模块构建配置
#
# 此 CMakeLists.txt 由 hvigor 构建系统调用，编译 NAPI 原生模块 libdse_napi.so。
# 引擎核心代码通过源码引入（非预编译库），与引擎主 CMakeLists.txt 共享编译定义。
#
# 构建方式（二选一）：
#   方式 A（推荐）：DevEco Studio 自动构建
#   方式 B（命令行）：
#     cmake -DCMAKE_TOOLCHAIN_FILE=<OHOS_SDK>/native/build/cmake/ohos.toolchain.cmake \
#           -DOHOS_ARCH=arm64-v8a -DCMAKE_BUILD_TYPE=Release ..

cmake_minimum_required(VERSION 3.20)
project(dse_napi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DSEngine 源码根目录（相对于 ohos_app/entry/src/main/cpp/）
set(DSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

# 启用鸿蒙平台支持
set(DSE_ENABLE_HARMONY_PLATFORM ON CACHE BOOL "" FORCE)

# 引擎源码（简化版 — 仅包含平台层 + 核心依赖）
# 完整集成时应引用引擎主 CMakeLists.txt 的 dse_engine 目标
file(GLOB_RECURSE DSE_HARMONY_SOURCES
    "${DSE_ROOT}/engine/platform/harmony/*.cpp"
    "${DSE_ROOT}/engine/platform/shared/*.cpp"
    "${DSE_ROOT}/engine/assets/harmony_rawfile_fs.cpp"
)

# 注意：完整集成需要引入引擎全部源码。
# 此处仅列出鸿蒙平台层文件作为模板参考。
# 实际项目应使用 add_subdirectory(${DSE_ROOT}) 或将 dse_engine 作为预编译库链接。

add_library(dse_napi SHARED ${DSE_HARMONY_SOURCES})

target_include_directories(dse_napi PRIVATE
    ${DSE_ROOT}
    ${DSE_ROOT}/engine
    ${DSE_ROOT}/depends/spdlog/include
    ${DSE_ROOT}/depends/glm
)

target_compile_definitions(dse_napi PRIVATE
    DSE_ENABLE_HARMONY_PLATFORM
    __OHOS__
)

# OHOS NDK 链接库
target_link_libraries(dse_napi PRIVATE
    ace_napi          # NAPI 运行时
    ace_ndk.z         # XComponent NDK
    native_window     # OHNativeWindow
    hilog_ndk.z       # 日志 (OH_LOG_Print)
    EGL               # EGL
    GLESv3            # OpenGL ES 3
    rawfile.z         # rawfile 资产访问
)

# Vulkan 支持（可选 — 旗舰设备）
option(DSE_ENABLE_VULKAN "Enable Vulkan RHI on OHOS" OFF)
if(DSE_ENABLE_VULKAN)
    target_compile_definitions(dse_napi PRIVATE DSE_ENABLE_VULKAN)
    target_link_libraries(dse_napi PRIVATE vulkan)
endif()
