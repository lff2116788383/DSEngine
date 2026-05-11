#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct BloomParams
{
    float src_texel_w;
    float src_texel_h;
    float dst_texel_w;
    float dst_texel_h;
};

uniform BloomParams u_params;

layout(binding = 1, rgba16f) uniform writeonly image2D u_dst;
layout(binding = 0) uniform sampler2D u_src;

void main()
{
    ivec2 dst_coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dst_size = imageSize(u_dst);
    bool _32 = dst_coord.x >= dst_size.x;
    bool _42;
    if (!_32)
    {
        _42 = dst_coord.y >= dst_size.y;
    }
    else
    {
        _42 = _32;
    }
    if (_42)
    {
        return;
    }
    vec2 uv = (vec2(dst_coord) + vec2(0.5)) * vec2(u_params.dst_texel_w, u_params.dst_texel_h);
    float x = u_params.src_texel_w;
    float y = u_params.src_texel_h;
    vec3 a = textureLod(u_src, uv + vec2(-x, y), 0.0).xyz;
    vec3 b = textureLod(u_src, uv + vec2(0.0, y), 0.0).xyz;
    vec3 c = textureLod(u_src, uv + vec2(x, y), 0.0).xyz;
    vec3 d = textureLod(u_src, uv + vec2(-x, 0.0), 0.0).xyz;
    vec3 e = textureLod(u_src, uv, 0.0).xyz;
    vec3 f = textureLod(u_src, uv + vec2(x, 0.0), 0.0).xyz;
    vec3 g = textureLod(u_src, uv + vec2(-x, -y), 0.0).xyz;
    vec3 h = textureLod(u_src, uv + vec2(0.0, -y), 0.0).xyz;
    vec3 i = textureLod(u_src, uv + vec2(x, -y), 0.0).xyz;
    vec3 result = (((e * 4.0) + ((((b + d) + f) + h) * 2.0)) + (((a + c) + g) + i)) * 0.0625;
    imageStore(u_dst, dst_coord, vec4(result, 1.0));
}

