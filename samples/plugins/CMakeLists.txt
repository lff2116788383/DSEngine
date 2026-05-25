# 示例插件 CMakeLists.txt
# 单独构建：cmake -S . -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release
cmake_minimum_required(VERSION 3.20)
project(dse_sample_plugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(sample_plugin SHARED sample_plugin.cpp)

# 需要访问编辑器插件 API 头文件和 ImGui 头文件
target_include_directories(sample_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../apps/editor_cpp/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../depends/imgui
)

# 输出到 plugins/ 目录
set_target_properties(sample_plugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/plugins"
)
