#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

// SDF 参数通过 push constant 传入
layout(push_constant) uniform PushConstants {
    mat4 u_model;
    float u_sdf_threshold;    // 边缘阈值 (默认 0.5)
    float u_sdf_smoothing;    // 平滑宽度 (默认 0.1)
    float u_outline_width;    // 描边宽度 (0 = 无描边)
    float u_shadow_softness;  // 阴影柔软度 (0 = 无阴影)
} pc;

void main() {
    float distance = texture(u_texture, vTexCoord).a;

    // SDF 文本抗锯齿
    float alpha = smoothstep(pc.u_sdf_threshold - pc.u_sdf_smoothing,
                             pc.u_sdf_threshold + pc.u_sdf_smoothing,
                             distance);

    // 描边 (outline)
    vec4 final_color = vColor;
    if (pc.u_outline_width > 0.0) {
        float outline_min = pc.u_sdf_threshold - pc.u_outline_width;
        float outline_alpha = smoothstep(outline_min - pc.u_sdf_smoothing,
                                         outline_min + pc.u_sdf_smoothing,
                                         distance);
        // 描边颜色固定为黑色，可后续扩展
        vec4 outline_color = vec4(0.0, 0.0, 0.0, 1.0);
        final_color = mix(outline_color, vColor, alpha);
        alpha = outline_alpha;
    }

    FragColor = vec4(final_color.rgb, final_color.a * alpha);
    if (FragColor.a < 0.01) discard;
}
