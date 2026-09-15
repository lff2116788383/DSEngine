set(DSE_GTEST_SMOKE_TARGET dse_gtest_smoke_tests)

add_executable(${DSE_GTEST_SMOKE_TARGET}
    rhi_pixel_harness.cpp
    skybox_pixel_smoke_test.cpp
    sprite_batch_pixel_smoke_test.cpp
    sprite_batch_sdf_pixel_smoke_test.cpp
    sprite_batch_vfx_pixel_smoke_test.cpp
    postprocess_pixel_smoke_test.cpp
    engine_core_services_smoke_test.cpp
    lua_host_smoke_test.cpp
    lua_lifecycle_smoke_test.cpp
    runtime_lifecycle_smoke_test.cpp
    gl_rhi_smoke_test.cpp
    vulkan_rhi_smoke_test.cpp
    dx11_rhi_smoke_test.cpp
    sprite_primitive_smoke_test.cpp
    instanced_ssbo_pixel_smoke_test.cpp
    instanced_vertex_rate_pixel_smoke_test.cpp
    bind_group_pixel_smoke_test.cpp
    stencil_pixel_smoke_test.cpp
    immediate_draw_pixel_smoke_test.cpp
    hair_pixel_smoke_test.cpp
    forward_pbr_pixel_smoke_test.cpp
    forward_shaded_pixel_smoke_test.cpp
    skinned_mesh_pixel_smoke_test.cpp
    skinned_shaded_pixel_smoke_test.cpp
    instanced_shaded_pixel_smoke_test.cpp
    skinned_instanced_shaded_pixel_smoke_test.cpp
    editor_view_mode_pixel_smoke_test.cpp
    gbuffer_mesh_pixel_smoke_test.cpp
    spot_shaded_pixel_smoke_test.cpp
    point_spot_shadow_shaded_pixel_smoke_test.cpp
    morph_shaded_pixel_smoke_test.cpp
    gpu_skinning_morph_compute_smoke_test.cpp
    danim_gpu_skinning_frame_consistency_test.cpp
    three_rhi_golden_scene_test.cpp
    three_rhi_soak_test.cpp
    shader_graph_preview_golden_test.cpp
    terrain_external_pixel_smoke_test.cpp
    tree_shared_template_pixel_smoke_test.cpp
    spine_unlit2d_pixel_smoke_test.cpp
    foliage_wind_pixel_smoke_test.cpp
    instanced_mesh_pixel_smoke_test.cpp
    depth_only_pixel_smoke_test.cpp
    indirect_mesh_pixel_smoke_test.cpp
    physics3d_smoke_test.cpp
    impostor_system_smoke_test.cpp
)

