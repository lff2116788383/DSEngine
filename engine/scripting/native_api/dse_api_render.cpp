/**
 * @file dse_api_render.cpp
 * @brief DSEngine Native C ABI — 渲染服务（L5，手写）
 *
 * L5：依赖主相机投影（world_to_screen）与 AssetManager 资源加载
 * （MeshRenderer 材质/贴图），非纯组件字段访问，codegen 无法表达。
 * 在此手写 C ABI，使 Lua / C# / 编辑器三端共享同一实现：
 *   - Lua L_EcsWorldToScreen      → 委托 dse_render_world_to_screen
 *   - Lua L_EcsSetMeshMaterial（dmat 路径分支）→ 委托 dse_mesh_renderer_set_material_from_dmat
 *   - Lua L_EcsSetMeshTexture     → 委托 dse_mesh_renderer_set_texture
 *
 * 语义与原 Lua 实现逐值等价（含主相机选择、Y 翻转、可见性判定、slot 别名、贴图 handle 绑定）。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/assets/asset_manager.h"
#include "engine/platform/screen.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

using Entity = entt::entity;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline AssetManager* GAM() { return static_cast<AssetManager*>(dse_get_asset_manager_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }

} // namespace

// ============================================================
// dse_render_world_to_screen — 主相机 3D→2D 投影（与 Lua world_to_screen 逐值等价）
// ============================================================
extern "C" int dse_render_world_to_screen(float wx, float wy, float wz,
                                          float* out_sx, float* out_sy) {
    if (out_sx) *out_sx = 0.0f;
    if (out_sy) *out_sy = 0.0f;

    World* world = GW();
    if (!world) return 0;

    // 选取最高优先级的启用相机（main camera）
    auto cam_view = world->registry().view<dse::Camera3DComponent, TransformComponent>();
    entt::entity main_cam = entt::null;
    int max_priority = -9999;
    for (auto entity : cam_view) {
        auto& cam = cam_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_cam = entity;
        }
    }
    if (main_cam == entt::null) return 0;

    auto& cam = cam_view.get<dse::Camera3DComponent>(main_cam);
    auto& transform = cam_view.get<TransformComponent>(main_cam);

    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view_mat = glm::lookAt(transform.position, transform.position + front, up);
    glm::mat4 proj_mat = glm::perspective(glm::radians(cam.fov), cam.aspect_ratio, cam.near_clip, cam.far_clip);

    glm::vec4 clip = proj_mat * view_mat * glm::vec4(wx, wy, wz, 1.0f);
    bool visible = clip.w > 0.0f;
    if (clip.w == 0.0f) clip.w = 0.0001f;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float screen_w = static_cast<float>(Screen::width());
    float screen_h = static_cast<float>(Screen::height());
    float sx = (ndc.x * 0.5f + 0.5f) * screen_w;
    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_h;  // flip Y

    if (out_sx) *out_sx = sx;
    if (out_sy) *out_sy = sy;
    return (visible && ndc.z >= -1.0f && ndc.z <= 1.0f) ? 1 : 0;
}

// ============================================================
// dse_render_screen_to_world_ray — 主相机 2D→3D 反投影拾取射线
// ============================================================
extern "C" int dse_render_screen_to_world_ray(float sx, float sy,
                                              float* out_origin, float* out_dir) {
    World* world = GW();
    if (!world) return 0;

    auto cam_view = world->registry().view<dse::Camera3DComponent, TransformComponent>();
    entt::entity main_cam = entt::null;
    int max_priority = -9999;
    for (auto entity : cam_view) {
        auto& cam = cam_view.get<dse::Camera3DComponent>(entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_cam = entity;
        }
    }
    if (main_cam == entt::null) return 0;

    auto& cam = cam_view.get<dse::Camera3DComponent>(main_cam);
    auto& transform = cam_view.get<TransformComponent>(main_cam);

    glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view_mat = glm::lookAt(transform.position, transform.position + front, up);
    glm::mat4 proj_mat = glm::perspective(glm::radians(cam.fov), cam.aspect_ratio, cam.near_clip, cam.far_clip);
    glm::mat4 inv_vp = glm::inverse(proj_mat * view_mat);

    float screen_w = static_cast<float>(Screen::width());
    float screen_h = static_cast<float>(Screen::height());
    if (screen_w <= 0.0f || screen_h <= 0.0f) return 0;

    // 屏幕像素 → NDC（与 world_to_screen 的 Y 翻转互逆）
    float ndc_x = (sx / screen_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (sy / screen_h) * 2.0f;

    glm::vec4 near_clip = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 far_clip  = inv_vp * glm::vec4(ndc_x, ndc_y,  1.0f, 1.0f);
    if (near_clip.w == 0.0f || far_clip.w == 0.0f) return 0;
    glm::vec3 near_pt = glm::vec3(near_clip) / near_clip.w;
    glm::vec3 far_pt  = glm::vec3(far_clip) / far_clip.w;

    glm::vec3 dir = glm::normalize(far_pt - near_pt);
    if (out_origin) {
        out_origin[0] = transform.position.x;
        out_origin[1] = transform.position.y;
        out_origin[2] = transform.position.z;
    }
    if (out_dir) {
        out_dir[0] = dir.x;
        out_dir[1] = dir.y;
        out_dir[2] = dir.z;
    }
    return 1;
}

// ============================================================
// dse_mesh_renderer_set_material_from_dmat — 从 .dmat 载入 MaterialInstance 并拷入组件
// ============================================================
extern "C" int dse_mesh_renderer_set_material_from_dmat(uint32_t e, const char* dmat_path,
                                                        uint32_t material_index) {
    World* world = GW();
    AssetManager* assets = GAM();
    if (!world || !assets || !dmat_path) return 0;

    auto* mesh = world->registry().try_get<dse::MeshRendererComponent>(TE(e));
    if (!mesh) return 0;

    auto material = assets->LoadMaterialInstanceFromDmat(dmat_path, static_cast<std::size_t>(material_index));
    if (!material) return 0;

    mesh->material_instance_id = material->GetId();
    mesh->material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;
    mesh->shader_variant = material->GetShaderVariant();
    mesh->color = material->GetBaseColor();
    mesh->emissive = material->GetEmissiveColor();
    mesh->albedo_texture_handle = material->GetTextureSlots().albedo;
    mesh->normal_texture_handle = material->GetTextureSlots().normal;
    mesh->metallic_roughness_texture_handle = material->GetTextureSlots().metallic_roughness;
    mesh->emissive_texture_handle = material->GetTextureSlots().emissive;
    mesh->occlusion_texture_handle = material->GetTextureSlots().occlusion;
    mesh->metallic = material->GetScalarOverrides().metallic;
    mesh->roughness = material->GetScalarOverrides().roughness;
    mesh->ao = material->GetScalarOverrides().ao;
    mesh->normal_strength = material->GetScalarOverrides().normal_strength;
    mesh->material_alpha_cutoff = material->GetScalarOverrides().alpha_cutoff;
    mesh->material_alpha_test = material->GetScalarOverrides().alpha_test;
    mesh->material_double_sided = material->GetRasterOverrides().double_sided;
    return 1;
}

// ============================================================
// dse_mesh_renderer_set_texture — 按 slot 名载入贴图并绑定到对应 handle
// ============================================================
extern "C" int dse_mesh_renderer_set_texture(uint32_t e, const char* slot, const char* path,
                                             uint32_t* out_handle, int* out_width, int* out_height) {
    World* world = GW();
    AssetManager* assets = GAM();
    if (!world || !assets || !slot || !path) return 0;

    auto* mesh = world->registry().try_get<dse::MeshRendererComponent>(TE(e));
    if (!mesh) return 0;

    auto texture = assets->LoadTexture(path);
    if (!texture) return 0;

    const unsigned int handle = texture->GetHandle();
    const std::string s(slot);
    if (s == "albedo" || s == "base_color" || s == "diffuse") {
        mesh->albedo_texture_handle = handle;
    } else if (s == "normal" || s == "normal_map") {
        mesh->normal_texture_handle = handle;
    } else if (s == "metallic_roughness" || s == "roughness" || s == "mr") {
        mesh->metallic_roughness_texture_handle = handle;
    } else if (s == "emissive" || s == "emission") {
        mesh->emissive_texture_handle = handle;
    } else if (s == "occlusion" || s == "ao") {
        mesh->occlusion_texture_handle = handle;
    } else {
        return 0;
    }

    mesh->material_data_source = dse::MeshRendererComponent::MaterialDataSource::ComponentFallback;
    if (out_handle) *out_handle = static_cast<uint32_t>(handle);
    if (out_width)  *out_width  = texture->GetWidth();
    if (out_height) *out_height = texture->GetHeight();
    return 1;
}
