# ============================================================================
# dse_shader_compiler — 离线 shader 交叉编译工具
# ============================================================================
#
# 功能：GLSL 450 → SPIR-V → GLSL 430 / HLSL SM5
# 依赖：glslang (SPIR-V 生成), spirv-cross (交叉编译)
#
# 用法:
#   dse_shader_compiler --input-dir <path> --output-dir <path>
#                       [--target all|spv|glsl430|hlsl] [--embed]
# ============================================================================

cmake_minimum_required(VERSION 3.16)

if(NOT DSE_HAS_GLSLANG)
    message(WARNING "dse_shader_compiler requires glslang. Skipping.")
    return()
endif()

if(NOT DSE_HAS_SPIRV_CROSS)
    message(WARNING "dse_shader_compiler requires SPIRV-Cross. Skipping.")
    return()
endif()

add_executable(dse_shader_compiler shader_compiler.cpp)

target_include_directories(dse_shader_compiler PRIVATE
    ${DSE_GLSLANG_INCLUDE_DIR}
    ${DSE_SPIRV_CROSS_INCLUDE_DIR}
)

target_link_libraries(dse_shader_compiler PRIVATE
    glslang
    SPIRV
    glslang-default-resource-limits
    spirv-cross-core
    spirv-cross-glsl
    spirv-cross-hlsl
)

# Windows: link d3dcompiler for offline HLSL→DXBC compilation
if(WIN32)
    target_link_libraries(dse_shader_compiler PRIVATE d3dcompiler.lib)
    target_compile_definitions(dse_shader_compiler PRIVATE DSE_HAS_D3DCOMPILE=1)
endif()

if(MSVC)
    set_property(TARGET dse_shader_compiler PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    target_compile_options(dse_shader_compiler PRIVATE /utf-8)
    target_compile_definitions(dse_shader_compiler PRIVATE
        _CRT_STDIO_ISO_WIDE_SPECIFIERS=1
        _CRT_NONSTDC_NO_WARNINGS=1
        _CRT_DECLARE_NONSTDC_NAMES=1
    )
endif()

set_target_properties(dse_shader_compiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)
