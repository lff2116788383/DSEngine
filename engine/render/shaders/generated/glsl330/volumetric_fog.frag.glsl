#version 430

struct VolumetricFogParams
{
    float u_depth_handle;
    float u_fog_r;
    float u_fog_g;
    float u_fog_b;
    float u_fog_density;
    float u_height_falloff;
    float u_height_offset;
    float u_fog_start;
    float u_fog_end;
    float u_fog_steps;
    float u_sun_scatter;
    float u_sun_dir_x;
    float u_sun_dir_y;
    float u_sun_dir_z;
    float u_cam_pos_x;
    float u_cam_pos_y;
    float u_cam_pos_z;
    float u_near;
    float u_far;
    float u_right_x;
    float u_right_y;
    float u_right_z;
    float u_up_x;
    float u_up_y;
    float u_up_z;
    float u_fwd_x;
    float u_fwd_y;
    float u_fwd_z;
    float u_tan_fov_y;
    float u_aspect;
};

uniform VolumetricFogParams _20;

layout(binding = 1) uniform sampler2D screenTexture;
layout(binding = 2) uniform sampler2D u_depth_tex;

layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;

float VFogLinZ(float d)
{
    float z = (d * 2.0) - 1.0;
    return ((2.0 * _20.u_near) * _20.u_far) / ((_20.u_far + _20.u_near) - (z * (_20.u_far - _20.u_near)));
}

void main()
{
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth = texture(u_depth_tex, vTexCoords).x;
    if (depth >= 0.99989998340606689453125)
    {
        FragColor = scene;
        return;
    }
    float param = depth;
    float viewZ = VFogLinZ(param);
    vec2 ndc = (vTexCoords * 2.0) - vec2(1.0);
    vec3 camFwd = vec3(_20.u_fwd_x, _20.u_fwd_y, _20.u_fwd_z);
    vec3 camRight = vec3(_20.u_right_x, _20.u_right_y, _20.u_right_z);
    vec3 camUp = vec3(_20.u_up_x, _20.u_up_y, _20.u_up_z);
    vec3 viewDir = normalize((camFwd + (((camRight * ndc.x) * _20.u_tan_fov_y) * _20.u_aspect)) + ((camUp * ndc.y) * _20.u_tan_fov_y));
    float cosAngle = max(dot(viewDir, camFwd), 9.9999997473787516355514526367188e-05);
    float rayLen = viewZ / cosAngle;
    float marchStart = _20.u_fog_start;
    float marchEnd = min(rayLen, _20.u_fog_end);
    float steps = max(_20.u_fog_steps, 1.0);
    if (marchEnd <= marchStart)
    {
        FragColor = scene;
        return;
    }
    float stepLen = (marchEnd - marchStart) / steps;
    vec3 sunDir = vec3(_20.u_sun_dir_x, _20.u_sun_dir_y, _20.u_sun_dir_z);
    float cosTheta = dot(viewDir, -sunDir);
    float g = 0.7599999904632568359375;
    float g2 = g * g;
    float mie = (1.0 - g2) / (12.56637096405029296875 * pow(max((1.0 + g2) - ((2.0 * g) * cosTheta), 0.001000000047497451305389404296875), 1.5));
    vec3 fogColor = vec3(_20.u_fog_r, _20.u_fog_g, _20.u_fog_b);
    vec3 camPos = vec3(_20.u_cam_pos_x, _20.u_cam_pos_y, _20.u_cam_pos_z);
    float transmit = 1.0;
    vec3 inscatter = vec3(0.0);
    for (float i = 0.0; i < steps; i += 1.0)
    {
        float t = marchStart + ((i + 0.5) * stepLen);
        vec3 pos = camPos + (viewDir * t);
        float h = max(pos.y - _20.u_height_offset, 0.0);
        float den = _20.u_fog_density * exp((-_20.u_height_falloff) * h);
        float sT = exp((-den) * stepLen);
        inscatter += ((fogColor + (vec3(1.0) * (mie * _20.u_sun_scatter))) * (transmit * (1.0 - sT)));
        transmit *= sT;
        if (transmit < 0.001000000047497451305389404296875)
        {
            break;
        }
    }
    FragColor = vec4((scene.xyz * transmit) + inscatter, scene.w);
}

