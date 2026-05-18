#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform LightShaftParams {
    float u_depth_handle;
    float u_sun_x;
    float u_sun_y;
    float u_light_r;
    float u_light_g;
    float u_light_b;
    float u_density;
    float u_weight;
    float u_decay;
    float u_exposure;
    float u_num_samples;
    float u_intensity;
    float pad0; float pad1; float pad2; float pad3;
};

layout(set = 0, binding = 0) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    int samples = int(u_num_samples);
    vec2 delta_uv = (vec2(u_sun_x, u_sun_y) - vTexCoords) * u_density / float(samples);

    vec2 uv = vTexCoords;
    float illum_decay = 1.0;
    vec3 accumulated = vec3(0.0);

    for (int i = 0; i < samples; i++) {
        uv += delta_uv;
        vec2 suv = clamp(uv, 0.001, 0.999);
        float d = texture(u_depth_tex, suv).r;
        vec3 s = texture(screenTexture, suv).rgb;
        float sky = step(0.9999, d);
        float lum = dot(s, vec3(0.2126, 0.7152, 0.0722));
        float bright = smoothstep(0.8, 1.2, lum);
        float mask = max(sky, bright);
        accumulated += s * mask * illum_decay * u_weight;
        illum_decay *= u_decay;
        if (illum_decay < 0.003) break;
    }

    vec3 result = scene.rgb + accumulated * u_exposure * vec3(u_light_r, u_light_g, u_light_b) * u_intensity;
    FragColor = vec4(result, 1.0);
}
