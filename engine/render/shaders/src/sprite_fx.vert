#version 450
#extension GL_ARB_separate_shader_objects : enable
// Sprite-batch effect vertex shader (SDF text / UI VFX).
// Shares the sprite2d batch VAO layout (pos@0=vec3, color@1=vec4, uv@2=vec2) so
// one vertex buffer feeds default/SDF/VFX batches. Effect params travel in a
// single "push-block" UBO at set=0,binding=0 (slot 0), the same binding slot the
// default sprite2d path uses for PerFrame -- so it maps identically on GL
// (binding point), Vulkan (sorted set/binding) and D3D11 (cbuffer register),
// avoiding the multi-UBO slot divergence deferred to B5.
// Batch vertices are already world-transformed on the CPU, so only vp is applied.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) uniform SpriteFx {
    mat4 vp;
    vec4 p0;  // SDF: (threshold, smoothing, outline_width, shadow_softness)
              // VFX: gradient_start (rgba)
    vec4 p1;  // VFX: gradient_end (rgba)
    vec4 p2;  // VFX: (rect_w, rect_h, corner_radius, gradient_dir)
    vec4 p3;  // VFX: (blur_radius, blur_intensity, _, _)
};

void main() {
    gl_Position = vp * vec4(aPos, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
