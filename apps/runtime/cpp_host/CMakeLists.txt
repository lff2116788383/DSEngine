add_executable(DSEngine_example_cpp
    "main.cpp"
    "${CMAKE_SOURCE_DIR}/samples/cpp/phase1_demo_logic.cpp"
)

target_link_libraries(DSEngine_example_cpp PRIVATE dse_engine)
set_target_properties(DSEngine_example_cpp PROPERTIES OUTPUT_NAME "dsengine_cpp")
dse_apply_config_postfix(DSEngine_example_cpp)

if (MSVC)
    set_property(TARGET DSEngine_example_cpp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(DSEngine_example_cpp PRIVATE /utf-8)
endif()


dse_copy_runtime_assets(DSEngine_example_cpp)

# PhysX 运行时 DLL 复制（仅当 PhysX 已启用且库已找到时）
if(DSE_ENABLE_PHYSX AND DSE_HAS_PHYSX_LIBS)
    add_custom_command(TARGET DSEngine_example_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${PHYSX_DLLS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${PHYSX_DLLS_RELEASE}>"
            $<TARGET_FILE_DIR:DSEngine_example_cpp>
        COMMAND_EXPAND_LISTS
    )
endif()

install(TARGETS DSEngine_example_cpp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
