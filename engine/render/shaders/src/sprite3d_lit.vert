#version 450
#extension GL_ARB_separate_shader_objects : enable
// HD-2D M3.1 lit billboard vertex shader. Same vertex format as sprite3d.vert,
// plus a world-position varying for per-fragment directional/point/spot light.

layout(location = 0) in vec3 aPos;          // world position, camera-relative
layout(location = 1) in vec2 aCorner;       // x in [-0.5, 0.5], y in [0, 1]
layout(location = 2) in vec2 aSize;         // world width/height
layout(location = 3) in float aAnchor;      // 0 = feet, 0.5 = centered
layout(location = 4) in float aBillboard;   // 0=None, 1=Yaw, 2=YawPitch, 3=Screen
layout(location = 5) in vec4 aColor;        // color_tint * opacity
layout(location = 6) in vec2 aTexCoord;
layout(location = 7) in vec3 aAxisX;        // None-mode local right axis
layout(location = 8) in vec3 aAxisY;        // None-mode local up axis
layout(location = 9) in float aZOffset;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out vec3 vWorldPos;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 viewport;
};

void main() {
    vec3 world_pos = aPos;
    world_pos.z += aZOffset;

    int mode = int(aBillboard + 0.5);
    vec3 right = vec3(1.0, 0.0, 0.0);
    vec3 up = vec3(0.0, 1.0, 0.0);

    if (mode == 0) {
        right = normalize(aAxisX);
        up = normalize(aAxisY);
    } else if (mode == 1) {
        right = normalize(vec3(view[0][0], view[1][0], view[2][0]));
        up = vec3(0.0, 1.0, 0.0);
    } else if (mode == 2) {
        right = normalize(vec3(view[0][0], view[1][0], view[2][0]));
        up = normalize(vec3(view[0][1], view[1][1], view[2][1]));
    }

    vec3 pos = world_pos;
    if (mode == 3) {
        // Screen mode expands in clip space; lighting for HUD sprites is not
        // meaningful, but keep a valid world position for shader consistency.
        vec4 clip = vp * vec4(world_pos, 1.0);
        vec2 pixel_offset = vec2(aCorner.x * aSize.x,
                                 (aCorner.y - aAnchor) * aSize.y);
        clip.xy += pixel_offset * (2.0 / viewport.xy);
        vWorldPos = world_pos;
        vColor = aColor;
        vTexCoord = aTexCoord;
        gl_Position = clip;
        return;
    }

    pos = world_pos
        + right * (aCorner.x * aSize.x)
        + up * ((aCorner.y - aAnchor) * aSize.y);

    vWorldPos = pos;
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * vec4(pos, 1.0);
}