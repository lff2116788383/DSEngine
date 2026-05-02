set(DSE_GTEST_SMOKE_TARGET dse_gtest_smoke_tests)

add_executable(${DSE_GTEST_SMOKE_TARGET}
    engine_core_services_smoke_test.cpp
)

target_include_directories(${DSE_GTEST_SMOKE_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
        ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
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

# PhysX 运行时 DLL 复制（仅当 PhysX 已启用且库已找到时）
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_custom_command(TARGET ${DSE_GTEST_SMOKE_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PHYSX_DLLS_DEBUG}
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
