cmake_minimum_required(VERSION 3.17)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()
project(DSEngine VERSION 0.1.0 LANGUAGES C CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 预发布标签（SemVer pre-release）。首个 SDK 测试版为 v0.1.0-alpha。
# 正式发布时置空字符串即可（DSE_VERSION_STRING 退化为纯 "x.y.z"）。
# package_sdk.ps1 会解析此行用于命名发行包，保持格式为：set(DSEngine_VERSION_PRERELEASE "...")
set(DSEngine_VERSION_PRERELEASE "alpha")
if(DSEngine_VERSION_PRERELEASE STREQUAL "")
    set(DSEngine_VERSION_STRING "${PROJECT_VERSION}")
else()
    set(DSEngine_VERSION_STRING "${PROJECT_VERSION}-${DSEngine_VERSION_PRERELEASE}")
endif()

# 生成版本头文件 dse_version.h（从 .in 模板）
configure_file(
    ${CMAKE_SOURCE_DIR}/engine/dse_version.h.in
    ${CMAKE_BINARY_DIR}/generated/engine/dse_version.h
    @ONLY
)
# 让源码编译时能 #include "engine/dse_version.h"
include_directories(${CMAKE_BINARY_DIR}/generated)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D GLM_FORCE_SWIZZLE)#GLM开启重排，随意转换。 vec2 v2 = v4.xz();
add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_definitions(-D USE_FMOD_STUDIO)#是否使用fmod studio，加载bank文件的形式。

# 注意：当前工程会把 vcpkg 的整个 lib 目录以通配方式拼进 MSVC 链接命令，
# 其中如果存在 lua.lib，就会与源码内置 Lua 重复链接。
# 这里显式忽略 Lua 默认库名，作为针对该项目链接命令的最小修复。
if(MSVC)
    add_link_options(/NODEFAULTLIB:lua.lib /NODEFAULTLIB:lua5.1.lib /NODEFAULTLIB:lua51.lib /NODEFAULTLIB:lua53.lib /NODEFAULTLIB:lua54.lib)
endif()

option(DSE_ENABLE_PHYSX "Enable PhysX integration" OFF)
option(DSE_ENABLE_JOLT "Enable Jolt Physics integration" ON)
if(DSE_ENABLE_PHYSX AND DSE_ENABLE_JOLT)
    message(FATAL_ERROR "DSE_ENABLE_PHYSX and DSE_ENABLE_JOLT are mutually exclusive. Please enable only one 3D physics backend.")
endif()
option(DSE_ENABLE_2D "Enable 2D runtime path" ON)
option(DSE_ENABLE_3D "Enable 3D runtime path" ON)
option(DSE_ENABLE_SPINE "Enable Spine runtime path" ON)
if(ANDROID)
    option(DSE_ENABLE_VULKAN "Enable Vulkan RHI backend" OFF)
else()
    option(DSE_ENABLE_VULKAN "Enable Vulkan RHI backend" ON)
endif()
if(WIN32)
    option(DSE_ENABLE_D3D11 "Enable Direct3D 11 backend" ON)
else()
    option(DSE_ENABLE_D3D11 "Enable Direct3D 11 backend" OFF)
endif()
option(DSE_BUILD_EDITOR "Build editor via npm/node-gyp" OFF)
option(DSE_BUILD_LAUNCHER "Build launcher target" OFF)
option(DSE_BUILD_GTESTS "Build GoogleTest based test targets" ON)
option(DSE_ENABLE_PROFILER "Include EasyProfiler instrumentation" OFF)
option(DSE_ENABLE_LUA "Enable Lua scripting runtime" ON)
option(DSE_ENABLE_NAVMESH "Enable NavMesh/pathfinding (Recast/Detour)" ON)
option(DSE_ENABLE_NET "Enable networking layer (GameNetworkingSockets backend)" OFF)
option(DSE_ENABLE_HTTP "Enable async HTTP(S) client (IXWebSocket + OpenSSL)" OFF)
option(DSE_ENABLE_PCH "Enable precompiled headers for faster compilation" ON)
option(DSE_ENABLE_UNITY_BUILD "Enable CMake unity (jumbo) build for faster compilation" OFF)

