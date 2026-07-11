# ─── 可选：内嵌 Dear ImGui Test Engine 的真实控件级 UI 测试 ──────────────────
# 默认 OFF → 生产二进制零影响（不编入测试引擎、不改默认产物名）。
# ON 时把 test engine 源码编进编辑器，产出独立的 dsengine-editor-uitest，与常规
# dsengine-editor（供 editor-api 套件用）并存，互不覆盖。
option(DSE_EDITOR_UI_TESTS "Build editor with embedded Dear ImGui Test Engine (--run-ui-tests)" OFF)


# ─── 可选：一键发布功能（Web 构建产物上传到 DSE 服务器）─────────────────────────
# 默认 OFF → 离线模式（仅导出 zip，不上传）。ON 时链接 libcurl，启用在线上传功能。
option(DSE_PUBLISH_ENABLED "Enable online publish feature (requires libcurl)" OFF)

# ─── IXWebSocket (WebSocket server for Control Server / MCP) ────────────────
# 当 DSE_ENABLE_HTTP=ON 时，cmake/CMakeLists.txt.http 已提前 add_subdirectory 了
# 同一个 IXWebSocket（带 TLS=OpenSSL）。这里仅在尚未定义 ixwebsocket 目标时才自行加入，
# 避免重复 add_library(ixwebsocket) 冲突（HTTP + 编辑器同时启用时）。
if(NOT TARGET ixwebsocket)
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    set(USE_ZLIB OFF CACHE BOOL "" FORCE)
    set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/depends/IXWebSocket ${CMAKE_CURRENT_BINARY_DIR}/IXWebSocket)
endif()

set(DSE_GLAD_SOURCE ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps/glad_gl.c)
if(NOT EXISTS ${DSE_GLAD_SOURCE})
    set(DSE_GLAD_SOURCE ${CMAKE_SOURCE_DIR}/depends/glad/src/glad.c)
endif()

# 编辑器源文件（src/ 目录内新增面板无需修改此文件）
file(GLOB_RECURSE editor_srcs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Exclude ui_tests/ sources when not building with Test Engine
if(NOT DSE_EDITOR_UI_TESTS)
    list(FILTER editor_srcs EXCLUDE REGEX "ui_tests/")
endif()

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
    $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_dx11.cpp>
    $<$<BOOL:${DSE_ENABLE_VULKAN}>:${CMAKE_SOURCE_DIR}/depends/imgui/backends/imgui_impl_vulkan.cpp>
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
    $<$<BOOL:${DSE_ENABLE_VULKAN}>:${CMAKE_SOURCE_DIR}/depends/Vulkan-Headers/include>
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
        DSE_RENDER_TESTS
        IMGUI_USER_CONFIG="dse_imconfig.h"
    )
endif()

# 资源编译核心 importer.cpp（默认仅编进 AssetBuilder）。
# GltfImporter 只需 tinygltf（始终可用）；FbxImporter 需要 Assimp。
# 编辑器的 Import Preview (#9) 和 Asset Importer 面板始终需要 GltfImporter，
# 因此 importer.cpp + tinygltf 无条件编入；Assimp 仅在可用时链接。
target_sources(dse_editor_cpp PRIVATE ${CMAKE_SOURCE_DIR}/engine/assets/compiler/importer.cpp)
target_include_directories(dse_editor_cpp PRIVATE ${CMAKE_SOURCE_DIR}/depends/tinygltf)
target_compile_definitions(dse_editor_cpp PRIVATE DSE_EDITOR_HAS_IMPORTER=1)
if(DSE_HAS_ASSIMP)
    target_link_libraries(dse_editor_cpp PRIVATE assimp)
    target_compile_definitions(dse_editor_cpp PRIVATE DSE_HAS_ASSIMP=1)
endif()


# ─── Minizip (ZIP compression for Web export, always available) ────────────────
set(DSE_MINIZIP_DIR ${CMAKE_SOURCE_DIR}/depends/assimp/contrib/zlib/contrib/minizip)
target_sources(dse_editor_cpp PRIVATE
    ${DSE_MINIZIP_DIR}/zip.c
    ${DSE_MINIZIP_DIR}/ioapi.c
    $<$<PLATFORM_ID:Windows>:${DSE_MINIZIP_DIR}/iowin32.c>
)
target_include_directories(dse_editor_cpp PRIVATE
    ${CMAKE_SOURCE_DIR}/depends/assimp/contrib/zlib
    ${CMAKE_BINARY_DIR}/depends/assimp/contrib/zlib
    ${DSE_MINIZIP_DIR}
)
# Link zlib from assimp's internal build (zlibstatic target)
if(TARGET zlibstatic)
    target_link_libraries(dse_editor_cpp PRIVATE zlibstatic)
elseif(TARGET zlib)
    target_link_libraries(dse_editor_cpp PRIVATE zlib)
else()
    # Fallback: find system zlib
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        target_link_libraries(dse_editor_cpp PRIVATE ZLIB::ZLIB)
    else()
        # No zlib target (e.g. DSE_ENABLE_ASSIMP=OFF, the default) and no system
        # zlib: compile the bundled zlib sources directly so minizip can link.
        set(DSE_ZLIB_DIR ${CMAKE_SOURCE_DIR}/depends/assimp/contrib/zlib)
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/depends/assimp/contrib/zlib/zconf.h)
            configure_file(${DSE_ZLIB_DIR}/zconf.h.in
                           ${CMAKE_BINARY_DIR}/depends/assimp/contrib/zlib/zconf.h @ONLY)
        endif()
        target_sources(dse_editor_cpp PRIVATE
            ${DSE_ZLIB_DIR}/adler32.c ${DSE_ZLIB_DIR}/compress.c
            ${DSE_ZLIB_DIR}/crc32.c   ${DSE_ZLIB_DIR}/deflate.c
            ${DSE_ZLIB_DIR}/gzclose.c ${DSE_ZLIB_DIR}/gzlib.c
            ${DSE_ZLIB_DIR}/gzread.c  ${DSE_ZLIB_DIR}/gzwrite.c
            ${DSE_ZLIB_DIR}/infback.c ${DSE_ZLIB_DIR}/inffast.c
            ${DSE_ZLIB_DIR}/inflate.c ${DSE_ZLIB_DIR}/inftrees.c
            ${DSE_ZLIB_DIR}/trees.c   ${DSE_ZLIB_DIR}/uncompr.c
            ${DSE_ZLIB_DIR}/zutil.c
        )
    endif()
endif()

# ─── Online publish (libcurl, only when DSE_PUBLISH_ENABLED=ON) ────────────────
if(DSE_PUBLISH_ENABLED)
    find_package(CURL REQUIRED)
    target_compile_definitions(dse_editor_cpp PRIVATE DSE_PUBLISH_ENABLED=1)
    target_link_libraries(dse_editor_cpp PRIVATE CURL::libcurl)
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
