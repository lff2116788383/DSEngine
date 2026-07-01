#version 450

// Full-screen triangle for visibility buffer resolve
// No vertex input needed — generates a full-screen triangle from gl_VertexIndex

layout(location = 0) out vec2 v_uv;

void main() {
    // Full-screen triangle: 3 vertices covering [-1,1] NDC
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    v_uv = uvs[gl_VertexIndex];
}
