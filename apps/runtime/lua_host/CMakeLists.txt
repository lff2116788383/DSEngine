add_executable(dse_example_lua
    "main.cpp"
)

target_link_libraries(dse_example_lua PRIVATE dse_engine)
set_target_properties(dse_example_lua PROPERTIES OUTPUT_NAME "DSEngine_lua")
dse_apply_config_postfix(dse_example_lua)

if (MSVC)
    set_property(TARGET dse_example_lua PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

dse_copy_runtime_assets(dse_example_lua)

install(TARGETS dse_example_lua
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
