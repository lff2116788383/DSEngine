cmake_minimum_required(VERSION 3.17)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()
project(DSEngine C CXX)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D GLM_SWIZZLE)#GLM开启重排，随意转换。 vec2 v2 = v4.xz();

add_definitions(-D USE_FMOD_STUDIO)#是否使用fmod studio，加载bank文件的形式。

option(DSE_ENABLE_PHYSX "Enable PhysX integration" ON)
option(DSE_ENABLE_2D "Enable 2D runtime path" ON)
option(DSE_ENABLE_3D "Enable 3D runtime path" OFF)
option(DSE_BUILD_EDITOR "Build editor via npm/node-gyp" OFF)
option(DSE_BUILD_LAUNCHER "Build launcher target" OFF)
option(DSE_BUILD_ENGINE_TESTS "Build engine unit test targets" OFF)
if(DSE_ENABLE_2D)
    add_definitions(-D DSE_ENABLE_2D)
endif()
if(DSE_ENABLE_3D)
    add_definitions(-D DSE_ENABLE_3D)
endif()

# 设置输出目录为 bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

if (MSVC)
    set(CMAKE_VS_GLOBALS "VcpkgApplocalDeps=false")
    add_compile_options(/bigobj)
    add_compile_options(/wd4819)#忽略保存为unicode警告
    add_compile_options(/FS) # 修复多线程编译 PDB 写入冲突 C1041
    add_compile_options("$<$<CONFIG:Debug>:/Z7>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/Z7>")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif ()
include(cmake/CMakeLists.txt.glfw)
include(cmake/CMakeLists.txt.FreeType)
include(cmake/CMakeLists.txt.easy_profile_core)
include(cmake/CMakeLists.txt.lua_script)
include(cmake/CMakeLists.txt.luasocket)
if(DSE_ENABLE_PHYSX)
    include(cmake/CMakeLists.txt.Physx)
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
include_directories("depends/luasocket-3.0.0/src")
include_directories("depends/glm")
include_directories("depends/glm_ext")
include_directories("depends/rapidjson")
include_directories("depends/eventpp/tests")
include_directories("depends/eventpp/include")
include_directories("depends/timetool")
include_directories("depends/tinygltf")
include_directories("depends/assimp/include")

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
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
add_subdirectory(depends/assimp)

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

# PhysX 禁用时排除 physics3d 相关源文件
if(NOT DSE_ENABLE_PHYSX)
    list(FILTER engine_cpp EXCLUDE REGEX ".*physics3d.*\\.cpp$")
endif()

if(NOT DSE_ENABLE_3D)
    list(APPEND engine_cpp
        "modules/gameplay_3d/rendering/mesh_render_system.cpp"
        "modules/gameplay_3d/animation/animation_state_machine.cpp"
    )
endif()

if(DSE_ENABLE_3D)
    file(GLOB_RECURSE gameplay_3d_cpp "modules/gameplay_3d/*.cpp")
    
    add_library(DSE_Gameplay3D SHARED ${gameplay_3d_cpp})
    target_include_directories(DSE_Gameplay3D PRIVATE 
        ${CMAKE_SOURCE_DIR} 
        ${CMAKE_SOURCE_DIR}/engine 
        ${CMAKE_SOURCE_DIR}/modules
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
        ${CMAKE_SOURCE_DIR}/depends/glm
    )
    
    target_compile_definitions(DSE_Gameplay3D PRIVATE DSE_GAMEPLAY3D_EXPORTS)

    if(DSE_ENABLE_PHYSX)
        target_link_libraries(DSE_Gameplay3D PRIVATE dse_physx)
    endif()
    set_target_properties(DSE_Gameplay3D PROPERTIES OUTPUT_NAME "DSE_Gameplay3D")
    
    if(WIN32)
        set_target_properties(DSE_Gameplay3D PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()
    
    if (MSVC)
        set_property(TARGET DSE_Gameplay3D PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

file(GLOB_RECURSE box2d_cpp "${BOX2D_ROOT}/src/*.cpp")
file(GLOB_RECURSE spine_cpp "${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/*.cpp")
file(GLOB_RECURSE tiny_aes_c "${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/*.c")
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
    ${box2d_cpp}
    ${spine_cpp}
    ${tiny_aes_c}
    ${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/aes.c
    ${bundle_cpp}
    ${imgui_src}
    ${engine_cpp}
    engine/runtime/runtime_frame_ops.cpp
)
target_link_libraries(dse_engine PRIVATE assimp)
set_target_properties(dse_engine PROPERTIES OUTPUT_NAME "DSEngine")
if(WIN32)
    set_target_properties(dse_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
if(MSVC)
    target_compile_options(dse_engine PRIVATE /wd5033 /wd4834)
endif()

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

if (MSVC)
    set_property(TARGET dse_engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
        $<TARGET_FILE_DIR:${target_name}>
        COMMAND_EXPAND_LISTS
    )

    if(MSVC AND DSE_ENABLE_PHYSX)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(_dse_physx_dlls ${PHYSX_DLLS_DEBUG})
        elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(_dse_physx_dlls ${PHYSX_DLLS_RELEASE})
        else()
            set(_dse_physx_dlls ${PHYSX_DLLS_DEBUG} ${PHYSX_DLLS_RELEASE})
        endif()

        if(_dse_physx_dlls)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_dse_physx_dlls}
                $<TARGET_FILE_DIR:${target_name}>
                COMMAND_EXPAND_LISTS
            )
        endif()
    endif()
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

if(DSE_BUILD_ENGINE_TESTS)
    add_subdirectory(tests)
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
