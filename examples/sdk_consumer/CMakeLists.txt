cmake_minimum_required(VERSION 3.17)
project(DSEngineConsumerExample CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /wd4819)
endif()

# ── 查找 SDK ──────────────────────────────────────────────────────
# 使用方式：cmake -B build -DCMAKE_PREFIX_PATH=<sdk_install_dir> ..
find_package(DSEngine REQUIRED)

message(STATUS "Found DSEngine ${DSEngine_VERSION}")
message(STATUS "  Include dirs: ${DSEngine_INCLUDE_DIRS}")
message(STATUS "  Libraries:    ${DSEngine_LIBRARIES}")

# ── 最小示例程序（链接 + 头文件边界验证）────────────────────────────
add_executable(consumer_example main.cpp)
target_link_libraries(consumer_example PRIVATE DSEngine::dse_engine)

# 将运行时 DLL 复制到可执行文件目录
add_custom_command(TARGET consumer_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:consumer_example>
    $<TARGET_FILE_DIR:consumer_example>
    COMMAND_EXPAND_LISTS
)

# ── 完整 Demo：C++ 宿主 + Lua 3D 物理场景 ───────────────────────────
# 经 dse::runtime::RunEngine(BusinessMode::Lua) 启动引擎并加载 demo_scene.lua，
# 证明仅凭 SDK 公共头即可搭建一个真实的「3D + Lua + Jolt 物理」游戏宿主。
add_executable(consumer_game game_main.cpp)
target_link_libraries(consumer_game PRIVATE DSEngine::dse_engine)

# 运行时 DLL + 启动脚本拷到 exe 旁（运行还需 data/ 着色器，见 README）。
add_custom_command(TARGET consumer_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:consumer_game>
    $<TARGET_FILE_DIR:consumer_game>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/demo_scene.lua"
    $<TARGET_FILE_DIR:consumer_game>
    COMMAND_EXPAND_LISTS
)