# --- Android NDK 交叉编译默认值 ---
if(ANDROID)
    set(DSE_ENABLE_PHYSX OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
    set(DSE_ENABLE_PROFILER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    set(DSE_BUILD_GTESTS OFF CACHE BOOL "" FORCE)
    add_definitions(-D __ANDROID__)
    message(STATUS "Android NDK build: PhysX/D3D11/EasyProfiler/GTest disabled")
endif()


# --- in-tree 依赖 submodule 完整性检查 ---
# 本工程所有第三方依赖以 git submodule 形式内置于 depends/，克隆后必须先执行：
#     git submodule update --init --recursive
# 这里用代表性 sentinel 文件检测；缺失时给出明确指引，而非晦涩的下游编译/配置报错。
set(_DSE_SUBMODULE_SENTINELS
    "depends/glm/glm/glm.hpp"
    "depends/imgui/imgui.h"
    "depends/assimp/CMakeLists.txt"
    "depends/glfw-3.3-3.4/CMakeLists.txt"
    "depends/JoltPhysics/Build/CMakeLists.txt"
    "depends/box2d-2.4.1/CMakeLists.txt"
    "depends/spdlog/CMakeLists.txt"
    "depends/entt-3.13.0/CMakeLists.txt"
)
if(DSE_ENABLE_VULKAN)
    list(APPEND _DSE_SUBMODULE_SENTINELS
        "depends/Vulkan-Headers/include/vulkan/vulkan.h"
        "depends/glslang/CMakeLists.txt"
        "depends/spirv-cross/CMakeLists.txt")
endif()
set(_DSE_MISSING_SUBMODULES "")
foreach(_dse_s IN LISTS _DSE_SUBMODULE_SENTINELS)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${_dse_s}")
        list(APPEND _DSE_MISSING_SUBMODULES "${_dse_s}")
    endif()
endforeach()
if(_DSE_MISSING_SUBMODULES)
    string(REPLACE ";" "\n    - " _DSE_MISSING_PRETTY "${_DSE_MISSING_SUBMODULES}")
    message(FATAL_ERROR
        "检测到依赖 submodule 缺失（depends/ 下内置第三方库未初始化）：\n"
        "    - ${_DSE_MISSING_PRETTY}\n"
        "\n"
        "本工程所有依赖以 git submodule 内置于 depends/。请在仓库根目录执行：\n"
        "    git submodule update --init --recursive\n"
        "\n"
        "完成后重新配置（VS：删除 build/ 目录后重新生成；命令行：重跑 cmake --preset）。")
endif()

if(DSE_ENABLE_NAVMESH)
    add_definitions(-D DSE_ENABLE_NAVMESH)
endif()
if(DSE_ENABLE_2D)
    add_definitions(-D DSE_ENABLE_2D)
endif()
if(DSE_ENABLE_3D)
    add_definitions(-D DSE_ENABLE_3D)
endif()

# Vulkan 源码集成
if(DSE_ENABLE_VULKAN)
    include(cmake/CMakeLists.txt.vulkan)
endif()

# glslang 源码集成（shader 离线编译工具链）
include(cmake/CMakeLists.txt.glslang)

# SPIRV-Cross 源码集成（shader 交叉编译工具）
include(cmake/CMakeLists.txt.spirv_cross)

# 注意：DSE_ENABLE_PHYSX 宏定义移至 Physx cmake include 之后，
# 因为 DSE_HAS_PHYSX_LIBS 在 cmake/CMakeLists.txt.Physx 中才被设置。

# 设置输出目录为 bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

# 本项目主要依赖仓库内置 depends 源码，不希望 MSVC 的 vcpkg 自动把
# installed/<triplet>/lib/*.lib 注入到每个 target 的链接命令中。
# 注意：vcpkg 的 MSBuild 集成要求使用字符串 "false" 才会命中其条件判断。
set(VcpkgAutoLink "false" CACHE STRING "Disable MSBuild vcpkg auto-link wildcard injection" FORCE)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library" FORCE)
    set(CMAKE_VS_GLOBALS
        "VcpkgApplocalDeps=false"
        "VcpkgAutoLink=false"
    )
    add_compile_options(/bigobj)
    add_compile_options(/MP)   # 多线程编译：始终启用，不依赖构建脚本传参
    add_compile_options(/wd4819)#忽略保存为unicode警告
    add_compile_options(/FS) # 修复多线程编译 PDB 写入冲突 C1041
    add_compile_options("$<$<CONFIG:Debug>:/Z7>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Z7>")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    add_compile_definitions(_CRT_STDIO_ISO_WIDE_SPECIFIERS=1)
    add_compile_definitions(_CRT_NONSTDC_NO_WARNINGS=1)
    add_compile_definitions(_CRT_DECLARE_NONSTDC_NAMES=1)
endif ()
if(NOT ANDROID)
    include(cmake/CMakeLists.txt.glfw)
else()
    set(glfw_sources "")
endif()
include(cmake/CMakeLists.txt.FreeType)
if(DSE_ENABLE_PROFILER)
    include(cmake/CMakeLists.txt.easy_profile_core)
else()
    set(easy_profiler_core_source "")
    add_definitions(-D EASY_PROFILER_DISABLE)
endif()
if(DSE_ENABLE_LUA)
    include(cmake/CMakeLists.txt.lua_script)
    add_definitions(-D DSE_ENABLE_LUA)
else()
    set(lua_src "")
endif()
if(DSE_ENABLE_PHYSX)
    include(cmake/CMakeLists.txt.Physx)
endif()

# DSE_ENABLE_PHYSX 宏需在 Physx cmake include 之后定义，
# 因为 DSE_HAS_PHYSX_LIBS 在 cmake/CMakeLists.txt.Physx 中才被设置
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_definitions(-D DSE_ENABLE_PHYSX)
endif()

# Jolt Physics (跨平台替代 PhysX)
include(cmake/CMakeLists.txt.Jolt)
if(DSE_ENABLE_JOLT AND DSE_HAS_JOLT)
    add_definitions(-D DSE_ENABLE_JOLT)
endif()

# 网络层 (GameNetworkingSockets)。默认 OFF：OFF 路径完全不引入网络依赖，
# 现有三端构建零影响。ON 时由 cmake/CMakeLists.txt.gns 负责按平台分流加密后端
# (桌面 libsodium / 移动 OpenSSL) 并接入 protobuf + GNS 静态库。
if(DSE_ENABLE_NET)
    include(cmake/CMakeLists.txt.gns)
    if(DSE_HAS_NET)
        add_definitions(-D DSE_NET_ENABLED=1)
    endif()
endif()

# 通用异步 HTTP(S) 客户端 (IXWebSocket + OpenSSL)。默认 OFF：OFF 路径完全不引入
# IXWebSocket/OpenSSL 依赖，现有构建零影响。ON 时由 cmake/CMakeLists.txt.http
# 编出 dse_http 静态库并定义 DSE_ENABLE_HTTP（供 engine/http 与 Lua 绑定启用）。
if(DSE_ENABLE_HTTP)
    include(cmake/CMakeLists.txt.http)
    if(DSE_HAS_HTTP)
        add_definitions(-D DSE_ENABLE_HTTP=1)
    endif()
endif()

#头文件目录（全局，合并为一处避免重复）
include_directories(
    "${CMAKE_SOURCE_DIR}"
    "engine"
    "modules"
    "samples"
    "depends"
    "depends/spdlog/include"
    "depends/glm"
    "depends/glm_ext"
    "depends/rapidjson/include"
    "depends/eventpp/include"
    "depends/tinygltf"
)
set(DSE_HAS_ASSIMP OFF)
if(EXISTS "${CMAKE_SOURCE_DIR}/depends/assimp/CMakeLists.txt")
    include_directories("depends/assimp/include")
    set(DSE_HAS_ASSIMP ON)
endif()

# Use local depends instead of FetchContent
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENTT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/entt-3.13.0)

set(DSE_PREV_CMAKE_WARN_DEPRECATED ${CMAKE_WARN_DEPRECATED})
set(DSE_PREV_CMAKE_SUPPRESS_DEVELOPER_WARNINGS ${CMAKE_SUPPRESS_DEVELOPER_WARNINGS})
set(CMAKE_WARN_DEPRECATED OFF)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1)

