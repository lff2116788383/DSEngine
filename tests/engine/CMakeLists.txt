set(DSE_ENGINE_TEST_TARGET dse_engine_unit_tests)

set(DSE_ENGINE_TEST_SOURCES
    main.cpp
    base/tween_test.cpp
    base/time_test.cpp
    input/input_test.cpp
    core/dynamic_library_test.cpp
    core/event_bus_test.cpp
    core/job_system_test.cpp
    core/memory_pool_test.cpp
    core/object_pool_test.cpp
    ecs/world_test.cpp
    scene/transform_system_test.cpp
    scene/prefab_test.cpp
    scene/octree_test.cpp
    scene/scene_flow_test.cpp
    scene/editor_scene_io_bridge_test.cpp
    runtime/runtime_access_test.cpp
    runtime/frame_pipeline_static_regression_test.cpp
    cpp_runtime_startup_scene_test.cpp
    mesh_material_desc_test.cpp
    mesh_render_material_desc_static_test.cpp
    scripting/cpp_business_runtime_test.cpp
    ../modules/gameplay_2d/camera/camera_system_test.cpp
    ../modules/gameplay_2d/animation/animation_system_test.cpp
    ../modules/gameplay_2d/localization/localization_system_test.cpp
    ../modules/gameplay_2d/localization/font_manager_test.cpp
    ../modules/gameplay_2d/particle/particle_system_test.cpp
    ../modules/gameplay_2d/particle/particle_advanced_test.cpp
    ../modules/gameplay_2d/tilemap/tilemap_system_test.cpp
    ../modules/gameplay_2d/smoke_snapshot_test.cpp
    ../modules/gameplay_2d/ui/ui_system_test.cpp
    ../modules/gameplay_2d/ui/ui_layout_test.cpp
    ../modules/gameplay_2d/ui/ui_animation_test.cpp
    ../modules/gameplay_2d/ui/ui_advanced_test.cpp
    ../modules/gameplay_2d/physics/physics2d_advanced_test.cpp
    ../modules/gameplay_2d/rendering/sprite_render_system_test.cpp
    ../modules/gameplay_2d/tilemap/tilemap_advanced_test.cpp
    ../modules/gameplay_2d/ui/ui_deep_state_test.cpp
    assets/asset_manager_test.cpp
    assets/importer_cooker_test.cpp
    build_generation_guard_test.cpp
    runtime_host_integration_test.cpp
    audio/audio_system_test.cpp
    render/command_buffer_test.cpp
    render/rhi_device_test.cpp
    render/ui_render_system_test.cpp
    profiler/cpu_profiler_test.cpp
    profiler/memory_profiler_test.cpp
    profiler/render_profiler_test.cpp
    platform/screen_test.cpp
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
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_scene_io.cpp
    ${CMAKE_SOURCE_DIR}/engine/scripting/cpp/cpp_business_runtime.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/camera/camera_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/animation/animation_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/localization/localization_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/localization/font_manager.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/particle/particle_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/tilemap/tilemap_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/rendering/sprite_render_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/ui/ui_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/ui/ui_layout.cpp
    ${CMAKE_SOURCE_DIR}/engine/assets/asset_manager.cpp
    ${CMAKE_SOURCE_DIR}/engine/profiler/cpu_profiler.cpp
    ${CMAKE_SOURCE_DIR}/engine/profiler/memory_profiler.cpp
    ${CMAKE_SOURCE_DIR}/engine/profiler/render_profiler.cpp
    ${CMAKE_SOURCE_DIR}/engine/physics/physics2d/physics2d_system.cpp
    ${CMAKE_SOURCE_DIR}/engine/platform/screen.cpp
    ${CMAKE_SOURCE_DIR}/engine/render/rhi/rhi_device.cpp
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps/glad_gl.c
    ${CMAKE_SOURCE_DIR}/depends/tiny-AES-c/aes.c
    ${CMAKE_SOURCE_DIR}/depends/bundle/bundle.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_3d/rendering/frustum_culling_system.cpp
    ${CMAKE_SOURCE_DIR}/modules/gameplay_3d/ai/steering_system.cpp
)

