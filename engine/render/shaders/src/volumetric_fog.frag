#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
layout(set = 2, binding = 2) uniform sampler2D u_depth_tex;

layout(std140, set = 2, binding = 0) uniform VolumetricFogParams {
    float u_fog_r;    float u_fog_g;    float u_fog_b;
    float u_fog_density;
    float u_height_falloff;
    float u_height_offset;
    float u_fog_start;
    float u_fog_end;
    float u_fog_steps;
    float u_sun_scatter;
    float u_sun_dir_x; float u_sun_dir_y; float u_sun_dir_z;
    float u_cam_pos_x; float u_cam_pos_y; float u_cam_pos_z;
    float u_near;      float u_far;
    float u_right_x;   float u_right_y;  float u_right_z;
    float u_up_x;      float u_up_y;     float u_up_z;
    float u_fwd_x;     float u_fwd_y;    float u_fwd_z;
    float u_tan_fov_y;
    float u_aspect;
};

float VFogLinZ(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth = texture(u_depth_tex, vTexCoords).r;
    if (depth >= 0.9999) { FragColor = scene; return; }

    float viewZ = VFogLinZ(depth);
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    vec3 camFwd   = vec3(u_fwd_x,   u_fwd_y,   u_fwd_z);
    vec3 camRight = vec3(u_right_x, u_right_y, u_right_z);
    vec3 camUp    = vec3(u_up_x,    u_up_y,    u_up_z);
    vec3 viewDir = normalize(camFwd
        + ndc.x * camRight * u_tan_fov_y * u_aspect
        + ndc.y * camUp    * u_tan_fov_y);
    float cosAngle = max(dot(viewDir, camFwd), 0.0001);
    float rayLen   = viewZ / cosAngle;

    float marchStart = u_fog_start;
    float marchEnd   = min(rayLen, u_fog_end);
    float steps = max(u_fog_steps, 1.0);
    if (marchEnd <= marchStart) { FragColor = scene; return; }

    float stepLen  = (marchEnd - marchStart) / steps;
    vec3  sunDir   = vec3(u_sun_dir_x, u_sun_dir_y, u_sun_dir_z);
    float cosTheta = dot(viewDir, -sunDir);
    float g = 0.76; float g2 = g * g;
    float mie = (1.0 - g2) / (4.0 * 3.14159265 *
        pow(max(1.0 + g2 - 2.0 * g * cosTheta, 0.001), 1.5));

    vec3  fogColor  = vec3(u_fog_r, u_fog_g, u_fog_b);
    vec3  camPos    = vec3(u_cam_pos_x, u_cam_pos_y, u_cam_pos_z);
    float transmit  = 1.0;
    vec3  inscatter = vec3(0.0);
    for (float i = 0.0; i < steps; i += 1.0) {
        float t   = marchStart + (i + 0.5) * stepLen;
        vec3  pos = camPos + viewDir * t;
        float h   = max(pos.y - u_height_offset, 0.0);
        float den = u_fog_density * exp(-u_height_falloff * h);
        float sT  = exp(-den * stepLen);
        inscatter += transmit * (1.0 - sT) * (fogColor + mie * u_sun_scatter * vec3(1.0));
        transmit *= sT;
        if (transmit < 0.001) break;
    }
    FragColor = vec4(scene.rgb * transmit + inscatter, scene.a);
}
