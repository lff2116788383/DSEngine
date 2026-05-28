#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

layout(set = 2, binding = 0) uniform UIEffectParams {
    vec4 u_gradient_start;
    vec4 u_gradient_end;
    vec4 u_rect_size_and_radius;
    vec4 u_blur_params;
};

float roundedBoxSDF(vec2 center_pos, vec2 half_size, float radius) {
    vec2 d = abs(center_pos) - half_size + vec2(radius);
    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0) - radius;
}

void main() {
    vec4 texColor = texture(u_texture, vTexCoord);
    vec4 base_color = texColor * vColor;

    vec2 rect_size = u_rect_size_and_radius.xy;
    float corner_radius = u_rect_size_and_radius.z;
    float gradient_dir = u_rect_size_and_radius.w;

    float blur_radius = u_blur_params.x;
    float blur_intensity = u_blur_params.y;

    float grad_t = 0.0;
    if (gradient_dir < 0.5) {
        grad_t = vTexCoord.x;
    } else if (gradient_dir < 1.5) {
        grad_t = vTexCoord.y;
    } else {
        grad_t = (vTexCoord.x + vTexCoord.y) * 0.5;
    }
    vec4 gradient_color = mix(u_gradient_start, u_gradient_end, grad_t);
    base_color *= gradient_color;

    if (blur_radius > 0.0) {
        float lod = blur_radius * 0.1;
        vec4 blurred = textureLod(u_texture, vTexCoord, lod);
        base_color = mix(base_color, blurred * vColor * gradient_color, blur_intensity);
    }

    if (corner_radius > 0.0 && rect_size.x > 0.0 && rect_size.y > 0.0) {
        vec2 pixel_pos = vTexCoord * rect_size;
        vec2 center = pixel_pos - rect_size * 0.5;
        float dist = roundedBoxSDF(center, rect_size * 0.5, corner_radius);
        float aa = fwidth(dist);
        float alpha = 1.0 - smoothstep(-aa, aa, dist);
        base_color.a *= alpha;
    }

    FragColor = base_color;
}
