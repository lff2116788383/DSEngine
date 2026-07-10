# ── Engine source collection ─────────────────────────────────────
# Extracted from root CMakeLists.txt for maintainability.
# Sets: engine_cpp, spine_cpp, tiny_aes_c, bundle_cpp, imgui_src
# Requires: all DSE_ENABLE_* options, DSE_HAS_* flags, BOX2D_ROOT

file(GLOB_RECURSE engine_cpp CONFIGURE_DEPENDS
    "engine/*.cpp"
    "modules/gameplay_2d/*.cpp"
    "modules/runtime_bridge/*.cpp"
)

list(FILTER engine_cpp EXCLUDE REGEX ".*engine/assets/compiler/.*\\.cpp$")

# 网络层(engine/net/**)由 cmake/CMakeLists.txt.gns 编译为独立的 dse_net 目标
list(FILTER engine_cpp EXCLUDE REGEX ".*engine/net/.*\\.cpp$")

# HTTP 客户端(engine/http/**)由 cmake/CMakeLists.txt.http 编译为独立的 dse_http 目标
list(FILTER engine_cpp EXCLUDE REGEX ".*engine/http/.*\\.cpp$")

# 排除 codegen 生成文件（*.gen.cpp）— 由下方 GLOB 统一加回
list(FILTER engine_cpp EXCLUDE REGEX ".*\\.gen\\.cpp$")

# 自动发现 codegen 生成的源文件（C ABI 实现 + Lua 绑定 + 反射）
file(GLOB DSE_GEN_CPP CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/engine/scripting/native_api/dse_api_*.gen.cpp"
    "${CMAKE_SOURCE_DIR}/engine/scripting/lua/bindings/lua_binding_*.gen.cpp"
)
list(APPEND engine_cpp ${DSE_GEN_CPP})

# 反射注册（由 codegen 生成，非 scripting 目录下）
list(APPEND engine_cpp "${CMAKE_SOURCE_DIR}/engine/reflect/component_reflection.gen.cpp")

# Vulkan RHI 后端源文件仅在启用时编译
if(NOT DSE_ENABLE_VULKAN)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/render/rhi/vulkan/.*\\.cpp$")
endif()

# Android / Web(Emscripten) / iOS / OHOS: 排除桌面 GLFW platform 实现
if(ANDROID OR EMSCRIPTEN OR (DSE_ENABLE_APPLE_PLATFORM AND IOS) OR (DSE_ENABLE_HARMONY_PLATFORM AND OHOS))
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/glfw/.*\\.cpp$")
endif()

# 非 Android: 排除 Android 专用实现
if(NOT ANDROID)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/android/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/assets/android_asset_fs\\.cpp$")
endif()

# 非 Emscripten: 排除 Web 专用实现
if(NOT EMSCRIPTEN)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/web/.*\\.cpp$")
endif()

# iOS 平台：.mm 文件仅在 iOS + DSE_ENABLE_APPLE_PLATFORM 时编译
if(DSE_ENABLE_APPLE_PLATFORM AND IOS)
    file(GLOB ios_platform_mm CONFIGURE_DEPENDS "engine/platform/ios/*.mm")
    list(APPEND engine_cpp ${ios_platform_mm})
endif()

# 非 OHOS: 排除 HarmonyOS 专用实现
if(NOT (DSE_ENABLE_HARMONY_PLATFORM AND OHOS))
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/harmony/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/assets/harmony_rawfile_fs\\.cpp$")
endif()

# 共享平台工具（egl_helper）：仅在 Android 或 OHOS 构建时编译
if(NOT ANDROID AND NOT (DSE_ENABLE_HARMONY_PLATFORM AND OHOS))
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/platform/shared/.*\\.cpp$")
endif()

# D3D11 RHI 后端源文件仅在启用时编译
if(NOT DSE_ENABLE_D3D11)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/render/rhi/dx11/.*\\.cpp$")
endif()

# WebGPU RHI 后端源文件仅在启用时编译
if(NOT DSE_ENABLE_WEBGPU)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/render/rhi/webgpu/.*\\.cpp$")
endif()

# Lua 脚本运行时仅在启用时编译
if(NOT DSE_ENABLE_LUA)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/scripting/lua/.*\\.cpp$")
endif()

# C# 脚本运行时仅在启用时编译
if(NOT DSE_ENABLE_CSHARP)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/scripting/csharp/.*\\.cpp$")
endif()

if(NOT DSE_ENABLE_SPINE)
    list(FILTER engine_cpp EXCLUDE REGEX ".*modules/gameplay_2d/spine/.*\\.cpp$")
endif()

if(NOT DSE_ENABLE_VIRTUAL_GEOMETRY)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/render/virtual_geometry/.*\\.cpp$")
endif()

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
    file(GLOB_RECURSE gameplay_3d_cpp CONFIGURE_DEPENDS "modules/gameplay_3d/*.cpp")
    list(APPEND engine_cpp ${gameplay_3d_cpp})
endif()

# 物理后端源文件排除逻辑（必须在 gameplay_3d append 之后执行）
set(_HAS_ANY_PHYSICS3D OFF)
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    set(_HAS_ANY_PHYSICS3D ON)
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d_system_jolt\\.cpp$")
elseif(DSE_ENABLE_JOLT AND DSE_HAS_JOLT)
    set(_HAS_ANY_PHYSICS3D ON)
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d/physics3d_system\\.cpp$")
endif()
if(NOT _HAS_ANY_PHYSICS3D)
    list(FILTER engine_cpp EXCLUDE REGEX ".*/physics3d/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/vehicle/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/buoyancy/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/ragdoll/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*/fracture/.*\\.cpp$")
    # PhysicsLODSystem 是纯距离 LOD 逻辑(sleep/wake 调度)，不依赖任何物理后端(无 Jolt/PhysX 头/类型)，
    # 且被 open-world C ABI(dse_api_open_world_p2p5.cpp)无条件引用；故无物理后端时仍需编入，
    # 否则 Web/无物理构建链接缺失 PhysicsLODSystem 符号。
    list(APPEND engine_cpp "${CMAKE_SOURCE_DIR}/engine/physics/physics3d/physics_lod.cpp")
endif()

# NavMesh 禁用时排除 navigation 源文件
if(NOT DSE_ENABLE_NAVMESH)
    list(FILTER engine_cpp EXCLUDE REGEX ".*engine/navigation/.*\\.cpp$")
    list(FILTER engine_cpp EXCLUDE REGEX ".*modules/gameplay_3d/ai/nav_.*\\.cpp$")
endif()

# Spine 运行时（重型第三方库）仅在启用时编译
if(DSE_ENABLE_SPINE)
    file(GLOB_RECURSE spine_cpp "${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/*.cpp")
endif()
set(tiny_aes_c
    "${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/aes.c"
)
file(GLOB_RECURSE bundle_cpp "${CMAKE_SOURCE_DIR}/depends/bundle/*.cpp")

# bundle 第三方源编译选项
if(NOT MSVC)
    set_source_files_properties(${bundle_cpp} PROPERTIES
        COMPILE_OPTIONS "-Wno-register;-Wno-c++11-narrowing")
endif()
if(ANDROID OR EMSCRIPTEN)
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
