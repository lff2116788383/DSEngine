#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(push_constant) uniform ContactShadowParams {
    vec3 u_light_dir;
    float u_near;
    float u_far;
    vec2 u_screen_size;
    float u_strength;
    float u_step_size;
    int u_num_steps;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    if (depth >= 1.0) { FragColor = vec4(1.0); return; }
    float linDepth = linearizeDepth(depth);
    vec3 lightDir = normalize(u_light_dir);
    vec2 texelSize = 1.0 / u_screen_size;
    float occlusion = 0.0;
    int validSteps = 0;
    for (int i = 1; i <= u_num_steps; ++i) {
        float dist = u_step_size * float(i);
        vec2 sampleUV = vTexCoords + lightDir.xy * texelSize * dist * 50.0;
        if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0) break;
        float sampleDepth = textureLod(screenTexture, sampleUV, 0.0).r;
        if (sampleDepth >= 1.0) continue;
        float sampleLin = linearizeDepth(sampleDepth);
        float diff = sampleLin - linDepth;
        if (diff > 0.0 && diff < u_step_size) {
            float k = 1.0 - (diff / u_step_size);
            occlusion += k * k;
        }
        ++validSteps;
    }
    float shadow = validSteps > 0 ? 1.0 - clamp(occlusion / float(validSteps) * u_strength, 0.0, 1.0) : 1.0;
    FragColor = vec4(vec3(shadow), 1.0);
}
