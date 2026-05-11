set(DSE_GTEST_UNIT_TARGET dse_gtest_unit_tests)

add_executable(${DSE_GTEST_UNIT_TARGET}
    # engine/base
    engine/base/time_test.cpp
    engine/base/bezier_test.cpp
    engine/base/tween_test.cpp
    engine/base/debug_test.cpp
    # engine/core
    engine/core/event_bus_test.cpp
    engine/core/event_id_test.cpp
    engine/core/event_id_cross_dll_test.cpp
    engine/core/service_locator_test.cpp
    engine/core/job_system_test.cpp
    engine/core/math_pool_test.cpp
    engine/core/dynamic_library_test.cpp
    engine/core/module_test.cpp
    # engine/ecs
    engine/ecs/world_test.cpp
    engine/ecs/ecs_component_test.cpp
    engine/ecs/animator_component_test.cpp
    engine/ecs/sprite_component_test.cpp
    engine/ecs/camera_component_test.cpp
    engine/ecs/tilemap_component_test.cpp
    engine/ecs/ui_component_test.cpp
    engine/ecs/script_component_test.cpp
    engine/ecs/audio_component_test.cpp
    engine/ecs/gameplay_test.cpp
    engine/ecs/components_3d_test.cpp
    engine/ecs/components_2d_test.cpp
    engine/ecs/components_3d_particle_test.cpp
    engine/ecs/components_3d_physics_test.cpp
    engine/ecs/components_3d_fracture_test.cpp
    engine/ecs/components_3d_cloth_test.cpp
    engine/ecs/components_3d_fluid_test.cpp
    engine/ecs/particle_curve_test.cpp
    # engine/input
    engine/input/input_test.cpp
    # engine/audio
    engine/audio/audio_system_test.cpp
    engine/audio/audio_3d_spatial_test.cpp
    # engine/physics
    engine/physics/physics3d_system_test.cpp
    engine/physics/physics2d_joint_test.cpp
    # engine/render
    engine/render/render_graph_test.cpp
    engine/render/rhi_types_test.cpp
    engine/render/vulkan_rhi_test.cpp
    engine/render/dx11_rhi_test.cpp
    # engine/scene
    engine/scene/octree_test.cpp
    engine/scene/quad_tree_test.cpp
    engine/scene/transform_system_test.cpp
    engine/scene/raw_scene_data_test.cpp
    engine/scene/sub_scene_test.cpp
    engine/scene/scene_manager_test.cpp
    engine/scene/scene_transition_test.cpp
    engine/scene/uuid_cross_scene_test.cpp
    # engine/assets
    engine/assets/asset_manager_test.cpp
    engine/assets/asset_pipeline_async_test.cpp
    # engine/profiler
    engine/profiler/profiler_test.cpp
    # engine/runtime
    engine/runtime/frame_pipeline_test.cpp
    engine/runtime/runtime_context_test.cpp
    engine/runtime/engine_app_test.cpp
    # modules/gameplay_2d
    modules/gameplay_2d/animation_system_2d_test.cpp
    modules/gameplay_2d/camera_system_2d_test.cpp
    modules/gameplay_2d/particle_system_2d_test.cpp
    modules/gameplay_2d/tilemap_system_test.cpp
    modules/gameplay_2d/localization_system_test.cpp
    # modules/gameplay_3d（AnimationStateMachine 已编译进 dse_engine）
    modules/gameplay_3d/animation_state_machine_test.cpp
)

# 3D 模块测试需要DSE_Gameplay3D 库（条件编译）
if(DSE_ENABLE_3D)
    target_sources(${DSE_GTEST_UNIT_TARGET} PRIVATE
        modules/gameplay_3d/steering_system_test.cpp
        modules/gameplay_3d/frustum_culling_system_test.cpp
        modules/gameplay_3d/animator_system_test.cpp
        modules/gameplay_3d/free_camera_controller_system_test.cpp
    )
endif()

target_include_directories(${DSE_GTEST_UNIT_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/modules
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
        ${CMAKE_SOURCE_DIR}/depends/glm
)

target_compile_features(${DSE_GTEST_UNIT_TARGET} PRIVATE cxx_std_20)

# Vulkan include paths (for vulkan_rhi_test.cpp)
if(DSE_ENABLE_VULKAN AND DSE_HAS_VULKAN)
    target_include_directories(${DSE_GTEST_UNIT_TARGET} PRIVATE
        ${DSE_VULKAN_INCLUDE_DIR}
    )
endif()
# D3D11 compile definition propagation (for dx11_rhi_test.cpp)
if(DSE_ENABLE_D3D11 AND WIN32)
    target_compile_definitions(${DSE_GTEST_UNIT_TARGET} PRIVATE DSE_ENABLE_D3D11)
endif()
if(MSVC)
    target_compile_options(${DSE_GTEST_UNIT_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_GTEST_UNIT_TARGET}
    PRIVATE
        GTest::gtest_main
        GTest::gmock
        dse_engine
)

# DSE_Gameplay3D 已静态编入 dse_engine，无需额外链接

# PhysX 运行时DLL 复制（仅当PhysX 已启用且库已找到时）
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_custom_command(TARGET ${DSE_GTEST_UNIT_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${PHYSX_DLLS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLLS_RELEASE}>"
            $<TARGET_FILE_DIR:${DSE_GTEST_UNIT_TARGET}>
        COMMAND_EXPAND_LISTS
    )
endif()

add_test(
    NAME gtest.engine.unit
    COMMAND ${DSE_GTEST_UNIT_TARGET}
)
set_tests_properties(gtest.engine.unit PROPERTIES
    LABELS "engine;unit;gtest"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