set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/box2d-2.4.1)
# 强制 box2d 使用 DLL CRT，与 dse_engine 的 /MDd 一致
if(MSVC)
    set_property(TARGET box2d PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Recast & Detour 静态库（仅在 NavMesh 启用时编译）
if(DSE_ENABLE_NAVMESH)
    include(cmake/CMakeLists.txt.recastnavigation)
endif()
if(DSE_HAS_ASSIMP)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "Disable Assimp install/export for local dependency build" FORCE)
    set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "Disable assimp PDB install to avoid missing .pdb install failure" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
        ${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES}
        ${CMAKE_SOURCE_DIR}/depends/lua
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/depends/assimp/generated")
    add_subdirectory(depends/assimp)
    # 强制 assimp 使用 DLL CRT，与 dse_engine 的 /MDd 一致
    if(MSVC)
        set_property(TARGET assimp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()


set(CMAKE_WARN_DEPRECATED ${DSE_PREV_CMAKE_WARN_DEPRECATED})
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ${DSE_PREV_CMAKE_SUPPRESS_DEVELOPER_WARNINGS})

# 仅追加上方未覆盖的第三方库路径
include_directories(
    ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
    ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include
    ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/include
    ${CMAKE_SOURCE_DIR}/depends/tiny-AES-c
    ${CMAKE_SOURCE_DIR}/depends/imgui
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends
)
set(BOX2D_ROOT "${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1")

file(GLOB_RECURSE engine_cpp CONFIGURE_DEPENDS
    "engine/*.cpp"
    "modules/gameplay_2d/*.cpp"
    "modules/runtime_bridge/*.cpp"
)

list(FILTER engine_cpp EXCLUDE REGEX ".*engine/assets/compiler/.*\\.cpp$")

# 网络层(engine/net/**)由 cmake/CMakeLists.txt.gns 编译为独立的 dse_net 目标
# （依赖 GNS 头/库），不并入 dse_engine 主目标，且 OFF 时完全不编译。
list(FILTER engine_cpp EXCLUDE REGEX ".*engine/net/.*\\.cpp$")

# HTTP 客户端(engine/http/**)由 cmake/CMakeLists.txt.http 编译为独立的 dse_http
# 目标（唯一 include IXWebSocket 之处），不并入 dse_engine 主目标。
list(FILTER engine_cpp EXCLUDE REGEX ".*engine/http/.*\\.cpp$")

# 排除 codegen 生成文件（*.gen.cpp）— 生成文件默认作为参考暂存
# 注意：dse_api_<prefix>.gen.cpp 与手写 dse_api.cpp 同 TU 域，须显式加回（见下）
list(FILTER engine_cpp EXCLUDE REGEX ".*\\.gen\\.cpp$")

# 显式加回已完成迁移的 gen 文件（手写版本已删除或已精简，无符号冲突）
list(APPEND engine_cpp
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_transform.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_camera3d.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_mesh_renderer.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_dir_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_point_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_spot_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_sky_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_tree.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_terrain_tile.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_dyn_obstacle.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_navmesh_rebake.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_post_process.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_animator3d.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_transform.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_camera3d.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_dir_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_point_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_mesh_renderer.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_spot_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_sky_light.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_tree.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_terrain_tile.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_dyn_obstacle.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_navmesh_rebake.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_post_process.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_ecs_animator3d.gen.cpp"
)

# Vulkan RHI 后端源文件仅在启用时编译
if(NOT DSE_ENABLE_VULKAN)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/render/rhi/vulkan/.*\\.cpp$")
endif()

# Android: 排除 GLFW platform 实现（使用 AndroidApp 替代）
if(ANDROID)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/glfw/.*\\.cpp$")
endif()

# 非 Android: 排除 Android 专用实现（防止 NDK 头文件缺失导致编译失败）
if(NOT ANDROID)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/android/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/assets/android_asset_fs\\.cpp$")
endif()

# D3D11 RHI 后端源文件仅在启用时编译
if(NOT DSE_ENABLE_D3D11)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/render/rhi/dx11/.*\\.cpp$")
endif()

# Lua 脚本运行时仅在启用时编译
if(NOT DSE_ENABLE_LUA)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/scripting/lua/.*\\.cpp$")
endif()


if(NOT DSE_ENABLE_SPINE)
    list(FILTER engine_cpp EXCLUDE REGEX ".*modules/gameplay_2d/spine/.*\\.cpp$")
endif()

# PhysX 排除逻辑移至 gameplay_3d append 之后（见下方）

if(NOT DSE_ENABLE_3D)
    list(APPEND engine_cpp
        "modules/gameplay_3d/rendering/mesh_render_system.cpp"
        "modules/gameplay_3d/animation/animator_system.cpp"
        "modules/gameplay_3d/animation/anim_layer_blend_system.cpp"
        "modules/gameplay_3d/animation/ik_solver_system.cpp"
        "modules/gameplay_3d/animation/foot_ik_system.cpp"
        "modules/gameplay_3d/bone_attachment_system.cpp"
        "modules/gameplay_3d/particles/particle3d_system.cpp"
        "modules/gameplay_3d/ai/steering_system.cpp"
    )
endif()

if(DSE_ENABLE_3D)
    # Gameplay3D 全部编入 dse_engine，避免跨 DLL EnTT 模板实例化导致的内存损坏。
    # 不再构建 DSE_Gameplay3D.dll。
    file(GLOB_RECURSE gameplay_3d_cpp CONFIGURE_DEPENDS "modules/gameplay_3d/*.cpp")
    list(APPEND engine_cpp ${gameplay_3d_cpp})