target_include_directories(${DSE_GTEST_SMOKE_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/modules
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
        ${CMAKE_SOURCE_DIR}/depends/glm
        ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps  # glad2 (<glad/gl.h>) for GL smoke tests
)

target_compile_features(${DSE_GTEST_SMOKE_TARGET} PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(${DSE_GTEST_SMOKE_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_GTEST_SMOKE_TARGET}
    PRIVATE
        GTest::gtest_main
        GTest::gmock
        dse_engine
)

# Vulkan 冒烟测试依赖（仅当 Vulkan 启用时添加头文件路径）
if(DSE_ENABLE_VULKAN AND DSE_HAS_VULKAN)
    if(DSE_VULKAN_HEADERS_FROM_SOURCE)
        target_include_directories(${DSE_GTEST_SMOKE_TARGET} PRIVATE ${DSE_VULKAN_INCLUDE_DIR})
        target_link_libraries(${DSE_GTEST_SMOKE_TARGET} PRIVATE Vulkan::Headers)
    endif()
    if(DSE_VULKAN_LOADER_FROM_SOURCE)
        target_link_libraries(${DSE_GTEST_SMOKE_TARGET} PRIVATE Vulkan::Loader)
    elseif(DSE_VULKAN_FROM_SYSTEM)
        target_include_directories(${DSE_GTEST_SMOKE_TARGET} PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(${DSE_GTEST_SMOKE_TARGET} PRIVATE Vulkan::Vulkan)
    endif()
    if(DSE_HAS_GLSLANG)
        target_include_directories(${DSE_GTEST_SMOKE_TARGET} PRIVATE
            ${DSE_GLSLANG_INCLUDE_DIR}
            ${DSE_GLSLANG_INCLUDE_DIR}/glslang/Include
            ${DSE_GLSLANG_INCLUDE_DIR}/SPIRV
        )
    endif()
endif()

# PhysX smoke 测试依赖（include + DLL 复制）
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    target_include_directories(${DSE_GTEST_SMOKE_TARGET} PRIVATE ${PHYSX_INCLUDE_DIR} ${PXSHARED_INCLUDE_DIR})
    add_custom_command(TARGET ${DSE_GTEST_SMOKE_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${PHYSX_DLLS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLLS_RELEASE}>"
            $<TARGET_FILE_DIR:${DSE_GTEST_SMOKE_TARGET}>
        COMMAND_EXPAND_LISTS
    )
endif()

# ---------------------------------------------------------------------------
# smoke 套件分层（托管 CI 回归网 / 真实 GPU 门禁）
# ---------------------------------------------------------------------------
# 背景：整个 smoke 二进制曾是「一个 ctest 测试 + gpu 标签」，于是托管（无 GPU）
# CI 用 -LE gpu 把 346 个用例全部排除 -> 常规 CI 对 smoke 层零覆盖。
#
# 现在按「是否构造真实 RHI/GPU 上下文」把用例分成两层，分类在配置期从源码自动
# 推导（不维护手工名单）：源文件引用 rhi_pixel_harness / *RhiDevice / rhi_device
# 即归入 gpu 层，否则归入 headless 层。
#
#   gtest.engine.smoke.headless : 无 GPU 依赖（core services / lua / physics3d /
#                                 runtime 生命周期），标签不含 gpu -> 托管 CI 实跑
#   gtest.engine.smoke.gpu      : 渲染/像素/RHI 冒烟，标签含 gpu -> 自托管 GPU
#                                 runner 以 ctest -L gpu 门禁
#
# 新增 smoke 源文件会被自动分类；若一个文件既无 TEST 宏也未匹配任何一侧，配置期
# 会打印 STATUS 行便于核对。
set(_smoke_gpu_suites "")
set(_smoke_headless_suites "")
file(GLOB _smoke_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(_smoke_src ${_smoke_sources})
    file(READ "${_smoke_src}" _smoke_src_text)
    string(REGEX MATCHALL "TEST(_F)?\\([ \t]*[A-Za-z0-9_]+" _smoke_decls "${_smoke_src_text}")
    if(_smoke_decls STREQUAL "")
        continue()
    endif()
    set(_smoke_file_suites "")
    foreach(_smoke_decl ${_smoke_decls})
        string(REGEX REPLACE "^TEST(_F)?\\([ \t]*" "" _smoke_suite "${_smoke_decl}")
        list(APPEND _smoke_file_suites "${_smoke_suite}")
    endforeach()
    if(_smoke_src_text MATCHES "rhi_pixel_harness|RhiDevice|rhi_device")
        list(APPEND _smoke_gpu_suites ${_smoke_file_suites})
    else()
        list(APPEND _smoke_headless_suites ${_smoke_file_suites})
    endif()
endforeach()
list(REMOVE_DUPLICATES _smoke_gpu_suites)
list(REMOVE_DUPLICATES _smoke_headless_suites)
list(SORT _smoke_gpu_suites)
list(SORT _smoke_headless_suites)

string(REPLACE ";" ":" _smoke_gpu_filter "${_smoke_gpu_suites}")
string(REPLACE ";" ":" _smoke_headless_filter "${_smoke_headless_suites}")
message(STATUS "smoke 分层: headless=[${_smoke_headless_filter}] gpu=[${_smoke_gpu_filter}]")

if(NOT _smoke_headless_filter STREQUAL "")
    add_test(
        NAME gtest.engine.smoke.headless
        COMMAND ${DSE_GTEST_SMOKE_TARGET} --gtest_filter=${_smoke_headless_filter}.*
    )
    set_tests_properties(gtest.engine.smoke.headless PROPERTIES
        LABELS "engine;smoke;gtest"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

add_test(
    NAME gtest.engine.smoke.gpu
    COMMAND ${DSE_GTEST_SMOKE_TARGET} --gtest_filter=${_smoke_gpu_filter}.*
)
# 渲染/像素冒烟：three-RHI golden/soak 与 shader-graph preview 显式断言
# device.is_software==false（真实 GPU 读回），托管 runner 无法通过，
# 因此保留 gpu 标签由自托管 runner 以 ctest -L gpu 门禁。
set_tests_properties(gtest.engine.smoke.gpu PROPERTIES
    LABELS "engine;smoke;gtest;gpu"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
