set(DSE_GTEST_INTEGRATION_TARGET dse_gtest_integration_tests)

file(GLOB_RECURSE DSE_GTEST_INTEGRATION_SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/*.cpp"
)

if(NOT DSE_ENABLE_3D)
    list(FILTER DSE_GTEST_INTEGRATION_SRCS EXCLUDE REGEX ".*/engine/ecs/gameplay3d_ecs_integration_test\\.cpp$")
    list(FILTER DSE_GTEST_INTEGRATION_SRCS EXCLUDE REGEX ".*/engine/physics/physics3d_ecs_integration_test\\.cpp$")
    list(FILTER DSE_GTEST_INTEGRATION_SRCS EXCLUDE REGEX ".*/engine/scripting/lua_binding_3d_integration_test\\.cpp$")
    list(FILTER DSE_GTEST_INTEGRATION_SRCS EXCLUDE REGEX ".*/engine/scripting/lua_binding_animation_integration_test\\.cpp$")
    list(FILTER DSE_GTEST_INTEGRATION_SRCS EXCLUDE REGEX ".*/modules/(fracture|cloth|fluid)_system_integration_test\\.cpp$")
endif()

add_executable(${DSE_GTEST_INTEGRATION_TARGET}
    ${DSE_GTEST_INTEGRATION_SRCS}
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_test_harness.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_scene_io.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_prefab.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_scene_tabs.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_control_server.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_control_tools.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_plugin_manager.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_settings.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_crash.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_visual_script_compiler.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_collider_edit.cpp
)

target_include_directories(${DSE_GTEST_INTEGRATION_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/modules
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
        ${CMAKE_SOURCE_DIR}/depends/glm
        ${CMAKE_SOURCE_DIR}/depends/rapidjson/include
        ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src
        ${CMAKE_SOURCE_DIR}/depends/imgui
        ${CMAKE_SOURCE_DIR}/depends/imgui/backends
)

target_compile_features(${DSE_GTEST_INTEGRATION_TARGET} PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(${DSE_GTEST_INTEGRATION_TARGET} PRIVATE /utf-8)
endif()

# IXWebSocket — 若 editor 未构建则自行引入
if(NOT TARGET ixwebsocket)
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    set(USE_ZLIB OFF CACHE BOOL "" FORCE)
    set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/depends/IXWebSocket ${CMAKE_CURRENT_BINARY_DIR}/IXWebSocket)
endif()

target_link_libraries(${DSE_GTEST_INTEGRATION_TARGET}
    PRIVATE
        GTest::gtest_main
        GTest::gmock
        dse_engine
        ixwebsocket
)

# PhysX 运行时 DLL 复制（仅当 PhysX 已启用且库已找到时）
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_custom_command(TARGET ${DSE_GTEST_INTEGRATION_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${PHYSX_DLLS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLLS_RELEASE}>"
            $<TARGET_FILE_DIR:${DSE_GTEST_INTEGRATION_TARGET}>
        COMMAND_EXPAND_LISTS
    )
endif()

add_test(
    NAME gtest.engine.integration
    COMMAND ${DSE_GTEST_INTEGRATION_TARGET}
)
set_tests_properties(gtest.engine.integration PROPERTIES
    LABELS "engine;integration;gtest"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
