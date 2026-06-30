#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// Impostor LOD billboard 顶点着色器。
//
// 与 particle_instanced.vert 同模式：共享单位 quad VB + PerFrame UBO + per-instance SSBO。
// 每实例包含 world pos, size, atlas 帧信息；VS 按相机方向选帧 + billboard 朝向。
//
// 配 impostor.frag（atlas 采样 + 可选法线重建）。

layout(location = 0) in vec3 aPos;       // 单位 quad 局部坐标（x,y ∈ [-0.5,0.5]，z=0）
layout(location = 1) in vec2 aTexCoord;  // quad UV（[0,1]²）

layout(location = 0) out vec2 vTexCoord;      // 最终 atlas UV
layout(location = 1) out vec3 vViewDir;       // 物体空间视线方向（供 frag 法线重建）
layout(location = 2) out float vFade;         // 距离渐变因子 [0,1]

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 foliage_wind;   // 未使用，保持 UBO 布局兼容
    vec4 foliage_push;
};

// 每实例数据（ImpostorSystem 填充到 SSBO）
struct ImpostorInstance {
    vec4 pos_size;      // xyz = world position, w = billboard half-size
    vec4 frame_info;    // x = frame_x（整数帧列）, y = frame_y（整数帧行）
                        // z = frames_x_total, w = frames_y_total
    vec4 pivot_fade;    // xyz = pivot_offset（世界空间）, w = fade factor [0,1]
};
layout(std430, set = 7, binding = 0) readonly buffer ImpostorInstances {
    ImpostorInstance u_instances[];
};

void main() {
    ImpostorInstance inst = u_instances[gl_InstanceIndex];

    vec3 world_pos = inst.pos_size.xyz + inst.pivot_fade.xyz;
    float half_size = inst.pos_size.w;

    // 从 view 矩阵取相机右/上轴做朝相机 billboard
    vec3 camera_right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camera_up    = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 world = world_pos
        + camera_right * aPos.x * half_size
        + camera_up    * aPos.y * half_size;

    gl_Position = vp * vec4(world, 1.0);

    // 计算 atlas UV：将 quad UV 映射到当前帧在 atlas 中的子区域
    float fx = inst.frame_info.x;
    float fy = inst.frame_info.y;
    float total_x = inst.frame_info.z;
    float total_y = inst.frame_info.w;

    vec2 frame_size = vec2(1.0 / total_x, 1.0 / total_y);
    vec2 frame_origin = vec2(fx * frame_size.x, fy * frame_size.y);
    vTexCoord = frame_origin + aTexCoord * frame_size;

    // 视线方向（物体空间）
    vViewDir = normalize(camera_pos.xyz - world_pos);
    vFade = inst.pivot_fade.w;
}
