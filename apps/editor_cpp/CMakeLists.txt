add_executable(dse_editor_cpp 
    src/main.cpp
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps/glad_gl.c
)

target_include_directories(dse_editor_cpp PRIVATE 
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/include
    ${CMAKE_SOURCE_DIR}/depends/glfw-3.3-3.4/deps
    ${CMAKE_SOURCE_DIR}/depends/imgui
    ${CMAKE_SOURCE_DIR}/depends/imgui/backends
)

target_link_libraries(dse_editor_cpp PRIVATE dse_engine opengl32)

# 将编译产物放到 bin 目录下
set_target_properties(dse_editor_cpp PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    OUTPUT_NAME "dsengine-editor"
)
