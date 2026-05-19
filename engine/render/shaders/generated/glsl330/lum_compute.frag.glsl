#version 430

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexCoords;

void main()
{
    float logSum = 0.0;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            vec2 uv = (vec2(float(i), float(j)) + vec2(0.5)) / vec2(8.0);
            vec3 c = texture(screenTexture, uv).xyz;
            float lum = dot(c, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
            logSum += log(max(lum, 9.9999997473787516355514526367188e-05));
        }
    }
    float avgLogLum = logSum / 64.0;
    FragColor = vec4(avgLogLum, 0.0, 0.0, 1.0);
}

