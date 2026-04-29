set(DSE_GTEST_INTEGRATION_TARGET dse_gtest_integration_tests)

add_executable(${DSE_GTEST_INTEGRATION_TARGET}
    # engine/assets
    engine/assets/assetmanager_vfs_integration_test.cpp
    engine/assets/asset_async_full_chain_integration_test.cpp
    # engine/core
    engine/core/eventbus_servicelocator_integration_test.cpp
    engine/core/jobsystem_eventbus_integration_test.cpp
    # engine/ecs
    engine/ecs/ecs_physics2d_integration_test.cpp
    engine/ecs/ecs_transform_integration_test.cpp
    # engine/physics
    engine/physics/physics2d_collision_event_integration_test.cpp
    # engine/render
    engine/render/rendergraph_integration_test.cpp
    # engine/scene
    engine/scene/scene_world_serialization_integration_test.cpp
    # engine/scripting
    engine/scripting/lua_runtime_integration_test.cpp
    engine/scripting/lua_eventbus_integration_test.cpp
    # modules
    modules/module_lifecycle_integration_test.cpp
)

target_include_directories(${DSE_GTEST_INTEGRATION_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
)

target_compile_features(${DSE_GTEST_INTEGRATION_TARGET} PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(${DSE_GTEST_INTEGRATION_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_GTEST_INTEGRATION_TARGET}
    PRIVATE
        GTest::gtest_main
        GTest::gmock
        dse_engine
)

add_test(
    NAME gtest.engine.integration
    COMMAND ${DSE_GTEST_INTEGRATION_TARGET}
)
set_tests_properties(gtest.engine.integration PROPERTIES
    LABELS "engine;integration;gtest"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
