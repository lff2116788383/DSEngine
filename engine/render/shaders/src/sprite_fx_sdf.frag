#version 450
#extension GL_ARB_separate_shader_objects : enable
// SDF text fragment shader for the sprite batch renderer. Params arrive in the
// shared SpriteFx push-block UBO (p0 = threshold/smoothing/outline/shadow).
// Mirrors the legacy text_sdf.frag math but sources params from the UBO instead
// of push constants, so it is driven purely by generic primitives.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

layout(std140, set = 0, binding = 0) uniform SpriteFx {
    mat4 vp;
    vec4 p0;
    vec4 p1;
    vec4 p2;
    vec4 p3;
};

void main() {
    float u_sdf_threshold = p0.x;
    float u_sdf_smoothing = p0.y;
    float u_outline_width = p0.z;

    float distance = texture(u_texture, vTexCoord).a;

    // Screen-space adaptive AA: derive coverage from the SDF gradient, falling
    // back to the configured smoothing width when derivatives degenerate.
    float aa = max(fwidth(distance), u_sdf_smoothing);

    float alpha = smoothstep(u_sdf_threshold - aa,
                             u_sdf_threshold + aa,
                             distance);

    vec4 final_color = vColor;
    if (u_outline_width > 0.0) {
        float outline_min = u_sdf_threshold - u_outline_width;
        float outline_alpha = smoothstep(outline_min - aa,
                                         outline_min + aa,
                                         distance);
        vec4 outline_color = vec4(0.0, 0.0, 0.0, 1.0);
        final_color = mix(outline_color, vColor, alpha);
        alpha = outline_alpha;
    }

    FragColor = vec4(final_color.rgb, final_color.a * alpha);
    if (FragColor.a < 0.01) discard;
}
