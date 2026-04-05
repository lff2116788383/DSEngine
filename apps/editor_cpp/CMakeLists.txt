add_executable(dse_editor_cpp WIN32
    src/main.cpp
    src/editor_aux_panels.cpp
    src/editor_hierarchy_panel.cpp
    src/editor_inspector_panel.cpp
    src/editor_profiler_panel.cpp
    src/editor_scene_io.cpp
    src/editor_shell.cpp
    src/editor_toolbar.cpp
    src/editor_viewport_panel.cpp
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps/glad_gl.c
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
    target_compile_options(dse_editor_cpp PRIVATE /utf-8)
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
    ${CMAKE_SOURCE_DIR}/depends/imgui
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends
    ${CMAKE_SOURCE_DIR}/third_party/imguizmo
    ${CMAKE_SOURCE_DIR}/depends/glm
    ${CMAKE_SOURCE_DIR}/depends/glm_ext
    ${CMAKE_SOURCE_DIR}/depends/spdlog/include
    ${CMAKE_SOURCE_DIR}/depends/box2d-2.4.1/include
    ${CMAKE_SOURCE_DIR}/depends
)

target_link_libraries(dse_editor_cpp PRIVATE dse_engine opengl32)

# 将编译产物放到 bin 目录下
set_target_properties(dse_editor_cpp PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    OUTPUT_NAME "dsengine-editor"
)
