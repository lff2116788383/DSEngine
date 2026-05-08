add_executable(dse_example_lua
    "main.cpp"
)

target_link_libraries(dse_example_lua PRIVATE dse_engine)
set_target_properties(dse_example_lua PROPERTIES OUTPUT_NAME "DSEngine_lua")
dse_apply_config_postfix(dse_example_lua)

if (MSVC)
    set_property(TARGET dse_example_lua PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(dse_example_lua PRIVATE /utf-8)
endif()


dse_copy_runtime_assets(dse_example_lua)

# PhysX 运行时 DLL 复制（仅当 PhysX 已启用且库已找到时）
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_custom_command(TARGET dse_example_lua POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${PHYSX_DLLS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLLS_RELEASE}>"
            $<TARGET_FILE_DIR:dse_example_lua>
        COMMAND_EXPAND_LISTS
    )
endif()

install(TARGETS dse_example_lua
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
