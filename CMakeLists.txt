cmake_minimum_required(VERSION 3.17)
project(DSEngine)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D GLM_SWIZZLE)#GLM开启重排，随意转换。 vec2 v2 = v4.xz();

add_definitions(-D USE_FMOD_STUDIO)#是否使用fmod studio，加载bank文件的形式。


# 设置输出目录为 bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)

if (MSVC)
    add_compile_options(/bigobj)
    add_compile_options(/wd4819)#忽略保存为unicode警告
    add_compile_options(/FS) # 修复多线程编译 PDB 写入冲突 C1041
endif ()



include(CMakeLists.txt.glfw)
include(CMakeLists.txt.FreeType)
include(CMakeLists.txt.fmod)
include(CMakeLists.txt.easy_profile_core)
include(CMakeLists.txt.lua_script)
include(CMakeLists.txt.luasocket)
include(CMakeLists.txt.Physx)
include(CMakeLists.txt.imgui)

#头文件目录
include_directories("depends")
include_directories("src")
include_directories("depends/spdlog/include")

include_directories("depends/spscqueue/include")
include_directories("depends/rttr-0.9.6/src")
include_directories("depends/luasocket-3.0.0/src")
include_directories("depends/glm")
include_directories("depends/glm_ext")
include_directories("depends/rapidjson")
include_directories("depends/eventpp/tests")
include_directories("depends/eventpp/include")
include_directories("depends/timetool")
include_directories("depends/entt")
include_directories("../depends/box2d/include")

file(GLOB_RECURSE engine_cpp "src/*.cpp")
# 移除已经被删除的组件目录中的文件，CMake GLOB会自动处理，但为了安全我们排除可能残留的引用
list(FILTER engine_cpp EXCLUDE REGEX "src/component/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/physics/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/renderer/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/ui/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/resource/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/audio/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/editor/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/lighting/.*")
list(FILTER engine_cpp EXCLUDE REGEX "src/core/.*")
# file(GLOB_RECURSE rttr_cpp "depends/rttr-0.9.6/src/rttr/*.cpp")
file(GLOB_RECURSE example_cpp examples/*.cpp)
file(GLOB_RECURSE box2d_cpp "../depends/box2d/src/*.cpp")

# add_executable(${PROJECT_NAME} ${glfw_sources} ${rttr_cpp} ${freetype_src} ${easy_profiler_core_source} ${lua_src} ${luasocket_c} ${imgui_src} ${box2d_cpp} ${engine_cpp} ${example_cpp})
add_executable(${PROJECT_NAME} ${glfw_sources} ${freetype_src} ${easy_profiler_core_source} ${lua_src} ${luasocket_c} ${imgui_src} ${box2d_cpp} ${engine_cpp} ${example_cpp})

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "_debug")

if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_definitions(-D _DEBUG)
endif ()

# 拷贝资源文件和DLL
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/examples
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples
)