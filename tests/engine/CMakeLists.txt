set(DSE_ENGINE_TEST_TARGET dse_engine_unit_tests)

add_executable(${DSE_ENGINE_TEST_TARGET}
    main.cpp
    base/tween_test.cpp
    base/time_test.cpp
    input/input_test.cpp
    core/event_bus_test.cpp
    core/job_system_test.cpp
    ecs/world_test.cpp
    scene/transform_system_test.cpp
    scene/prefab_test.cpp
    scene/octree_test.cpp
    runtime/runtime_access_test.cpp
    scripting/cpp_business_runtime_test.cpp
    ../modules/gameplay_2d/camera/camera_system_test.cpp
    ../modules/gameplay_2d/animation/animation_system_test.cpp
    ../modules/gameplay_2d/particle/particle_system_test.cpp
    ../modules/gameplay_2d/tilemap/tilemap_system_test.cpp
    ../modules/gameplay_2d/ui/ui_system_test.cpp
    ../modules/gameplay_3d/animation/animator_system_test.cpp
    ../modules/gameplay_3d/rendering/terrain_system_test.cpp
    ../modules/gameplay_3d/rendering/csm_test.cpp
    ../modules/gameplay_3d/rendering/mesh_renderer_test.cpp
    ../modules/gameplay_3d/rendering/camera_3d_test.cpp
    ../modules/gameplay_3d/rendering/skybox_test.cpp
    ../modules/gameplay_3d/rendering/frustum_culling_test.cpp
    ../modules/gameplay_3d/rendering/advanced_3d_components_test.cpp
    ../modules/gameplay_3d/ai/steering_system_test.cpp
    ${CMAKE_SOURCE_DIR}/engine/base/time.cpp
    ${CMAKE_SOURCE_DIR}/engine/base/debug.cpp
    ${CMAKE_SOURCE_DIR}/engine/input/input.cpp
    ${CMAKE_SOURCE_DIR}/engine/core/job_system.cpp
    ${CMAKE_SOURCE_DIR}/engine/ecs/world.cpp
    ${CMAKE_SOURCE_DIR}/engine/scene/scene.cpp
    ${CMAKE_SOURCE_DIR}/engine/scene/transform_system.cpp
    ${CMAKE_SOURCE_DIR}/engine/scripting/cpp/cpp_business_runtime.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/camera/camera_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/animation/animation_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/particle/particle_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/tilemap/tilemap_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/ui/ui_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_3d/rendering/frustum_culling_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_3d/ai/steering_system.cpp
)

target_include_directories(${DSE_ENGINE_TEST_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
)

target_compile_features(${DSE_ENGINE_TEST_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_ENGINE_TEST_TARGET} PRIVATE /utf-8)
endif()

add_test(
    NAME engine.unit
    COMMAND ${DSE_ENGINE_TEST_TARGET}
)

set_tests_properties(engine.unit PROPERTIES LABELS "engine;unit")
