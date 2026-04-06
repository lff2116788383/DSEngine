set(DSE_SPINE_TEST_TARGET dse_spine_tests)

add_executable(${DSE_SPINE_TEST_TARGET}
    ../engine/main.cpp
    spine_system_test.cpp
    ../modules/gameplay_2d/runtime_smoke_snapshot_test.cpp
)

target_include_directories(${DSE_SPINE_TEST_TARGET}
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/depends
)

target_compile_features(${DSE_SPINE_TEST_TARGET} PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(${DSE_SPINE_TEST_TARGET} PRIVATE /utf-8)
endif()

target_link_libraries(${DSE_SPINE_TEST_TARGET}
    PRIVATE
        dse_engine
)

add_test(
    NAME engine.spine
    COMMAND ${DSE_SPINE_TEST_TARGET} "[engine][unit][spine]"
)

set_tests_properties(engine.spine PROPERTIES
    LABELS "engine;unit;spine"
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "All tests passed"
)

add_test(
    NAME engine.spine.smoke
    COMMAND ${DSE_SPINE_TEST_TARGET} "[smoke][snapshot][spine]"
)

set_tests_properties(engine.spine.smoke PROPERTIES
    LABELS "engine;smoke;snapshot;spine"
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "All tests passed"
)

dse_copy_runtime_dlls(${DSE_SPINE_TEST_TARGET})