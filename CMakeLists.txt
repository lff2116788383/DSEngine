cmake_minimum_required(VERSION 3.17)
project(DSEngine)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-D GLM_SWIZZLE)#GLM开启重排，随意转换。 vec2 v2 = v4.xz();

add_definitions(-D USE_FMOD_STUDIO)#是否使用fmod studio，加载bank文件的形式。


if (MSVC)
    add_compile_options(/bigobj)
    add_compile_options(/wd4819)#忽略保存为unicode警告
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

file(GLOB_RECURSE engine_cpp src/*.cpp)
file(GLOB_RECURSE rttr_cpp "depends/rttr-0.9.6/src/rttr/*.cpp")
file(GLOB_RECURSE example_cpp examples/*.cpp)

add_executable(${PROJECT_NAME} ${glfw_sources} ${rttr_cpp} ${freetype_src} ${easy_profiler_core_source} ${lua_src} ${luasocket_c} ${imgui_src}  ${engine_cpp} ${example_cpp})

if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_definitions(-D _DEBUG)
endif ()