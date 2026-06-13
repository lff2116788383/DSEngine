# Web (Emscripten) 宿主可执行：产出 index.html + .js + .wasm（+ 可选 .data）。
# 仅在 EMSCRIPTEN 工具链下编入（见顶层 CMakeLists 的 if(EMSCRIPTEN) 守卫）。

add_executable(dse_web_host
    "web_main.cpp"
)

target_link_libraries(dse_web_host PRIVATE dse_engine)
set_target_properties(dse_web_host PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")

# 资源预打包目录：把工程 data/ 打进 MEMFS（运行时只读 VFS）。
# Curated MEMFS preload: only the assets the Web 2D demo actually needs, so
# the .data payload stays small (the full data/ tree contains large demo
# assets like vse_demo/ and mirror_assets/ that are not used on Web).
# Each entry maps <repo>/<path> to /<path> in MEMFS.
set(DSE_WEB_PRELOAD_ITEMS
    "data/main.lua"
    "data/icon"
    "data/font"
    CACHE STRING "Repo-relative paths preloaded into the Emscripten MEMFS")

set(_dse_web_link_flags
    "-sUSE_GLFW=3"
    "-sFULL_ES3=1"
    "-sMIN_WEBGL_VERSION=2"
    "-sMAX_WEBGL_VERSION=2"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXIT_RUNTIME=0"
    "-sASSERTIONS=1"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

foreach(_item IN LISTS DSE_WEB_PRELOAD_ITEMS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_item}")
        list(APPEND _dse_web_link_flags
            "--preload-file=${CMAKE_SOURCE_DIR}/${_item}@/${_item}")
    endif()
endforeach()

target_link_options(dse_web_host PRIVATE ${_dse_web_link_flags})

install(TARGETS dse_web_host RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
