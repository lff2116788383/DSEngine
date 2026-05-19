set(DSE_GTEST_INTEGRATION_TARGET dse_gtest_integration_tests)

add_executable(${DSE_GTEST_INTEGRATION_TARGET}
    # engine/assets
    engine/assets/assetmanager_vfs_integration_test.cpp
    engine/assets/asset_async_full_chain_integration_test.cpp
    # engine/core
    engine/core/eventbus_servicelocator_integration_test.cpp
    engine/core/jobsystem_eventbus_integration_test.cpp
    # engine/input
    engine/input/input_actionmapping_integration_test.cpp
    # engine/ecs
    engine/ecs/ecs_physics2d_integration_test.cpp
    engine/ecs/ecs_transform_integration_test.cpp
    engine/ecs/gameplay3d_ecs_integration_test.cpp
    # engine/physics
    engine/physics/physics2d_collision_event_integration_test.cpp
    engine/physics/physics2d_joint_integration_test.cpp
    engine/physics/physics2d_circle_collider_integration_test.cpp
    engine/physics/physics2d_polygon_collider_integration_test.cpp
    engine/physics/physics3d_ecs_integration_test.cpp
    # engine/profiler
    engine/profiler/profiler_jobsystem_integration_test.cpp
    # engine/render
    engine/render/rendergraph_integration_test.cpp
    engine/render/render_scene_integration_test.cpp
    # engine/scene
    engine/scene/scene_world_serialization_integration_test.cpp
    # engine/scripting
    engine/scripting/lua_runtime_integration_test.cpp
    engine/scripting/lua_eventbus_integration_test.cpp
    engine/scripting/lua_ecs_integration_test.cpp
    engine/scripting/lua_binding_3d_integration_test.cpp
    engine/scripting/lua_binding_audio_integration_test.cpp
    engine/scripting/lua_binding_physics2d_integration_test.cpp
    engine/scripting/lua_binding_animation_integration_test.cpp
    engine/scripting/lua_binding_dssl_integration_test.cpp
    engine/scripting/lua_binding_ui_integration_test.cpp
    engine/scripting/lua_binding_particles_integration_test.cpp
    # engine/runtime
    engine/runtime/runtime_subsystem_integration_test.cpp
    # engine/audio
    engine/audio/audio_asset_integration_test.cpp
    # modules
    modules/module_lifecycle_integration_test.cpp
    modules/fracture_system_integration_test.cpp
    modules/cloth_system_integration_test.cpp
    modules/fluid_system_integration_test.cpp
    # editor (headless functional tests)
    editor/editor_functional_test.cpp
    editor/editor_selection_integration_test.cpp
    editor/editor_control_server_test.cpp
    editor/editor_plugin_manager_test.cpp
    editor/editor_test_stubs.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_test_harness.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_snapshot.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_scene_io.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_prefab.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_scene_tabs.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_control_server.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_control_tools.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_plugin_manager.cpp
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_settings.cpp
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
