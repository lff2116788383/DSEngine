#version 430

layout(binding = 3, std140) uniform BloomParams
{
    float exposure;
    float bloomIntensity;
} _73;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D bloomBlur;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

vec3 AcesFilmic(vec3 x)
{
    float a = 2.5099999904632568359375;
    float b = 0.02999999932944774627685546875;
    float c = 2.4300000667572021484375;
    float d = 0.589999973773956298828125;
    float e = 0.14000000059604644775390625;
    return clamp((x * ((x * a) + vec3(b))) / ((x * ((x * c) + vec3(d))) + vec3(e)), vec3(0.0), vec3(1.0));
}

void main()
{
    vec3 color = texture(screenTexture, vTexCoords).xyz;
    vec3 bloomColor = texture(bloomBlur, vTexCoords).xyz;
    color += (bloomColor * _73.bloomIntensity);
    vec3 param = color * _73.exposure;
    color = AcesFilmic(param);
    color = pow(color, vec3(0.4545454680919647216796875));
    float ign = fract(52.98291778564453125 * fract((0.067110560834407806396484375 * gl_FragCoord.x) + (0.005837149918079376220703125 * gl_FragCoord.y)));
    color += vec3((ign - 0.5) / 255.0);
    FragColor = vec4(color, 1.0);
}

