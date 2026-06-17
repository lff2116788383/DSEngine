add_executable(dse_standalone
    "main.cpp"
    # Windows 应用程序图标资源
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/standalone.rc>
)

target_link_libraries(dse_standalone PRIVATE dse_engine)
set_target_properties(dse_standalone PROPERTIES OUTPUT_NAME "dsengine_game")
dse_apply_config_postfix(dse_standalone)

if (MSVC)
    set_property(TARGET dse_standalone PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(dse_standalone PRIVATE /utf-8)
endif()

dse_copy_runtime_assets(dse_standalone)

# PhysX runtime DLLs
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_custom_command(TARGET dse_standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${PHYSX_DLLS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLLS_RELEASE}>"
            $<TARGET_FILE_DIR:dse_standalone>
        COMMAND_EXPAND_LISTS
    )
endif()

install(TARGETS dse_standalone
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
