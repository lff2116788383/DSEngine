cmake_minimum_required(VERSION 3.16)

add_executable(dse_dssl_compiler
    dssl_compiler.cpp
    dssl_parser.cpp
    dssl_codegen.cpp
)

if(MSVC)
    set_property(TARGET dse_dssl_compiler PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(dse_dssl_compiler PRIVATE /utf-8)
endif()

set_target_properties(dse_dssl_compiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)