endif()

# 物理后端源文件排除逻辑（必须在 gameplay_3d append 之后执行）
set(_HAS_ANY_PHYSICS3D OFF)
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    set(_HAS_ANY_PHYSICS3D ON)
    # PhysX 模式下排除 Jolt 实现
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d_system_jolt\\.cpp$")
elseif(DSE_ENABLE_JOLT AND DSE_HAS_JOLT)
    set(_HAS_ANY_PHYSICS3D ON)
    # Jolt 模式下排除 PhysX 专属实现文件
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d/physics3d_system\\.cpp$")
    # Vehicle / Buoyancy / Fracture / Ragdoll 均已适配 Jolt 后端
endif()
if(NOT _HAS_ANY_PHYSICS3D)
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/vehicle/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/buoyancy/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/ragdoll/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/fracture/.*\\.cpp$")
endif()

# NavMesh 禁用时排除 navigation 源文件
if(NOT DSE_ENABLE_NAVMESH)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/navigation/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*modules/gameplay_3d/ai/nav_.*\\.cpp$")
endif()

file(GLOB_RECURSE spine_cpp "${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/*.cpp")
set(tiny_aes_c
    "${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/aes.c"
)
file(GLOB_RECURSE bundle_cpp "${CMAKE_SOURCE_DIR}/depends/bundle/*.cpp")

# bundle 是第三方打包/压缩库（含 miniz/lzma 等），使用了 C++17 已移除的 register 存储类
# 与窄化初始化；clang(Android NDK) 默认将其视为错误。仅对该第三方源关闭对应诊断，不改其源码。
if(NOT MSVC)
    set_source_files_properties(${bundle_cpp} PROPERTIES
        COMPILE_OPTIONS "-Wno-register;-Wno-c++11-narrowing")
endif()
# Android(arm64 等非 x86)：bundle 的 MCM/TANGELO 高比率编解码器内联了 x86 SSE/MMX intrinsics
# （emmintrin/mmintrin），无法在 ARM 上编译。DSEngine 仅用 ZIP(miniz/DEFLATE) 路径，这些编解码器
# 从未被使用，按 bundle 自带的库配置开关排除即可（不改第三方源码、不影响打包/解包功能）。
if(ANDROID)
    set_source_files_properties(${bundle_cpp} PROPERTIES
        COMPILE_DEFINITIONS "BUNDLE_NO_MCM;BUNDLE_NO_TANGELO")
endif()


set(imgui_src
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_opengl3.cpp"
)
if(NOT ANDROID)
    list(APPEND imgui_src "${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_glfw.cpp")
endif()

option(DSE_BUILD_SHARED "Build dse_engine as shared library (DLL). Requires DSE_EXPORT on public APIs." OFF)
set(DSE_ENGINE_ALL_SOURCES
    ${glfw_sources}
    ${freetype_src}
    ${easy_profiler_core_source}
    ${lua_src}
    ${spine_cpp}
    ${tiny_aes_c}
    ${bundle_cpp}
    ${imgui_src}
    ${engine_cpp}
)
if(DSE_BUILD_SHARED)
    add_library(dse_engine SHARED ${DSE_ENGINE_ALL_SOURCES})
else()
    add_library(dse_engine STATIC ${DSE_ENGINE_ALL_SOURCES})
endif()
target_link_libraries(dse_engine PRIVATE box2d)
# 崩溃报告子系统（engine/diagnostics/crash_handler.cpp）：Windows 上需 DbgHelp
# 提供 MiniDumpWriteDump / StackWalk64 / Sym* 符号化。
if(WIN32)
    target_link_libraries(dse_engine PRIVATE Dbghelp)
endif()
if(DSE_HAS_ASSIMP)
    target_link_libraries(dse_engine PRIVATE assimp)
endif()
if(DSE_ENABLE_NAVMESH)
    target_link_libraries(dse_engine PRIVATE recast detour detour_tilecache)
endif()
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    target_include_directories(dse_engine PRIVATE ${PHYSX_INCLUDE_DIR} ${PXSHARED_INCLUDE_DIR})
    target_link_directories(dse_engine PRIVATE
        $<$<CONFIG:Debug>:${PHYSX_DLL_DIR_DEBUG}>
        $<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLL_DIR_RELEASE}>
    )
    # PHYSX_LIBS 由 cmake/CMakeLists.txt.Physx 计算，已包含 release→debug import lib 回退
    target_link_libraries(dse_engine PRIVATE
        ${PHYSX_LIBS}
        $<$<CONFIG:Debug>:${PHYSX_LIB_DIR_DEBUG}/PhysXExtensions_static_64.lib>
        $<$<NOT:$<CONFIG:Debug>>:${PHYSX_LIB_DIR_RELEASE}/PhysXExtensions_static_64.lib>
    )
endif()
if(DSE_ENABLE_JOLT AND DSE_HAS_JOLT)
    target_link_libraries(dse_engine PRIVATE Jolt)
