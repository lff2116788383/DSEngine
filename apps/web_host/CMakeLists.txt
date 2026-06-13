# Web (Emscripten) 宿主可执行：产出 index.html + .js + .wasm（+ 可选 .data）。
# 仅在 EMSCRIPTEN 工具链下编入（见顶层 CMakeLists 的 if(EMSCRIPTEN) 守卫）。

add_executable(dse_web_host
    "web_main.cpp"
)

target_link_libraries(dse_web_host PRIVATE dse_engine)
set_target_properties(dse_web_host PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")

# 资源预打包目录：把工程 data/ 打进 MEMFS（运行时只读 VFS）。
set(DSE_WEB_PRELOAD_DIR "${CMAKE_SOURCE_DIR}/data" CACHE PATH
    "Directory preloaded into the Emscripten MEMFS via --preload-file")

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

if(EXISTS "${DSE_WEB_PRELOAD_DIR}")
    list(APPEND _dse_web_link_flags "--preload-file=${DSE_WEB_PRELOAD_DIR}@/data")
endif()

target_link_options(dse_web_host PRIVATE ${_dse_web_link_flags})

install(TARGETS dse_web_host RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
