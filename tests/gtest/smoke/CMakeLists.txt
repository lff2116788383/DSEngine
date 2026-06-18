set(DSE_GTEST_SMOKE_TARGET dse_gtest_smoke_tests)

add_executable(${DSE_GTEST_SMOKE_TARGET}
    rhi_pixel_harness.cpp
    skybox_pixel_smoke_test.cpp
    sprite_batch_pixel_smoke_test.cpp
    sprite_batch_sdf_pixel_smoke_test.cpp
    sprite_batch_vfx_pixel_smoke_test.cpp
    engine_core_services_smoke_test.cpp
    lua_host_smoke_test.cpp
    lua_lifecycle_smoke_test.cpp
    runtime_lifecycle_smoke_test.cpp
    gl_rhi_smoke_test.cpp
    vulkan_rhi_smoke_test.cpp
    dx11_rhi_smoke_test.cpp
    sprite_primitive_smoke_test.cpp
    instanced_ssbo_pixel_smoke_test.cpp
    forward_pbr_pixel_smoke_test.cpp
    skinned_mesh_pixel_smoke_test.cpp
    physics3d_smoke_test.cpp
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

add_test(
    NAME gtest.engine.smoke
    COMMAND ${DSE_GTEST_SMOKE_TARGET}
)
set_tests_properties(gtest.engine.smoke PROPERTIES
    LABELS "engine;smoke;gtest"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
