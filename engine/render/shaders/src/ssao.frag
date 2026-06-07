#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;

layout(push_constant) uniform SsaoParams {
    float u_radius;
    float u_bias;
    float u_near;
    float u_far;
    vec2 u_screen_size;
    float u_sample_count;
    float u_power;
    float u_intensity;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

vec3 reconstructNormal(vec2 uv) {
    vec2 texel = 1.0 / u_screen_size;
    float dc = linearizeDepth(texture(screenTexture, uv).r);
    float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
    float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
    float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
    float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
    return normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
}

const vec3 kernel[32] = vec3[](
    vec3( 0.5381, 0.1856,-0.4319), vec3( 0.1379, 0.2486, 0.4430),
    vec3( 0.3371, 0.5679,-0.0057), vec3(-0.6999,-0.0451,-0.0019),
    vec3( 0.0689,-0.1598,-0.8547), vec3( 0.0560, 0.0069,-0.1843),
    vec3(-0.0146, 0.1402, 0.0762), vec3( 0.0100,-0.1924,-0.0344),
    vec3(-0.3577,-0.5301,-0.4358), vec3(-0.3169, 0.1063, 0.0158),
    vec3( 0.0103,-0.5869, 0.0046), vec3(-0.0897,-0.4940, 0.3287),
    vec3( 0.7119,-0.0154,-0.0918), vec3(-0.0533, 0.0596,-0.5411),
    vec3( 0.0352,-0.0631, 0.5460), vec3(-0.4776, 0.2847,-0.0271),
    vec3( 0.2932, 0.4459, 0.2636), vec3(-0.1820,-0.3244, 0.5732),
    vec3( 0.4428,-0.3241,-0.1975), vec3(-0.5679, 0.2157, 0.3764),
    vec3( 0.1251, 0.6310,-0.2199), vec3(-0.4038,-0.0568,-0.7341),
    vec3( 0.6512, 0.2826, 0.1492), vec3(-0.2410, 0.5512, 0.4153),
    vec3( 0.0890,-0.4371, 0.5681), vec3(-0.6210, 0.4877,-0.0632),
    vec3( 0.3589,-0.5746, 0.1293), vec3(-0.1187, 0.2145,-0.6723),
    vec3( 0.4810, 0.0547,-0.5378), vec3(-0.3451,-0.6129, 0.1472),
    vec3( 0.1839, 0.3482, 0.6541), vec3(-0.5234, 0.0311,-0.4187)
);

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    if (depth >= 1.0) { FragColor = vec4(1.0); return; }
    float linDepth = linearizeDepth(depth);
    vec3 normal = reconstructNormal(vTexCoords);
    float occlusion = 0.0;
    float rScale = u_radius / linDepth;
    int sampleCount = clamp(int(u_sample_count), 4, 32);
    for (int i = 0; i < sampleCount; ++i) {
        vec3 sampleDir = kernel[i];
        if (dot(sampleDir, normal) < 0.0) sampleDir = -sampleDir;
        vec2 sampleUV = vTexCoords + sampleDir.xy * rScale * (1.0 / u_screen_size);
        float sampleDepth = linearizeDepth(texture(screenTexture, sampleUV).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(linDepth - sampleDepth));
        if (sampleDepth < linDepth - u_bias) occlusion += rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(sampleCount));
    occlusion = pow(occlusion, u_power) * u_intensity;
    occlusion = clamp(occlusion, 0.0, 1.0);
    FragColor = vec4(vec3(occlusion), 1.0);
}
