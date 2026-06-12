# DSEngine headless CLI（OUTPUT 名 dse）：建项目 / 打包加密 / 完整 build
add_executable(dse_cli
    "main.cpp"
)

target_link_libraries(dse_cli PRIVATE dse_engine)
target_include_directories(dse_cli PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(dse_cli PROPERTIES OUTPUT_NAME "dse")

if (MSVC)
    set_property(TARGET dse_cli PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(dse_cli PRIVATE /utf-8)
endif()

install(TARGETS dse_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
