#version 430

struct ColorGradingParams
{
    float u_lut_intensity;
};

uniform ColorGradingParams _40;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 5) uniform sampler3D u_lut;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

void main()
{
    vec3 color = texture(screenTexture, vTexCoords).xyz;
    vec3 lutColor = texture(u_lut, clamp(color, vec3(0.0), vec3(1.0))).xyz;
    color = mix(color, lutColor, vec3(_40.u_lut_intensity));
    float ign = fract(52.98291778564453125 * fract((0.067110560834407806396484375 * gl_FragCoord.x) + (0.005837149918079376220703125 * gl_FragCoord.y)));
    color += vec3((ign - 0.5) / 255.0);
    FragColor = vec4(color, 1.0);
}