endif()
# --- 异步 HTTP 客户端 ---
if(DSE_ENABLE_HTTP AND DSE_HAS_HTTP)
    target_link_libraries(dse_engine PRIVATE dse_http)
    # 无头验证 Lua 绑定 dse.http（裸 lua_State + 本地回环服务器），不依赖窗口/RHI。
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/http/http_lua_smoke.cpp")
        add_executable(dse_http_lua_smoke "${CMAKE_SOURCE_DIR}/tests/http/http_lua_smoke.cpp")
        target_link_libraries(dse_http_lua_smoke PRIVATE dse_engine dse_http)
        target_include_directories(dse_http_lua_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/depends/IXWebSocket"
            "${CMAKE_SOURCE_DIR}/depends/lua")
        target_compile_features(dse_http_lua_smoke PRIVATE cxx_std_17)
        if(MSVC)
            set_property(TARGET dse_http_lua_smoke PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            target_compile_options(dse_http_lua_smoke PRIVATE /utf-8)
        endif()
    endif()
endif()
# --- 游戏网络传输 (GameNetworkingSockets) Lua 绑定 ---
if(DSE_ENABLE_NET AND DSE_HAS_NET)
    # dse.net 绑定(engine/scripting/.../lua_binding_net.cpp)随 dse_engine 编译，
    # 调用 dse_net C ABI；故把 dse_net 链进 dse_engine（静态库依赖透传给可执行）。
    target_link_libraries(dse_engine PRIVATE dse_net)
    # 无头验证 Lua 绑定 dse.net（裸 lua_State + 本地 UDP 回环），不依赖窗口/RHI。
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/net/net_lua_smoke.cpp")
        add_executable(dse_net_lua_smoke "${CMAKE_SOURCE_DIR}/tests/net/net_lua_smoke.cpp")
        target_link_libraries(dse_net_lua_smoke PRIVATE dse_engine dse_net)
        target_include_directories(dse_net_lua_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_SOURCE_DIR}/depends/lua")
        target_compile_features(dse_net_lua_smoke PRIVATE cxx_std_17)
        if(MSVC)
            set_property(TARGET dse_net_lua_smoke PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            target_compile_options(dse_net_lua_smoke PRIVATE /utf-8)
        endif()
    endif()
endif()
# --- 自描述二进制序列化 (dse.serialize) Lua 绑定 ---
# 绑定 engine/scripting/.../lua_binding_serialize.cpp 随 dse_engine 编译（无外部依赖、
# 不受 NET/HTTP 开关影响）。此处仅在启用 Lua 时构建其无头往返 smoke。
if(DSE_ENABLE_LUA AND EXISTS "${CMAKE_SOURCE_DIR}/tests/serialize/serialize_smoke.cpp")
    add_executable(dse_serialize_smoke "${CMAKE_SOURCE_DIR}/tests/serialize/serialize_smoke.cpp")
    target_link_libraries(dse_serialize_smoke PRIVATE dse_engine)
    target_include_directories(dse_serialize_smoke PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/depends/lua")
    target_compile_features(dse_serialize_smoke PRIVATE cxx_std_17)
    if(MSVC)
        set_property(TARGET dse_serialize_smoke PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(dse_serialize_smoke PRIVATE /utf-8)
    endif()
endif()
# --- D3D11 RHI 后端 ---
if(DSE_ENABLE_D3D11 AND WIN32)
    add_definitions(-D DSE_ENABLE_D3D11)
    target_link_libraries(dse_engine PRIVATE d3d11 dxgi d3dcompiler dxguid)
elseif(DSE_ENABLE_D3D11 AND NOT WIN32)
    message(WARNING "DSE_ENABLE_D3D11=ON but platform is not Windows. D3D11 backend will be disabled.")
    set(DSE_ENABLE_D3D11 OFF)
endif()
# --- Vulkan RHI 后端 ---
if(DSE_ENABLE_VULKAN AND DSE_HAS_VULKAN)
    add_definitions(-D DSE_ENABLE_VULKAN)

    # Vulkan 头文件 + Loader 链接（源码构建或系统 SDK）
    if(DSE_VULKAN_HEADERS_FROM_SOURCE)
        target_link_libraries(dse_engine PRIVATE Vulkan::Headers)
    endif()
    if(DSE_VULKAN_LOADER_FROM_SOURCE)
        target_link_libraries(dse_engine PRIVATE Vulkan::Loader)
    elseif(DSE_VULKAN_FROM_SYSTEM)
        target_include_directories(dse_engine PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(dse_engine PRIVATE Vulkan::Vulkan)
    endif()

    # glslang 运行时编译库（GLSL→SPIR-V）
    if(DSE_HAS_GLSLANG)
        add_definitions(-D DSE_HAS_GLSLANG)
        target_include_directories(dse_engine PRIVATE
            ${DSE_GLSLANG_INCLUDE_DIR}
            ${DSE_GLSLANG_INCLUDE_DIR}/glslang/Include
            ${DSE_GLSLANG_INCLUDE_DIR}/SPIRV
        )
        target_link_libraries(dse_engine PRIVATE glslang SPIRV MachineIndependent GenericCodeGen glslang-default-resource-limits)
    else()
        message(WARNING "DSE_ENABLE_VULKAN=ON but glslang not available. "
                        "Vulkan shader compilation will be disabled at runtime.")
    endif()
    # SPIRV-Cross 运行时反射（GPU-driven shader 自动发现 descriptor bindings）
    # 注意：不链接 spirv-cross 库以避免 WINDOWS_EXPORT_ALL_SYMBOLS 符号数超限（LNK1189）。
    # 改为在 spirv_cross_embedded.cpp 中直接 include 需要的 spirv-cross 源文件。
    if(DSE_HAS_SPIRV_CROSS)
        target_compile_definitions(dse_engine PRIVATE DSE_HAS_SPIRV_CROSS)
        target_include_directories(dse_engine PRIVATE ${DSE_SPIRV_CROSS_INCLUDE_DIR})
    endif()
elseif(DSE_ENABLE_VULKAN AND NOT DSE_HAS_VULKAN)
    message(WARNING "DSE_ENABLE_VULKAN=ON but Vulkan headers/loader not available. "
                    "Vulkan RHI backend will be disabled.")
    set(DSE_ENABLE_VULKAN OFF)
endif()
set_target_properties(dse_engine PROPERTIES OUTPUT_NAME "DSEngine")
if(DSE_BUILD_SHARED)
    target_compile_definitions(dse_engine PRIVATE DSE_ENGINE_EXPORTS)
else()
    target_compile_definitions(dse_engine PUBLIC DSE_BUILD_STATIC)
endif()

if(MSVC)
    target_compile_options(dse_engine PRIVATE /utf-8 /wd5033 /wd4834)
endif()

target_include_directories(dse_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/render/shaders/generated
)
if(NOT ANDROID)
    target_include_directories(dse_engine PRIVATE ${CMAKE_SOURCE_DIR}/depends/glad/include)
endif()

# --- Android 链接库 ---
if(ANDROID)
    target_link_libraries(dse_engine PRIVATE EGL GLESv3 android log)
endif()

# --- Linux 桌面链接库 (X11 窗口 + GL + dl + pthread) ---
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Threads REQUIRED)
    target_link_libraries(dse_engine PRIVATE
        X11 Xrandr Xinerama Xcursor Xi
        GL ${CMAKE_DL_LIBS} Threads::Threads rt m)
endif()
if(DSE_ENABLE_LUA)
    target_include_directories(dse_engine PRIVATE
        ${CMAKE_SOURCE_DIR}/depends/lua
    )
endif()

if(EXISTS "${BOX2D_ROOT}/include")
    target_include_directories(dse_engine PRIVATE "${BOX2D_ROOT}/include")
endif()
if(EXISTS "${BOX2D_ROOT}/src")
    target_include_directories(dse_engine PRIVATE "${BOX2D_ROOT}/src")
endif()

function(dse_apply_config_postfix target_name)
    set_target_properties(${target_name} PROPERTIES
        DEBUG_POSTFIX "_debug"
        RELEASE_POSTFIX "_release"
        RELWITHDEBINFO_POSTFIX "_relwithdebinfo"
        MINSIZEREL_POSTFIX "_minsizerel"
    )
endfunction()

dse_apply_config_postfix(dse_engine)
# IMPORT_SUFFIX / PREFIX 由 dse_apply_config_postfix 的 DEBUG_POSTFIX 等属性自动处理，
# 不再硬编码为 _debug.lib，否则 Release 配置也会链接到 _debug.lib。

# --- 预编译头（PCH）：加速常用 STL / GLM / EnTT 头文件解析 ---
if(DSE_ENABLE_PCH)
    target_precompile_headers(dse_engine PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/engine/dse_pch.h>"
    )
endif()

# --- Unity Build（可选）：合并翻译单元减少编译器启动 / 头文件重复解析开销 ---
if(DSE_ENABLE_UNITY_BUILD)
    set_target_properties(dse_engine PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE 16
    )
endif()

if (MSVC)
    set_property(TARGET dse_engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # 注意：LTCG (/GL) 与 WINDOWS_EXPORT_ALL_SYMBOLS 不兼容。
    # cmake -E __create_def 无法读取 /GL 产出的 .obj 文件（无标准 COFF 符号表）。
    # 因此不启用 INTERPROCEDURAL_OPTIMIZATION，Release 依赖 /O2 + Unity Build 即可。

    # Workaround: CMake 4.3 VS generator 在非 Debug 配置下可能不生成 objects.txt，
    # 导致 WINDOWS_EXPORT_ALL_SYMBOLS 的 __create_def PreLinkEvent 失败。
    # 此自定义 PRE_LINK 命令在 __create_def 之前运行，确保 objects.txt 存在。
    add_custom_command(TARGET dse_engine PRE_LINK
        COMMAND ${CMAKE_COMMAND} -E remove -f
            ${CMAKE_BINARY_DIR}/dse_engine.dir/$<CONFIG>/exports.def
        COMMAND ${CMAKE_COMMAND}
            -DOBJ_DIR=${CMAKE_BINARY_DIR}/dse_engine.dir/$<CONFIG>
            -P ${CMAKE_SOURCE_DIR}/cmake/write_objects_txt.cmake
        COMMENT "Generating objects.txt for WINDOWS_EXPORT_ALL_SYMBOLS"
        VERBATIM
    )
else()
    add_definitions(-D _DEBUG)
endif ()

# DSE_Gameplay3D 已静态编入 dse_engine，无需独立 install 规则。

add_custom_target(dse_copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/samples
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/samples
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/script
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/script
    COMMENT "Copying runtime assets to bin/"
)
add_dependencies(dse_copy_assets dse_engine)

function(dse_copy_runtime_assets target_name)
    add_dependencies(${target_name} dse_copy_assets)
endfunction()

function(dse_copy_runtime_dlls target_name)
    add_dependencies(${target_name} dse_copy_assets)
endfunction()

# 包含运行时宿主入口
add_subdirectory(apps/runtime)

if(DSE_BUILD_EDITOR)
    add_subdirectory(apps/editor_cpp)
endif()

# Standalone game runtime (no editor UI)
add_subdirectory(apps/standalone)

# 工具链：AssetBuilder
add_executable(AssetBuilder 
    apps/tools/asset_builder/main.cpp
    engine/assets/compiler/importer.cpp
)
target_include_directories(AssetBuilder PRIVATE ${CMAKE_SOURCE_DIR} depends/tinygltf depends/glm)
if(DSE_HAS_ASSIMP)
    target_link_libraries(AssetBuilder PRIVATE assimp)
    target_compile_definitions(AssetBuilder PRIVATE DSE_HAS_ASSIMP=1)
endif()
if(MSVC)
    set_property(TARGET AssetBuilder PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# 工具链：ShaderCompiler (GLSL 450 → SPIR-V / GLSL 330 / HLSL)
if(DSE_HAS_GLSLANG AND DSE_HAS_SPIRV_CROSS)
    # 工具链构建延后到下方：交叉编译时改用 host 预构建的 dse_shader_compiler。

    # --- Shader 离线编译：构建时自动执行 ---
    set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/engine/render/shaders/src")
    set(SHADER_GEN_DIR "${CMAKE_SOURCE_DIR}/engine/render/shaders/generated")

    file(GLOB SHADER_SOURCE_FILES
        "${SHADER_SRC_DIR}/*.vert"
        "${SHADER_SRC_DIR}/*.frag"
        "${SHADER_SRC_DIR}/*.comp"
    )

    # Exclude hair shaders (hand-written gen.h due to SSBO, cannot cross-compile to GLSL 330)
    list(FILTER SHADER_SOURCE_FILES EXCLUDE REGEX "hair\\.(vert|frag)$")

    # 生成的 embed 头文件列表（用于依赖跟踪）
    set(GENERATED_SHADER_HEADERS "")
    foreach(_shader ${SHADER_SOURCE_FILES})
        get_filename_component(_name ${_shader} NAME_WE)
        get_filename_component(_ext ${_shader} LAST_EXT)
        string(SUBSTRING ${_ext} 1 -1 _ext_clean)
        list(APPEND GENERATED_SHADER_HEADERS "${SHADER_GEN_DIR}/embed/${_name}_${_ext_clean}.gen.h")
    endforeach()
    # @VARIANTS: GPU_DRIVEN 变体（pbr.vert → pbr_gpu_driven_vert.gen.h）
    list(APPEND GENERATED_SHADER_HEADERS
        "${SHADER_GEN_DIR}/embed/pbr_gpu_driven_vert.gen.h"
        "${SHADER_GEN_DIR}/embed/pbr_gpu_driven_vert_reflect.gen.h"
    )
    # compute skinning shader（skinning.comp → skinning_comp.gen.h）
    list(APPEND GENERATED_SHADER_HEADERS
        "${SHADER_GEN_DIR}/embed/skinning_comp.gen.h"
        "${SHADER_GEN_DIR}/embed/skinning_comp_reflect.gen.h"
    )
    # skinning.comp 已被上面的 glob 收集（skinning_comp.gen.h），显式追加会重复；
    # MSBuild 静默去重，Ninja 会因 dupbuild 报错——统一去重以跨生成器安全。
    list(REMOVE_DUPLICATES GENERATED_SHADER_HEADERS)

    # dse_shader_compiler 是 host 工具：
    #  - 原生构建：直接构建该 target 并使用；
    #  - 交叉编译(Android/iOS)：clang 产物无法在 host 运行，改用 host 预构建可执行文件
    #    （先在桌面平台构建一次生成 bin/dse_shader_compiler[.exe]，或 -DDSE_HOST_SHADER_COMPILER=<path>）。
    if(CMAKE_CROSSCOMPILING)
        if(NOT DSE_HOST_SHADER_COMPILER)
            find_program(DSE_HOST_SHADER_COMPILER
                NAMES dse_shader_compiler dse_shader_compiler.exe
                PATHS "${CMAKE_SOURCE_DIR}/bin" NO_DEFAULT_PATH)
        endif()
        if(NOT DSE_HOST_SHADER_COMPILER)
            message(FATAL_ERROR
                "交叉编译需要 host 版 dse_shader_compiler。请先在桌面平台(Windows/Linux)"
                "构建一次以生成 bin/dse_shader_compiler，或通过 -DDSE_HOST_SHADER_COMPILER=<path> 指定。")
        endif()
        set(_DSE_SHADER_COMPILER_CMD "${DSE_HOST_SHADER_COMPILER}")
        set(_DSE_SHADER_COMPILER_DEP "")
        message(STATUS "Cross-compiling: using host shader compiler ${DSE_HOST_SHADER_COMPILER}")
    else()
        add_subdirectory(tools/shader_compiler)
        set(_DSE_SHADER_COMPILER_CMD dse_shader_compiler)
        set(_DSE_SHADER_COMPILER_DEP dse_shader_compiler)
    endif()

    add_custom_command(
        OUTPUT ${GENERATED_SHADER_HEADERS}
        COMMAND ${_DSE_SHADER_COMPILER_CMD}
            --input-dir "${SHADER_SRC_DIR}"
            --output-dir "${SHADER_GEN_DIR}"
            --target all --embed
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/bin/data/shader_cache"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/data/shader_cache"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/examples/KF_Framework/data/shader_cache"
        DEPENDS ${_DSE_SHADER_COMPILER_DEP} ${SHADER_SOURCE_FILES}
        COMMENT "Compiling shaders (GLSL 450 -> SPIR-V / GLSL 430 / HLSL) and clearing runtime shader cache"
        VERBATIM
    )

    add_custom_target(dse_compile_shaders DEPENDS ${GENERATED_SHADER_HEADERS})
    add_dependencies(dse_engine dse_compile_shaders)
    target_include_directories(dse_engine PRIVATE "${SHADER_GEN_DIR}")
    # 将生成的 embed 头文件注册为 dse_engine 的源，使 MSVC 能跟踪
    # #include 依赖——gen.h 变更时自动重编译包含它的 .cpp
    set_source_files_properties(${GENERATED_SHADER_HEADERS} PROPERTIES
        GENERATED TRUE
        HEADER_FILE_ONLY TRUE)
    target_sources(dse_engine PRIVATE ${GENERATED_SHADER_HEADERS})
endif()

# 工具链：DSSL Compiler (.dssl → GLSL 450)
add_subdirectory(tools/dssl_compiler)

# ---------------------------------------------------------------------------
# PUBLIC interface include directories（供下游 target_link_libraries 自动继承）
# BUILD_INTERFACE  : 源码编译时使用的路径
# INSTALL_INTERFACE: SDK 安装后消费者使用的路径
# ---------------------------------------------------------------------------
target_include_directories(dse_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engine>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/modules>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/glm>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/glm_ext>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/engine>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/modules>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm_ext>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/entt/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/box2d/include>
)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------

# 1. 引擎目标（DLL/LIB）
if(DSE_BUILD_SHARED)
install(TARGETS dse_engine
    EXPORT DSEngineTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}  COMPONENT sdk
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}  COMPONENT sdk
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  COMPONENT sdk
)

# 2. 引擎公共头文件
#    排除 RHI 三个后端的具体实现头（dx11/ opengl/ vulkan/）——它们是内部实现，
#    消费者只通过抽象层 rhi_factory.h / rhi_device.h 使用，且经核实无任何对外头文件
#    引用这些后端头（grep: 0 引用），故安全地不纳入 SDK 公共头边界。
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/engine
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h"
    PATTERN "*.h.in" EXCLUDE
    REGEX "render/rhi/(dx11|opengl|vulkan)/" EXCLUDE
)
# 2b. CMake 生成的版本头文件
install(FILES ${CMAKE_BINARY_DIR}/generated/engine/dse_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/engine
    COMPONENT sdk
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/modules/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/modules
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h"
)

# 3. 第三方依赖公共头文件（仅公共 API 中被引用的）
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/glm/glm/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm/glm
    COMPONENT sdk
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/glm_ext/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/glm_ext
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl"
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/entt/src
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DSEngine/third_party/box2d/include
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.h"
)

# 4. Lua 脚本
install(DIRECTORY ${CMAKE_SOURCE_DIR}/script/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/DSEngine/script
    COMPONENT sdk
    FILES_MATCHING PATTERN "*.lua"
)

# 5. 导出目标文件
install(EXPORT DSEngineTargets
    FILE DSEngineTargets.cmake
    NAMESPACE DSEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
    COMPONENT sdk
)
endif() # DSE_BUILD_SHARED

# 6. 生成并安装 Package Config 文件
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/DSEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DSEngineConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
    COMPONENT sdk
)

enable_testing()

if(DSE_BUILD_GTESTS)
    set(DSE_LOCAL_GTEST_DIR "${CMAKE_SOURCE_DIR}/depends/googletest-1.17.0")
    if(NOT EXISTS "${DSE_LOCAL_GTEST_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "DSE_BUILD_GTESTS=ON 需要本地 GoogleTest 源码。请将 googletest-1.17.0.zip 解压到 ${DSE_LOCAL_GTEST_DIR}"
        )
    endif()

    # 禁止 googletest 自身占据全局 include / install，避免与引擎构建冲突
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    # dse_engine 是 DLL；GoogleTest 和测试 exe 使用 DLL CRT，避免 Debug CRT 堆链表跨模块不一致。
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory("${DSE_LOCAL_GTEST_DIR}" "${CMAKE_BINARY_DIR}/third_party/googletest")

    add_subdirectory(tests/gtest)
endif()


find_program(NPM_EXECUTABLE NAMES npm.cmd npm)
find_program(NPX_EXECUTABLE NAMES npx.cmd npx)
find_package(Python3 COMPONENTS Interpreter QUIET)

set(DSE_LEGACY_EDITOR_DIR "${CMAKE_SOURCE_DIR}/apps/editor")
set(DSE_LEGACY_EDITOR_HAS_NODE_PIPELINE OFF)
if(EXISTS "${DSE_LEGACY_EDITOR_DIR}/package.json" AND EXISTS "${DSE_LEGACY_EDITOR_DIR}/binding.gyp")
    set(DSE_LEGACY_EDITOR_HAS_NODE_PIPELINE ON)
endif()

if(DSE_BUILD_LAUNCHER)
    if(EXISTS "${CMAKE_SOURCE_DIR}/apps/launcher/CMakeLists.txt")
        add_subdirectory(apps/launcher)
    else()
        add_custom_target(dse_launcher ALL
            COMMAND ${CMAKE_COMMAND} -E echo "launcher_not_implemented"
        )
    endif()
endif()

if(NPM_EXECUTABLE AND NPX_EXECUTABLE AND DSE_LEGACY_EDITOR_HAS_NODE_PIPELINE)
    add_test(
        NAME editor_bridge_rebuild
        COMMAND ${NPX_EXECUTABLE} node-gyp rebuild
        WORKING_DIRECTORY ${DSE_LEGACY_EDITOR_DIR}
    )
    set_tests_properties(editor_bridge_rebuild PROPERTIES FIXTURES_SETUP editor_bridge_ready)

    add_test(
        NAME editor_material_replay_regression
        COMMAND ${NPM_EXECUTABLE} run regress:material-replay
        WORKING_DIRECTORY ${DSE_LEGACY_EDITOR_DIR}
    )
    set_tests_properties(editor_material_replay_regression PROPERTIES FIXTURES_REQUIRED editor_bridge_ready)

    add_test(
        NAME editor_scene_schema_migrate_regression
        COMMAND ${NPM_EXECUTABLE} run regress:scene-schema-migrate
        WORKING_DIRECTORY ${DSE_LEGACY_EDITOR_DIR}
    )
endif()

# ── S1.5 Codegen Target ─────────────────────────────────────────────────────
# 用法: cmake --build build_vs2022 --target dse_codegen
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_target(dse_codegen
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/codegen/codegen.py
                --defs  ${CMAKE_SOURCE_DIR}/tools/codegen/binding_defs.json
                --out   ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running DSEngine binding codegen..."
        VERBATIM
    )
endif()

# ── Android 原生宿主 (NativeActivity host) ──────────────────────────────────
# 产物 libdse_android_host.so：通过 native_app_glue 提供的 ANativeActivity_onCreate
# 作为入口，链接 dse_engine（静态库时自包含），供打包 APK 使用。
# 构建: cmake --build <android_build_dir> --target dse_android_host
if(ANDROID)
    set(_DSE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    if(NOT EXISTS "${_DSE_GLUE_DIR}/android_native_app_glue.c")
        set(_DSE_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
    endif()
    if(NOT EXISTS "${_DSE_GLUE_DIR}/android_native_app_glue.c")
        message(FATAL_ERROR "找不到 native_app_glue 源（${_DSE_GLUE_DIR}）")
    endif()
    add_library(dse_android_host SHARED
        "${CMAKE_SOURCE_DIR}/apps/android_host/native_main.cpp"
        "${_DSE_GLUE_DIR}/android_native_app_glue.c")
    target_include_directories(dse_android_host PRIVATE
        "${CMAKE_SOURCE_DIR}" "${_DSE_GLUE_DIR}")
    target_link_libraries(dse_android_host PRIVATE dse_engine EGL GLESv3 android log)
    # NativeActivity 经 dlsym 取入口；强制保留 glue 提供的 ANativeActivity_onCreate
    target_link_options(dse_android_host PRIVATE "-u" "ANativeActivity_onCreate")
    # 固定输出名(去掉 Debug 后缀)，使 APK manifest 的 android.app.lib_name 稳定为 dse_android_host
    set_target_properties(dse_android_host PROPERTIES OUTPUT_NAME "dse_android_host" DEBUG_POSTFIX "")
endif()