if(DSE_ENABLE_SPINE)
    list(APPEND DSE_ENGINE_TEST_SOURCES
        ../modules/gameplay_2d/spine/spine_system_test.cpp
        ../modules/gameplay_2d/spine/spine_system_more_test.cpp
        ${CMAKE_SOURCE_DIR}/modules/gameplay_2d/spine/spine_system.cpp
        ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/Attachment.cpp
        ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/VertexAttachment.cpp
        ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/MeshAttachment.cpp
        ${CMAKE_SOURCE_DIR}/depends/spine-runtimes/spine-cpp/spine-cpp/src/spine/RegionAttachment.cpp
    )
endif()

add_executable(${DSE_ENGINE_TEST_TARGET} ${DSE_ENGINE_TEST_SOURCES})

target_include_directories(${DSE_ENGINE_TEST_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/tests
)

target_compile_features(${DSE_ENGINE_TEST_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_ENGINE_TEST_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_ENGINE_TEST_TARGET}
    PRIVATE
        box2d
        dse_engine
        opengl32
)

if(DSE_HAS_ASSIMP)
    target_link_libraries(${DSE_ENGINE_TEST_TARGET}
        PRIVATE
            assimp
    )
endif()

add_test(
    NAME engine.unit
    COMMAND ${DSE_ENGINE_TEST_TARGET}
)

set_tests_properties(engine.unit PROPERTIES
    LABELS "engine;unit"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME engine.2d.ui
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[ui],[smoke][snapshot][ui]"
)
set_tests_properties(engine.2d.ui PROPERTIES LABELS "engine;unit;2d;ui;smoke;snapshot")

add_test(
    NAME engine.2d.tilemap
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[tilemap],[smoke][snapshot][tilemap]"
)
set_tests_properties(engine.2d.tilemap PROPERTIES LABELS "engine;unit;2d;tilemap;smoke;snapshot")

add_test(
    NAME engine.2d.particle
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[particle],[smoke][snapshot][particle]"
)
set_tests_properties(engine.2d.particle PROPERTIES LABELS "engine;unit;2d;particle;smoke;snapshot")

add_test(
    NAME engine.2d.physics2d
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[physics2d],[smoke][snapshot][physics2d]"
)
set_tests_properties(engine.2d.physics2d PROPERTIES LABELS "engine;unit;2d;physics2d;smoke;snapshot")

add_test(
    NAME engine.2d.spine
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[spine]"
)
set_tests_properties(engine.2d.spine PROPERTIES LABELS "engine;unit;2d;spine;smoke;snapshot")

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

add_test(
    NAME engine.3d.unit
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[mesh_renderer],[terrain],[csm],[camera_3d],[animator3d],[steering],[culling]"
)
set_tests_properties(engine.3d.unit PROPERTIES LABELS "engine;unit;3d")

add_test(
    NAME engine.3d.scene_mvp
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[scene][3d][mvp]"
)
set_tests_properties(engine.3d.scene_mvp PROPERTIES
    LABELS "engine;unit;3d;scene;mvp;gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "All tests passed"
)

add_test(
    NAME engine.3d.smoke
    COMMAND $<TARGET_FILE:DSEngine_example_cpp>
)
set_tests_properties(engine.3d.smoke PROPERTIES
    LABELS "engine;smoke;3d"
    ENVIRONMENT "DSE_MAX_FRAMES=120"
    PASS_REGULAR_EXPRESSION "\\[3D-Smoke\\]\\[CPP\\] bootstrap_ok"
)

add_test(
    NAME engine.3d.runtime_mvp_smoke
    COMMAND $<TARGET_FILE:DSEngine_example_cpp>
)
set_tests_properties(engine.3d.runtime_mvp_smoke PROPERTIES
    LABELS "engine;smoke;3d;runtime;mvp;gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "DSE_STARTUP_SCENE=assets/scenes/3d_mvp_minimal.scene.json;DSE_MAX_FRAMES=120"
    PASS_REGULAR_EXPRESSION "\\[3D-Smoke\\]\\[CPP\\] startup_scene_loaded path=assets/scenes/3d_mvp_minimal.scene.json"
    TIMEOUT 20
)

add_dependencies(${DSE_ENGINE_TEST_TARGET} DSEngine_example_cpp)

set(DSE_LUA_RUNTIME_TEST_TARGET dse_lua_runtime_tests)

add_executable(${DSE_LUA_RUNTIME_TEST_TARGET}
    main.cpp
    scripting/lua_runtime_test.cpp
    runtime_smoke_snapshot_test.cpp
)

target_include_directories(${DSE_LUA_RUNTIME_TEST_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/tests
)

target_compile_features(${DSE_LUA_RUNTIME_TEST_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_LUA_RUNTIME_TEST_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_LUA_RUNTIME_TEST_TARGET}
    PRIVATE
        dse_engine
)

set(DSE_LUA_RUNTIME_CORE_SINGLE_TARGET dse_lua_runtime_core_single_test)
add_executable(${DSE_LUA_RUNTIME_CORE_SINGLE_TARGET}
    main.cpp
    lua_runtime_core_single_test.cpp
)
target_include_directories(${DSE_LUA_RUNTIME_CORE_SINGLE_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/tests
)
target_compile_features(${DSE_LUA_RUNTIME_CORE_SINGLE_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_LUA_RUNTIME_CORE_SINGLE_TARGET} PRIVATE /utf-8)
endif()
target_link_libraries(${DSE_LUA_RUNTIME_CORE_SINGLE_TARGET} PRIVATE dse_engine)

set(DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET dse_lua_runtime_smoke_single_test)
add_executable(${DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET}
    main.cpp
    lua_runtime_smoke_single_test.cpp
)
target_include_directories(${DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/tests
)
target_compile_features(${DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET} PRIVATE /utf-8)
endif()
target_link_libraries(${DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET} PRIVATE dse_engine)

set(DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET dse_lua_resource_injection_single_test)
add_executable(${DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET}
    main.cpp
    lua_resource_injection_test.cpp
)
target_include_directories(${DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/tests
)
target_compile_features(${DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET} PRIVATE /utf-8)
endif()
target_link_libraries(${DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET} PRIVATE dse_engine)

add_test(
    NAME engine.lua_runtime
    COMMAND ${DSE_LUA_RUNTIME_CORE_SINGLE_TARGET}
)
set_tests_properties(engine.lua_runtime PROPERTIES
    LABELS "engine;unit;lua_runtime;gate"
    PASS_REGULAR_EXPRESSION "All tests passed"
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    TIMEOUT 15
)

add_test(
    NAME engine.lua_runtime.smoke
    COMMAND ${DSE_LUA_RUNTIME_SMOKE_SINGLE_TARGET}
)
set_tests_properties(engine.lua_runtime.smoke PROPERTIES
    LABELS "engine;smoke;snapshot;lua_runtime"
    PASS_REGULAR_EXPRESSION "All tests passed"
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    TIMEOUT 15
)

add_test(
    NAME engine.resource_injection
    COMMAND ${DSE_LUA_RESOURCE_INJECTION_SINGLE_TARGET}
)
set_tests_properties(engine.resource_injection PROPERTIES
    LABELS "engine;unit;resource_injection;gate"
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "All tests passed"
)

add_test(
    NAME engine.build_generation
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[build_generation]"
)

set_tests_properties(engine.build_generation PROPERTIES
    LABELS "engine;unit;build_generation;vcpkg;gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "All tests passed"
)

add_test(
    NAME engine.cpp_runtime
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[cpp_runtime]"
)

set_tests_properties(engine.cpp_runtime PROPERTIES
    LABELS "engine;unit;cpp_runtime;resource_injection;gate"
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "All tests passed"
)

add_test(
    NAME engine.runtime_host.integration
    COMMAND ${DSE_ENGINE_TEST_TARGET} "[runtime_host]"
)

set_tests_properties(engine.runtime_host.integration PROPERTIES
    LABELS "engine;smoke;integration;runtime_host;gate"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    TIMEOUT 30
    PASS_REGULAR_EXPRESSION "All tests passed"
)

add_dependencies(${DSE_ENGINE_TEST_TARGET} DSEngine_example_cpp dse_example_lua)

dse_copy_runtime_dlls(${DSE_ENGINE_TEST_TARGET})
dse_copy_runtime_dlls(${DSE_LUA_RUNTIME_TEST_TARGET})
