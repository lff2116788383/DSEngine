#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 5) uniform sampler3D u_lut;

layout(push_constant) uniform ColorGradingParams {
    float u_lut_intensity;
};

void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    vec3 lutColor = texture(u_lut, clamp(color, 0.0, 1.0)).rgb;
    color = mix(color, lutColor, u_lut_intensity);
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    color += (ign - 0.5) / 255.0;
    FragColor = vec4(color, 1.0);
}
