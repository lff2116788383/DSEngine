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
    "data/main3d.lua"
    "data/icon"
    "data/font"
    "data/audio"
    # B-1 蒙皮可见激活：最小双骨 rig（dmesh/dskel/danim/dmat，合计 ~1.8KB），main3d.lua 加载。
    "data/animation/minimal_rig"
    # GPU-driven 等渲染剖面（?profile=NAME 经 FindProfilePath 命中 samples/lua/pipelines/<name>.lua）。
    "samples/lua/pipelines"
    CACHE STRING "Repo-relative paths preloaded into the Emscripten MEMFS")

set(_dse_web_link_flags
    "-sUSE_GLFW=3"
    "-sFULL_ES3=1"
    "-sMIN_WEBGL_VERSION=2"
    "-sMAX_WEBGL_VERSION=2"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXIT_RUNTIME=0"
    "-sASSERTIONS=1"
    "-sFORCE_FILESYSTEM=1"
    # shell.html 的 preRun 大资产懒加载（A5）需从 JS 经 Module.FS 写入 MEMFS，并用
    # add/removeRunDependency 让运行时等待异步 fetch 完成。导出这些运行时方法。
    "-sEXPORTED_RUNTIME_METHODS=['FS','callMain','addRunDependency','removeRunDependency']"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

# 阶段 B：WebGPU 后端启用时加入 Emscripten WebGPU JS 胶水（-sUSE_WEBGPU=1）。
# 设备由 shell.html 预创建并挂到 Module.preinitializedWebGPUDevice；C++ 经
# emscripten_webgpu_get_device() 取得。WebGPU 不可用时工厂回退 WebGL2(OpenGL)。
if(DSE_ENABLE_WEBGPU)
    list(APPEND _dse_web_link_flags "-sUSE_WEBGPU=1")
endif()

# 多线程里程碑（DSE_WEB_ENABLE_THREADS=ON）：链接共享内存 + 预建 worker 线程池。
#  -pthread                 : 链接 pthread 运行时，wasm 内存切换为 SharedArrayBuffer（需宿主 COOP/COEP）。
#  -sPTHREAD_POOL_SIZE      : 预建 worker，避免运行时同步阻塞主线程创建线程；用 navigator.hardwareConcurrency。
#  -sPTHREAD_POOL_SIZE_STRICT=0 : 池耗尽时允许按需再建（不直接 abort）。
#  main() 不代理到 worker（不加 -sPROXY_TO_PTHREAD）：GLFW/WebGPU 设备须在主线程，渲染留主线程，
#    线程池仅供 JobSystem 跑 CPU 作业（剔除/动画等）。
if(DSE_WEB_ENABLE_THREADS)
    list(APPEND _dse_web_link_flags
        "-pthread"
        "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency"
        "-sPTHREAD_POOL_SIZE_STRICT=0")
endif()

foreach(_item IN LISTS DSE_WEB_PRELOAD_ITEMS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_item}")
        list(APPEND _dse_web_link_flags
            "--preload-file=${CMAKE_SOURCE_DIR}/${_item}@/${_item}")
    endif()
endforeach()

target_link_options(dse_web_host PRIVATE ${_dse_web_link_flags})

install(TARGETS dse_web_host RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
