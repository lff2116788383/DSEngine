#version 430

const vec3 _245[16] = vec3[](vec3(0.5381000041961669921875, 0.18559999763965606689453125, -0.4318999946117401123046875), vec3(0.13789999485015869140625, 0.248600006103515625, 0.4429999887943267822265625), vec3(0.3370999991893768310546875, 0.567900002002716064453125, -0.0057000000961124897003173828125), vec3(-0.699899971485137939453125, -0.0450999997556209564208984375, -0.0019000000320374965667724609375), vec3(0.068899996578693389892578125, -0.159799993038177490234375, -0.854700028896331787109375), vec3(0.056000001728534698486328125, 0.0068999999202787876129150390625, -0.184300005435943603515625), vec3(-0.014600000344216823577880859375, 0.14020000398159027099609375, 0.076200000941753387451171875), vec3(0.00999999977648258209228515625, -0.19239999353885650634765625, -0.03440000116825103759765625), vec3(-0.35769999027252197265625, -0.53009998798370361328125, -0.4357999861240386962890625), vec3(-0.3169000148773193359375, 0.10629999637603759765625, 0.015799999237060546875), vec3(0.010300000198185443878173828125, -0.5868999958038330078125, 0.0046000001020729541778564453125), vec3(-0.08969999849796295166015625, -0.4939999878406524658203125, 0.328700006008148193359375), vec3(0.7118999958038330078125, -0.015399999916553497314453125, -0.091799996793270111083984375), vec3(-0.053300000727176666259765625, 0.0595999993383884429931640625, -0.541100025177001953125), vec3(0.03519999980926513671875, -0.063100002706050872802734375, 0.546000003814697265625), vec3(-0.4776000082492828369140625, 0.2847000062465667724609375, -0.0271000005304813385009765625));

struct SsaoParams
{
    float u_radius;
    float u_bias;
    float u_near;
    float u_far;
    vec2 u_screen_size;
};

uniform SsaoParams _27;

layout(binding = 1) uniform sampler2D screenTexture;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float linearizeDepth(float d)
{
    float z = (d * 2.0) - 1.0;
    return ((2.0 * _27.u_near) * _27.u_far) / ((_27.u_far + _27.u_near) - (z * (_27.u_far - _27.u_near)));
}

vec3 reconstructNormal(vec2 uv)
{
    vec2 texel = vec2(1.0) / _27.u_screen_size;
    float param = texture(screenTexture, uv).x;
    float dc = linearizeDepth(param);
    float param_1 = texture(screenTexture, uv - vec2(texel.x, 0.0)).x;
    float dl = linearizeDepth(param_1);
    float param_2 = texture(screenTexture, uv + vec2(texel.x, 0.0)).x;
    float dr = linearizeDepth(param_2);
    float param_3 = texture(screenTexture, uv - vec2(0.0, texel.y)).x;
    float db = linearizeDepth(param_3);
    float param_4 = texture(screenTexture, uv + vec2(0.0, texel.y)).x;
    float dt = linearizeDepth(param_4);
    return normalize(vec3(dl - dr, db - dt, (2.0 * texel.x) * dc));
}

void main()
{
    float depth = texture(screenTexture, vTexCoords).x;
    if (depth >= 1.0)
    {
        FragColor = vec4(1.0);
        return;
    }
    float param = depth;
    float linDepth = linearizeDepth(param);
    vec2 param_1 = vTexCoords;
    vec3 normal = reconstructNormal(param_1);
    float occlusion = 0.0;
    float rScale = _27.u_radius / linDepth;
    for (int i = 0; i < 16; i++)
    {
        vec3 sampleDir = _245[i];
        if (dot(sampleDir, normal) < 0.0)
        {
            sampleDir = -sampleDir;
        }
        vec2 sampleUV = vTexCoords + ((sampleDir.xy * rScale) * (vec2(1.0) / _27.u_screen_size));
        float param_2 = texture(screenTexture, sampleUV).x;
        float sampleDepth = linearizeDepth(param_2);
        float rangeCheck = smoothstep(0.0, 1.0, _27.u_radius / abs(linDepth - sampleDepth));
        if (sampleDepth < (linDepth - _27.u_bias))
        {
            occlusion += rangeCheck;
        }
    }
    occlusion = 1.0 - (occlusion / 16.0);
    FragColor = vec4(vec3(occlusion), 1.0);
}

