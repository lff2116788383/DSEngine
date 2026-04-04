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
    ../modules/gameplay_2d/localization/localization_system_test.cpp
    ../modules/gameplay_2d/localization/font_manager_test.cpp
    ../modules/gameplay_2d/particle/particle_system_test.cpp
    ../modules/gameplay_2d/particle/particle_advanced_test.cpp
    ../modules/gameplay_2d/tilemap/tilemap_system_test.cpp
    ../modules/gameplay_2d/ui/ui_system_test.cpp
    ../modules/gameplay_2d/ui/ui_layout_test.cpp
    ../modules/gameplay_2d/ui/ui_animation_test.cpp
    ../modules/gameplay_2d/ui/ui_advanced_test.cpp
    ../modules/gameplay_2d/physics/physics2d_advanced_test.cpp
    ../modules/gameplay_2d/tilemap/tilemap_advanced_test.cpp
    profiler/cpu_profiler_test.cpp
    profiler/memory_profiler_test.cpp
    profiler/render_profiler_test.cpp
    ../modules/gameplay_2d/physics/physics2d_system_test.cpp
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
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/localization/localization_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/localization/font_manager.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/particle/particle_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/tilemap/tilemap_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/ui/ui_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/ui/ui_layout.cpp
    ${CMAKE_SOURCE_DIR}/engine/profiler/cpu_profiler.cpp
    ${CMAKE_SOURCE_DIR}/engine/profiler/memory_profiler.cpp
    ${CMAKE_SOURCE_DIR}/engine/profiler/render_profiler.cpp
    ${CMAKE_SOURCE_DIR}/engine/physics/physics2d/physics2d_system.cpp
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

target_link_libraries(${DSE_ENGINE_TEST_TARGET}
    PRIVATE
        box2d
        assimp
)

add_test(
    NAME engine.unit
    COMMAND ${DSE_ENGINE_TEST_TARGET}
)

set_tests_properties(engine.unit PROPERTIES LABELS "engine;unit")

add_test(
    NAME engine.2d.ui
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[ui]"
)
set_tests_properties(engine.2d.ui PROPERTIES LABELS "engine;unit;2d;ui;smoke")

add_test(
    NAME engine.2d.tilemap
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[tilemap]"
)
set_tests_properties(engine.2d.tilemap PROPERTIES LABELS "engine;unit;2d;tilemap;smoke")

add_test(
    NAME engine.2d.particle
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[particle]"
)
set_tests_properties(engine.2d.particle PROPERTIES LABELS "engine;unit;2d;particle;smoke")

add_test(
    NAME engine.2d.physics2d
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[physics2d]"
)
set_tests_properties(engine.2d.physics2d PROPERTIES LABELS "engine;unit;2d;physics2d;smoke")

add_test(
    NAME engine.2d.localization
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[localization]"
)
set_tests_properties(engine.2d.localization PROPERTIES LABELS "engine;unit;2d;localization;smoke")

add_test(
    NAME engine.2d.animation
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[animation]"
)
set_tests_properties(engine.2d.animation PROPERTIES LABELS "engine;unit;2d;animation;smoke")

add_test(
    NAME engine.2d.camera
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[camera]"
)
set_tests_properties(engine.2d.camera PROPERTIES LABELS "engine;unit;2d;camera;smoke")

set(DSE_LUA_RUNTIME_TEST_TARGET dse_lua_runtime_tests)

add_executable(${DSE_LUA_RUNTIME_TEST_TARGET}
    main.cpp
    scripting/lua_runtime_test.cpp
)

target_include_directories(${DSE_LUA_RUNTIME_TEST_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
)

target_compile_features(${DSE_LUA_RUNTIME_TEST_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_LUA_RUNTIME_TEST_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_LUA_RUNTIME_TEST_TARGET}
    PRIVATE
        dse_engine
)

add_test(
    NAME engine.lua_runtime
    COMMAND ${DSE_LUA_RUNTIME_TEST_TARGET}
)

set_tests_properties(engine.lua_runtime PROPERTIES
    LABELS "engine;unit;lua_runtime"
    PASS_REGULAR_EXPRESSION "All tests passed"
)
