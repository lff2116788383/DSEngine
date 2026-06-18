#version 450
#extension GL_ARB_separate_shader_objects : enable
// UI visual-effects fragment shader for the sprite batch renderer. Params arrive
// in the shared SpriteFx push-block UBO. Mirrors the legacy ui_effects.frag
// (gradient / rounded-corner / blur) but sources params from the UBO so it is
// driven purely by generic primitives.

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

layout(std140, set = 0, binding = 0) uniform SpriteFx {
    mat4 vp;
    vec4 p0;  // gradient_start
    vec4 p1;  // gradient_end
    vec4 p2;  // (rect_w, rect_h, corner_radius, gradient_dir)
    vec4 p3;  // (blur_radius, blur_intensity, _, _)
};

float roundedBoxSDF(vec2 center_pos, vec2 half_size, float radius) {
    vec2 d = abs(center_pos) - half_size + vec2(radius);
    return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0) - radius;
}

void main() {
    vec4 u_gradient_start = p0;
    vec4 u_gradient_end = p1;
    vec2 rect_size = p2.xy;
    float corner_radius = p2.z;
    float gradient_dir = p2.w;
    float blur_radius = p3.x;
    float blur_intensity = p3.y;

    vec4 texColor = texture(u_texture, vTexCoord);
    vec4 base_color = texColor * vColor;

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
