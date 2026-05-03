cmake_minimum_required(VERSION 3.17)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()
project(DSEngine C CXX)
include(GNUInstallDirs)


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

option(DSE_ENABLE_PHYSX "Enable PhysX integration" ON)
option(DSE_ENABLE_2D "Enable 2D runtime path" ON)
option(DSE_ENABLE_3D "Enable 3D runtime path" ON)
option(DSE_ENABLE_SPINE "Enable Spine runtime path" ON)
option(DSE_BUILD_EDITOR "Build editor via npm/node-gyp" OFF)
option(DSE_BUILD_LAUNCHER "Build launcher target" OFF)
option(DSE_BUILD_GTESTS "Build GoogleTest based test targets" ON)


if(DSE_ENABLE_2D)
    add_definitions(-D DSE_ENABLE_2D)
endif()
if(DSE_ENABLE_3D)
    add_definitions(-D DSE_ENABLE_3D)
endif()
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
    add_compile_options(/wd4819)#忽略保存为unicode警告
    add_compile_options(/FS) # 修复多线程编译 PDB 写入冲突 C1041
    add_compile_options("$<$<CONFIG:Debug>:/Z7>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Z7>")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    add_compile_definitions(_CRT_STDIO_ISO_WIDE_SPECIFIERS=1)
    add_compile_definitions(_CRT_NONSTDC_NO_WARNINGS=1)
    add_compile_definitions(_CRT_DECLARE_NONSTDC_NAMES=1)
endif ()
include(cmake/CMakeLists.txt.glfw)
include(cmake/CMakeLists.txt.FreeType)
include(cmake/CMakeLists.txt.easy_profile_core)
include(cmake/CMakeLists.txt.lua_script)
include(cmake/CMakeLists.txt.luasocket)
if(DSE_ENABLE_PHYSX)
    include(cmake/CMakeLists.txt.Physx)
endif()

# DSE_ENABLE_PHYSX 宏需在 Physx cmake include 之后定义，
# 因为 DSE_HAS_PHYSX_LIBS 在 cmake/CMakeLists.txt.Physx 中才被设置
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_definitions(-D DSE_ENABLE_PHYSX)
endif()

#头文件目录
include_directories("depends")
include_directories("${CMAKE_SOURCE_DIR}")
include_directories("engine")
include_directories("modules")
include_directories("samples")
include_directories("depends/spdlog/include")

include_directories("depends/spscqueue/include")
include_directories("depends/rttr-0.9.6/src")
include_directories("depends/glm")
include_directories("depends/glm_ext")
include_directories("depends/rapidjson/include")
include_directories("depends/eventpp/include")
include_directories("depends/timetool")
include_directories("depends/tinygltf")
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
    add_subdirectory(depends/assimp)
    # 强制 assimp 使用 DLL CRT，与 dse_engine 的 /MDd 一致
    if(MSVC)
        set_property(TARGET assimp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()


set(CMAKE_WARN_DEPRECATED ${DSE_PREV_CMAKE_WARN_DEPRECATED})
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ${DSE_PREV_CMAKE_SUPPRESS_DEVELOPER_WARNINGS})

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/modules
    ${CMAKE_SOURCE_DIR}/samples
    ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
    ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include
    ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/include
    ${CMAKE_SOURCE_DIR}/depends/tiny-AES-c
    ${CMAKE_SOURCE_DIR}/depends/imgui
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends
)
set(BOX2D_ROOT "${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1")

file(GLOB_RECURSE engine_cpp
    "engine/*.cpp"
    "modules/gameplay_2d/*.cpp"
)

list(FILTER engine_cpp EXCLUDE REGEX ".*engine/assets/compiler/.*\\.cpp$")


if(NOT DSE_ENABLE_SPINE)
    list(FILTER engine_cpp EXCLUDE REGEX ".*modules/gameplay_2d/spine/.*\\.cpp$")
endif()

# PhysX 禁用或缺库时排除 physics3d 相关源文件
if(NOT DSE_ENABLE_PHYSX)
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d.*\\.cpp$")
elseif(MSVC AND NOT DSE_HAS_PHYSX_LIBS)
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d.*\\.cpp$")
endif()

if(NOT DSE_ENABLE_3D)
    list(APPEND engine_cpp
        "modules/gameplay_3d/rendering/mesh_render_system.cpp"
        "modules/gameplay_3d/animation/animation_state_machine.cpp"
        "modules/gameplay_3d/animation/animator_system.cpp"
        "modules/gameplay_3d/particles/particle3d_system.cpp"
        "modules/gameplay_3d/ai/steering_system.cpp"
    )
endif()

