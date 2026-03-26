cmake_minimum_required(VERSION 3.17)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
project(DSEngine)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D GLM_SWIZZLE)#GLM开启重排，随意转换。 vec2 v2 = v4.xz();

add_definitions(-D USE_FMOD_STUDIO)#是否使用fmod studio，加载bank文件的形式。

option(DSE_ENABLE_PHYSX "Enable PhysX integration" OFF)
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
# Use local depends instead of FetchContent
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENTT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/entt-3.13.0)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(depends/box2d-2.4.1)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/modules
    ${CMAKE_SOURCE_DIR}/samples
    ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
    ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include
)
set(BOX2D_ROOT "${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1")

file(GLOB_RECURSE engine_cpp
    "engine/*.cpp"
    "modules/*.cpp"
)
file(GLOB_RECURSE box2d_cpp "${BOX2D_ROOT}/src/*.cpp")

add_library(dse_engine SHARED
    ${glfw_sources}
    ${freetype_src}
    ${easy_profiler_core_source}
    ${lua_src}
    ${luasocket_c}
    ${box2d_cpp}
    ${engine_cpp}
)
set_target_properties(dse_engine PROPERTIES OUTPUT_NAME "DSEngine")
if(WIN32)
    set_target_properties(dse_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
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

# 包含运行时宿主入口
add_subdirectory(apps/runtime)

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
set(DSE_NPM_CACHE_DIR "${CMAKE_SOURCE_DIR}/.npm-cache")
set(DSE_ELECTRON_MIRROR "" CACHE STRING "Electron download mirror URL")
set(DSE_ELECTRON_CACHE_DIR "${CMAKE_SOURCE_DIR}/.cache/electron" CACHE PATH "Electron download cache directory")
set(DSE_ELECTRON_BUILDER_CACHE_DIR "${CMAKE_SOURCE_DIR}/.cache/electron-builder" CACHE PATH "electron-builder cache directory")
set(DSE_ELECTRON_CUSTOM_DIR "" CACHE STRING "Custom Electron directory name for mirror/offline dist")
set(DSE_ELECTRON_BUILDER_BINARIES_MIRROR "" CACHE STRING "electron-builder binaries mirror URL")
set(DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR "" CACHE STRING "Custom electron-builder binaries directory name for mirror/offline dist")
file(MAKE_DIRECTORY "${DSE_NPM_CACHE_DIR}")
file(MAKE_DIRECTORY "${DSE_ELECTRON_CACHE_DIR}")
file(MAKE_DIRECTORY "${DSE_ELECTRON_BUILDER_CACHE_DIR}")
set(DSE_NODE_BUILD_ENV
    NPM_CONFIG_CACHE=${DSE_NPM_CACHE_DIR}
    ELECTRON_CACHE=${DSE_ELECTRON_CACHE_DIR}
    ELECTRON_BUILDER_CACHE=${DSE_ELECTRON_BUILDER_CACHE_DIR}
)
if(DSE_ELECTRON_MIRROR)
    list(APPEND DSE_NODE_BUILD_ENV "ELECTRON_MIRROR=${DSE_ELECTRON_MIRROR}")
endif()
if(DSE_ELECTRON_CUSTOM_DIR)
    list(APPEND DSE_NODE_BUILD_ENV "ELECTRON_CUSTOM_DIR=${DSE_ELECTRON_CUSTOM_DIR}")
endif()
if(DSE_ELECTRON_BUILDER_BINARIES_MIRROR)
    list(APPEND DSE_NODE_BUILD_ENV "ELECTRON_BUILDER_BINARIES_MIRROR=${DSE_ELECTRON_BUILDER_BINARIES_MIRROR}")
endif()
if(DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR)
    list(APPEND DSE_NODE_BUILD_ENV "ELECTRON_BUILDER_BINARIES_CUSTOM_DIR=${DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR}")
endif()
if(WIN32 AND MSVC)
    set(DSE_VS_INSTALL_DIR "")
    if(CMAKE_GENERATOR_INSTANCE)
        set(DSE_VS_INSTALL_DIR "${CMAKE_GENERATOR_INSTANCE}")
    endif()
    if(NOT DSE_VS_INSTALL_DIR AND DEFINED ENV{VSINSTALLDIR})
        set(DSE_VS_INSTALL_DIR "$ENV{VSINSTALLDIR}")
    endif()
    if(DSE_VS_INSTALL_DIR)
        set(DSE_VS_DEV_CMD "${DSE_VS_INSTALL_DIR}/Common7/Tools/VsDevCmd.bat")
        if(EXISTS "${DSE_VS_DEV_CMD}")
            file(TO_NATIVE_PATH "${DSE_VS_DEV_CMD}" DSE_VS_DEV_CMD_NATIVE)
            file(TO_NATIVE_PATH "${NPX_EXECUTABLE}" DSE_NPX_EXECUTABLE_NATIVE)
            file(TO_NATIVE_PATH "${NPM_EXECUTABLE}" DSE_NPM_EXECUTABLE_NATIVE)
            set(DSE_VC_INSTALL_DIR "${DSE_VS_INSTALL_DIR}/VC/")
            list(APPEND DSE_NODE_BUILD_ENV
                "VCINSTALLDIR=${DSE_VC_INSTALL_DIR}"
            )
        endif()
    endif()
endif()
if(Python3_Interpreter_FOUND)
    list(APPEND DSE_NODE_BUILD_ENV "npm_config_python=${Python3_EXECUTABLE}")
endif()

set(DSE_EDITOR_NODE_GYP_COMMAND
    ${CMAKE_COMMAND} -E env ${DSE_NODE_BUILD_ENV} ${NPX_EXECUTABLE} node-gyp rebuild --msvs_version=2022
)
set(DSE_EDITOR_PACKAGE_COMMAND
    ${CMAKE_COMMAND} -E env ${DSE_NODE_BUILD_ENV} ${NPM_EXECUTABLE} run package:win
)

if(DSE_BUILD_EDITOR)
    if(NPM_EXECUTABLE AND NPX_EXECUTABLE)
        add_custom_target(dse_editor ALL
            COMMAND ${CMAKE_COMMAND} -E env ${DSE_NODE_BUILD_ENV} ${NPM_EXECUTABLE} install --ignore-scripts
            COMMAND ${DSE_EDITOR_NODE_GYP_COMMAND}
            COMMAND ${CMAKE_COMMAND} -E env ${DSE_NODE_BUILD_ENV} ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/editor
        )
        add_custom_target(dse_editor_pack ALL
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin/editor
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/apps/editor/main.js
                ${CMAKE_SOURCE_DIR}/apps/editor/preload.js
                ${CMAKE_SOURCE_DIR}/apps/editor/index.html
                ${CMAKE_SOURCE_DIR}/apps/editor/package.json
                ${CMAKE_SOURCE_DIR}/bin/editor
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/apps/editor/dist
                ${CMAKE_SOURCE_DIR}/bin/editor/dist
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/apps/editor/scripts
                ${CMAKE_SOURCE_DIR}/bin/editor/scripts
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/editor
        )
        add_dependencies(dse_editor_pack dse_editor)
        add_custom_target(dse_editor_exe
            COMMAND ${DSE_EDITOR_PACKAGE_COMMAND}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/editor
        )
        add_dependencies(dse_editor_exe dse_editor)
    else()
        message(FATAL_ERROR "DSE_BUILD_EDITOR requires npm and npx")
    endif()
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

if(NPM_EXECUTABLE AND NPX_EXECUTABLE)
    add_test(
        NAME editor_bridge_rebuild
        COMMAND ${NPX_EXECUTABLE} node-gyp rebuild
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/editor
    )
    set_tests_properties(editor_bridge_rebuild PROPERTIES FIXTURES_SETUP editor_bridge_ready)

    add_test(
        NAME editor_material_replay_regression
        COMMAND ${NPM_EXECUTABLE} run regress:material-replay
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/editor
    )
    set_tests_properties(editor_material_replay_regression PROPERTIES FIXTURES_REQUIRED editor_bridge_ready)

    add_test(
        NAME editor_scene_schema_migrate_regression
        COMMAND ${NPM_EXECUTABLE} run regress:scene-schema-migrate
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/editor
    )
endif()
