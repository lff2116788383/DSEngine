add_executable(DSEngine_example_cpp
    "main.cpp"
    "${CMAKE_SOURCE_DIR}/samples/cpp/phase1_demo_logic.cpp"
)

target_link_libraries(DSEngine_example_cpp PRIVATE dse_engine)
set_target_properties(DSEngine_example_cpp PROPERTIES OUTPUT_NAME "DSEngine_c++")
dse_apply_config_postfix(DSEngine_example_cpp)

if (MSVC)
    set_property(TARGET DSEngine_example_cpp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

dse_copy_runtime_assets(DSEngine_example_cpp)

install(TARGETS DSEngine_example_cpp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
