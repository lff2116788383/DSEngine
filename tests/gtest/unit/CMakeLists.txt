set(DSE_GTEST_UNIT_TARGET dse_gtest_unit_tests)

# 公共测试：engine / gameplay_2d / apps（目录内新增文件自动发现）
file(GLOB_RECURSE DSE_UNIT_COMMON_SRCS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/gameplay_2d/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/apps/*.cpp"
)

add_executable(${DSE_GTEST_UNIT_TARGET}
    ${DSE_UNIT_COMMON_SRCS}
    # editor_test_harness 位于测试目录外，保持显式引用
    ${CMAKE_SOURCE_DIR}/apps/editor_cpp/src/editor_test_harness.cpp
)

# 3D 模块测试（全部依赖 gameplay_3d 头文件，含 AnimationStateMachine / FootIK 等）
if(DSE_ENABLE_3D)
    file(GLOB_RECURSE DSE_UNIT_3D_SRCS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/modules/gameplay_3d/*.cpp"
    )
    target_sources(${DSE_GTEST_UNIT_TARGET} PRIVATE ${DSE_UNIT_3D_SRCS})
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