if(DSE_ENABLE_3D)
    file(GLOB_RECURSE gameplay_3d_cpp "modules/gameplay_3d/*.cpp")
    list(APPEND engine_cpp
        "modules/gameplay_3d/rendering/mesh_render_system.cpp"
        "modules/gameplay_3d/animation/animation_state_machine.cpp"
    )

    add_library(DSE_Gameplay3D SHARED ${gameplay_3d_cpp})


    target_include_directories(DSE_Gameplay3D PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/modules
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
        ${CMAKE_SOURCE_DIR}/depends/glm
    )
    
    target_compile_definitions(DSE_Gameplay3D PRIVATE
        DSE_GAMEPLAY3D_EXPORTS
        $<$<BOOL:${DSE_HAS_PHYSX_LIBS}>:DSE_HAS_PHYSX_LIBS=1>
    )


    if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
        target_include_directories(DSE_Gameplay3D PRIVATE ${PHYSX_INCLUDE_DIR} ${PXSHARED_INCLUDE_DIR})
        target_link_directories(DSE_Gameplay3D PRIVATE
            $<$<CONFIG:Debug>:${PHYSX_DLL_DIR_DEBUG}>
            $<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLL_DIR_RELEASE}>
        )
        target_link_libraries(DSE_Gameplay3D PRIVATE ${PHYSX_LIBS})
    endif()
    set_target_properties(DSE_Gameplay3D PROPERTIES OUTPUT_NAME "DSE_Gameplay3D")
    
    if(WIN32)
        set_target_properties(DSE_Gameplay3D PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
    
    if (MSVC)
        set_property(TARGET DSE_Gameplay3D PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

file(GLOB_RECURSE spine_cpp "${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/*.cpp")
set(tiny_aes_c
    "${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/aes.c"
)
file(GLOB_RECURSE bundle_cpp "${CMAKE_SOURCE_DIR}/depends/bundle/*.cpp")


set(imgui_src
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui_tables.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_glfw.cpp"
    "${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_opengl3.cpp"
)

add_library(dse_engine SHARED
    ${glfw_sources}
    ${freetype_src}
    ${easy_profiler_core_source}
    ${lua_src}
    ${luasocket_c}
    ${spine_cpp}
    ${tiny_aes_c}
    ${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/aes.c
    ${bundle_cpp}
    ${imgui_src}
    ${engine_cpp}
    modules/gameplay_2d/gameplay_2d_module.cpp
    engine/runtime/runtime_frame_ops.cpp
    engine/runtime/runtime_update_graph.cpp
    engine/runtime/runtime_render_shell.cpp
    engine/runtime/business_runtime_bridge.cpp
)
target_link_libraries(dse_engine PRIVATE box2d)
if(DSE_HAS_ASSIMP)
    target_link_libraries(dse_engine PRIVATE assimp)
endif()
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    target_include_directories(dse_engine PRIVATE ${PHYSX_INCLUDE_DIR} ${PXSHARED_INCLUDE_DIR})
    target_link_directories(dse_engine PRIVATE
        $<$<CONFIG:Debug>:${PHYSX_DLL_DIR_DEBUG}>
        $<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLL_DIR_RELEASE}>
    )
    target_link_libraries(dse_engine PRIVATE ${PHYSX_LIBS})
endif()
set_target_properties(dse_engine PROPERTIES OUTPUT_NAME "DSEngine")
if(WIN32)
    set_target_properties(dse_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()




if(MSVC)
    target_compile_options(dse_engine PRIVATE /utf-8 /wd5033 /wd4834)
endif()

target_include_directories(dse_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/depends/lua
    ${CMAKE_SOURCE_DIR}/depends/luasocket-3.0.0/src
    ${CMAKE_SOURCE_DIR}/depends/glad/include
)

if(DSE_ENABLE_3D)
    target_link_libraries(DSE_Gameplay3D PRIVATE dse_engine)
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
if(MSVC)
    set_target_properties(dse_engine PROPERTIES
        IMPORT_SUFFIX "_debug.lib"
        IMPORT_PREFIX ""
    )
endif()

if (MSVC)
    set_property(TARGET dse_engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    add_definitions(-D _DEBUG)
endif ()

if(DSE_ENABLE_3D)
    dse_apply_config_postfix(DSE_Gameplay3D)
    install(TARGETS DSE_Gameplay3D
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

function(dse_copy_runtime_assets target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target_name}>
        $<$<TARGET_EXISTS:DSE_Gameplay3D>:$<TARGET_FILE:DSE_Gameplay3D>>
        $<TARGET_FILE_DIR:${target_name}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/samples
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/samples
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/script
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/script
        COMMAND_EXPAND_LISTS
    )
endfunction()

function(dse_copy_runtime_dlls target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target_name}>
        $<$<TARGET_EXISTS:DSE_Gameplay3D>:$<TARGET_FILE:DSE_Gameplay3D>>
        $<TARGET_FILE_DIR:${target_name}>
        COMMAND_EXPAND_LISTS
    )
endfunction()

# 包含运行时宿主入口
add_subdirectory(apps/runtime)

if(DSE_BUILD_EDITOR)
    add_subdirectory(apps/editor_cpp)
endif()

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


install(TARGETS dse_engine
    EXPORT DSEngineTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/engine
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/modules/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/modules
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/script/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/DSEngine/script
    FILES_MATCHING PATTERN "*.lua"
)

install(EXPORT DSEngineTargets
    FILE DSEngineTargets.cmake
    NAMESPACE DSEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSEngine
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
