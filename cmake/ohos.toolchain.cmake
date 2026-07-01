# HarmonyOS / OpenHarmony 交叉编译工具链
#
# 用法:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/ohos.toolchain.cmake \
#         -DOHOS_ARCH=arm64-v8a ..
#
# 前置条件:
#   - 设置环境变量 OHOS_SDK 指向 HarmonyOS SDK 安装目录
#   - SDK 版本: OpenHarmony 5.0+ (API 12+)

cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME OHOS)
set(CMAKE_SYSTEM_VERSION 1)

# ─── 架构 ──────────────────────────────────────────────────────

if(NOT DEFINED OHOS_ARCH)
    set(OHOS_ARCH "arm64-v8a")
endif()

if(OHOS_ARCH STREQUAL "arm64-v8a")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(OHOS_TARGET "aarch64-linux-ohos")
elseif(OHOS_ARCH STREQUAL "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
    set(OHOS_TARGET "x86_64-linux-ohos")
else()
    message(FATAL_ERROR "Unsupported OHOS_ARCH: ${OHOS_ARCH}. "
                        "Supported: arm64-v8a, x86_64")
endif()

# ─── SDK 路径 ──────────────────────────────────────────────────

if(NOT DEFINED ENV{OHOS_SDK})
    message(FATAL_ERROR "OHOS_SDK environment variable not set. "
        "Please set it to your HarmonyOS SDK path, e.g.:\n"
        "  export OHOS_SDK=/path/to/ohos-sdk")
endif()

set(OHOS_SDK $ENV{OHOS_SDK})
set(OHOS_NATIVE "${OHOS_SDK}/native")

if(NOT EXISTS "${OHOS_NATIVE}")
    message(FATAL_ERROR "OHOS native SDK not found at: ${OHOS_NATIVE}")
endif()

# ─── 编译器 ───────────────────────────────────────────────────

set(CMAKE_C_COMPILER "${OHOS_NATIVE}/llvm/bin/clang")
set(CMAKE_CXX_COMPILER "${OHOS_NATIVE}/llvm/bin/clang++")
set(CMAKE_AR "${OHOS_NATIVE}/llvm/bin/llvm-ar")
set(CMAKE_RANLIB "${OHOS_NATIVE}/llvm/bin/llvm-ranlib")
set(CMAKE_STRIP "${OHOS_NATIVE}/llvm/bin/llvm-strip")

set(CMAKE_SYSROOT "${OHOS_NATIVE}/sysroot")

# ─── 编译参数 ─────────────────────────────────────────────────

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=${OHOS_TARGET}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=${OHOS_TARGET} -std=c++20" CACHE STRING "" FORCE)

# 头文件搜索路径
list(APPEND CMAKE_FIND_ROOT_PATH "${OHOS_NATIVE}/sysroot")

include_directories(SYSTEM
    "${OHOS_NATIVE}/sysroot/usr/include"
    "${OHOS_NATIVE}/sysroot/usr/include/${OHOS_TARGET}"
)

# ─── 平台定义 ─────────────────────────────────────────────────

# 确保 __OHOS__ 宏可用（部分 NDK 版本已内置）
add_definitions(-D__OHOS__)

# 启用鸿蒙平台支持
set(DSE_ENABLE_HARMONY_PLATFORM ON CACHE BOOL "" FORCE)

# 交叉编译搜索策略
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
