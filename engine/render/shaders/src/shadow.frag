#version 450
#extension GL_ARB_separate_shader_objects : enable

void main() {
    // Depth-only pass: no color output needed.
    // The depth buffer is written automatically by the rasterizer.
}
