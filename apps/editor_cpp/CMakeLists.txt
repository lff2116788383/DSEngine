# ─── 可选：内嵌 Dear ImGui Test Engine 的真实控件级 UI 测试 ──────────────────
# 默认 OFF → 生产二进制零影响（不编入测试引擎、不改默认产物名）。
# ON 时把 test engine 源码编进编辑器，产出独立的 dsengine-editor-uitest，与常规
# dsengine-editor（供 editor-api 套件用）并存，互不覆盖。
option(DSE_EDITOR_UI_TESTS "Build editor with embedded Dear ImGui Test Engine (--run-ui-tests)" OFF)

# ─── IXWebSocket (WebSocket server for Control Server / MCP) ────────────────
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/depends/IXWebSocket ${CMAKE_CURRENT_BINARY_DIR}/IXWebSocket)

set(DSE_GLAD_SOURCE ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps/glad_gl.c)
if(NOT EXISTS ${DSE_GLAD_SOURCE})
    set(DSE_GLAD_SOURCE ${CMAKE_SOURCE_DIR}/depends/glad/src/glad.c)
endif()

# 编辑器源文件（src/ 目录内新增面板无需修改此文件）
file(GLOB_RECURSE editor_srcs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(dse_editor_cpp WIN32
    ${editor_srcs}
    # Windows 应用程序图标资源
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/editor.rc>
    # 以下为 src/ 目录外的第三方源文件，保留显式引用
    ${DSE_GLAD_SOURCE}
    ${CMAKE_SOURCE_DIR}/depends/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/depends/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/depends/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/depends/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imguizmo/ImGuizmo.cpp
)

if(MSVC)
    set_target_properties(dse_editor_cpp PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    target_compile_options(dse_editor_cpp PRIVATE /utf-8 /Zc:preprocessor)
    set_property(TARGET dse_editor_cpp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_include_directories(dse_editor_cpp PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/engine/runtime
    ${CMAKE_SOURCE_DIR}/engine/scene
    ${CMAKE_SOURCE_DIR}/depends/entt-3.13.0/src
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/include
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps
    ${CMAKE_SOURCE_DIR}/depends/glad/include
    ${CMAKE_SOURCE_DIR}/depends/imgui
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends
    ${CMAKE_SOURCE_DIR}/third_party/imguizmo
    ${CMAKE_SOURCE_DIR}/depends/glm
    ${CMAKE_SOURCE_DIR}/depends/glm_ext
    ${CMAKE_SOURCE_DIR}/depends/spdlog/include
    ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include
    ${CMAKE_SOURCE_DIR}/depends
)

target_link_libraries(dse_editor_cpp PRIVATE dse_engine dse_editor_core ixwebsocket opengl32 comdlg32 crypt32)

# ─── Dear ImGui Test Engine 接线（仅 DSE_EDITOR_UI_TESTS=ON） ─────────────────
if(DSE_EDITOR_UI_TESTS)
    set(DSE_IMGUI_TE_DIR ${CMAKE_SOURCE_DIR}/depends/imgui_test_engine/imgui_test_engine)
    # 测试引擎 8 个源文件（与 src/ 内的用例一起编进 dse_editor_cpp）。
    target_sources(dse_editor_cpp PRIVATE
        ${DSE_IMGUI_TE_DIR}/imgui_te_engine.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_te_context.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_te_coroutine.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_te_exporters.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_te_perftool.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_te_ui.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_te_utils.cpp
        ${DSE_IMGUI_TE_DIR}/imgui_capture_tool.cpp
    )
    target_include_directories(dse_editor_cpp PRIVATE
        ${CMAKE_SOURCE_DIR}/depends/imgui_test_engine
        ${DSE_IMGUI_TE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_tests
    )
    # DSE_EDITOR_UI_TESTS 打开用例代码；IMGUI_USER_CONFIG 必须对整个 target 生效
    # （含 imgui.cpp / ImGuizmo），否则 ImGuiContext/ImGuiIO 的 ABI 在两侧不一致。
    target_compile_definitions(dse_editor_cpp PRIVATE
        DSE_EDITOR_UI_TESTS
        IMGUI_USER_CONFIG="dse_imconfig.h"
    )
endif()

# Animation Retargeting 面板需要在编辑器内导入 gltf/fbx 骨架并烘焙 .danim，
# 复用资源编译核心 importer.cpp（默认仅编进 AssetBuilder）。需要 assimp 与 tinygltf。
if(DSE_HAS_ASSIMP)
    target_sources(dse_editor_cpp PRIVATE ${CMAKE_SOURCE_DIR}/engine/assets/compiler/importer.cpp)
    target_include_directories(dse_editor_cpp PRIVATE ${CMAKE_SOURCE_DIR}/depends/tinygltf)
    target_link_libraries(dse_editor_cpp PRIVATE assimp)
    target_compile_definitions(dse_editor_cpp PRIVATE DSE_HAS_ASSIMP=1 DSE_EDITOR_HAS_IMPORTER=1)
endif()

# 将编译产物放到 bin 目录下。UI 测试构建用独立产物名，与常规编辑器并存不覆盖。
if(DSE_EDITOR_UI_TESTS)
    set(DSE_EDITOR_OUTPUT_NAME "dsengine-editor-uitest")
else()
    set(DSE_EDITOR_OUTPUT_NAME "dsengine-editor")
endif()
set_target_properties(dse_editor_cpp PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    OUTPUT_NAME "${DSE_EDITOR_OUTPUT_NAME}"
)

# 确保构建编辑器时同步构建 AssetBuilder 和 standalone runtime，
# 让 bin/ 目录里的工具链开箱可用，不需要开发者手动触发额外目标。
if(TARGET AssetBuilder)
    add_dependencies(dse_editor_cpp AssetBuilder)
endif()
if(TARGET dse_standalone)
    add_dependencies(dse_editor_cpp dse_standalone)
endif()

# Ship editor fonts next to the executable so the packaged/installed editor
# can render CJK glyphs (NotoSansSC) instead of garbled text.
add_custom_command(TARGET dse_editor_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:dse_editor_cpp>/fonts"
    COMMENT "Copying editor fonts next to dsengine-editor"
)
