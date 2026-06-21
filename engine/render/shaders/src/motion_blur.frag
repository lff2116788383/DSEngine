#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_color_texture;

layout(std140, set = 2, binding = 0) uniform MotionBlurParams {
    float intensity;
    float num_samples;
    float screen_w;
    float screen_h;
};

void main() {
    vec2 velocity = texture(screenTexture, vTexCoords).rg * intensity;
    int samples = max(int(num_samples), 1);
    vec3 color = texture(u_color_texture, vTexCoords).rgb;
    float total = 1.0;
    for (int i = 1; i < samples; ++i) {
        float t = float(i) / float(samples);
        vec2 sample_uv = vTexCoords + velocity * t;
        if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && sample_uv.y >= 0.0 && sample_uv.y <= 1.0) {
            color += textureLod(u_color_texture, sample_uv, 0.0).rgb;
            total += 1.0;
        }
    }
    FragColor = vec4(color / total, 1.0);
}
