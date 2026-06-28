/**
 * @file ui_tests_terrain.cpp
 * @brief ⑦ Terrain/Tilemap/Material 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 三块编辑器都靠「选中实体 → 面板自动接管该实体 → 在面板真实控件里改值」工作：
 *   - Material：选中带 MeshRenderer 的实体，"Material" 面板（停靠 Inspector 右侧、不压视口）
 *     里拖 Metallic 滑杆 / 勾 Double Sided，断言 ECS 组件字段被写回。
 *   - Tilemap："Tile Palette" 面板里点调色板格选瓦片、切笔刷工具、Clear All Tiles 清空、
 *     Resize+Apply 改地图尺寸，断言 TilemapComponent 数据与编辑器状态。
 *   - Terrain："Terrain Brush" 面板里切笔刷模式、Reset Heights 清高度、Splat 贴图路径输入，
 *     断言 TerrainComponent 数据与编辑器状态。
 *
 * 注：Tilemap/Terrain 面板是浮动窗、且测试需选中实体才能让面板接管目标实体——而选中态会让
 * 视口绘制 ImGuizmo 全屏覆盖窗、截走落在视口上的左键。面板一旦经「选中实体」接管（active_*
 * 记到 editor state、不随反选清除），即可 DeselectAll 撤掉 gizmo 再浮动放大面板安全点击。
 * Material 面板每帧读 ctx.selected_entity，必须保持选中；但它停靠右侧不压视口，gizmo 只在
 * 视口矩形内截击，故停靠点击不受影响（与 ④ Inspector 用例同理）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_selection.h"        // SelectionManager
#include "../editor_tilemap_panel.h"    // GetTilemapEditorState / TilemapBrushTool
#include "../editor_terrain_panel.h"    // GetTerrainEditorState / TerrainBrushMode

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"  // MeshRendererComponent / TerrainComponent
#include "engine/ecs/tilemap.h"               // TilemapComponent（全局命名空间）

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 右键 Hierarchy → Create Empty Entity（创建即选中），按 registry 差集取回新实体。
entt::entity NewSelectedEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield();
    SelectionManager::Get().Clear();
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) return en;
    }
    return entt::null;
}

// Inspector「Add Component」按钮 → 弹窗里点对应 MenuItem（label 即组件注册名）。
void AddComponent(ImGuiTestContext* ctx, const char* component_name) {
    ctx->WindowFocus("//Inspector");
    ctx->SetRef("//Inspector");
    ctx->ItemClick("Add Component");
    ctx->Yield();
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick(component_name);
    ctx->Yield(2);
    ctx->SetRef("//Inspector");
}

// 经 Hierarchy 右键「Delete Entity」删掉当前选中实体（编辑器正规删除路径）。
void DeleteSelectedEntity(ImGuiTestContext* ctx, entt::entity ent) {
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Delete Entity");
    ctx->Yield(2);
    IM_CHECK(!Reg().valid(ent));
}

} // namespace

void RegisterTerrainTilemapTests(ImGuiTestEngine* e) {
    // ── ⑦-1 Material 面板：拖 Metallic、勾 Double Sided，断言写回 MeshRenderer ──
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-terrain", "material_panel_edit_fields");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Mesh Renderer");
            IM_CHECK(Reg().all_of<dse::MeshRendererComponent>(ent));

            // Material 面板停靠 Inspector 右侧；激活其分页让控件可命中。
            ctx->WindowFocus("//Material");
            ctx->Yield(2);
            ctx->SetRef("//Material");

            // Metallic 滑杆（0..1）→ 写回 ECS。
            ctx->ItemInputValue("Metallic", 0.66f);
            ctx->Yield(2);
            IM_CHECK(std::abs(Reg().get<dse::MeshRendererComponent>(ent).metallic - 0.66f) < 0.01f);

            // Double Sided 勾选框翻转 → 写回 ECS。
            const bool before = Reg().get<dse::MeshRendererComponent>(ent).material_double_sided;
            ctx->ItemClick("Double Sided");
            ctx->Yield(2);
            IM_CHECK(Reg().get<dse::MeshRendererComponent>(ent).material_double_sided != before);

            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ⑦-2 Tile Palette：选瓦片 / 切工具 / Clear All / Resize，断言 TilemapComponent ──
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-terrain", "tilemap_panel_edit_grid");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);

            // Tilemap 不在 Add Component 注册表里，作为夹具直接挂上并填初值（非空瓦片便于验证清空）。
            {
                auto& tm = Reg().emplace<TilemapComponent>(ent);
                tm.width = 8; tm.height = 8; tm.tile_size = 1.0f;
                tm.tileset_cols = 4; tm.tileset_rows = 4;
                tm.tiles.assign(static_cast<size_t>(tm.width * tm.height), 3);
            }

            // 开面板并让其经「选中实体」接管该 tilemap（active_tilemap 记到 editor state）。
            *Services().show_tile_palette = true;
            ctx->Yield(4);
            IM_CHECK(GetTilemapEditorState().active_tilemap == ent);

            // 撤选撤 gizmo（active_tilemap 不随反选清除），再浮动放大面板安全点击。
            DeselectAll(ctx);
            ShowFloatingPanel(ctx, Services().show_tile_palette, "//Tile Palette");
            ctx->SetRef("//Tile Palette");

            // 调色板点格选瓦片（InvisibleButton id "##tile_<id>"）。
            ctx->ItemClick("##tile_5");
            ctx->Yield(2);
            IM_CHECK(GetTilemapEditorState().selected_tile_id == 5);

            // 切笔刷工具 Fill。
            ctx->ItemClick("Fill");
            ctx->Yield(2);
            IM_CHECK(GetTilemapEditorState().active_tool == TilemapBrushTool::FloodFill);

            // Clear All Tiles → tiles 全 0。
            ctx->ItemClick("Clear All Tiles");
            ctx->Yield(2);
            {
                auto& tm = Reg().get<TilemapComponent>(ent);
                bool all_zero = !tm.tiles.empty();
                for (int v : tm.tiles) if (v != 0) { all_zero = false; break; }
                IM_CHECK(all_zero);
            }

            // Resize Map：改 W/H 后 Apply → 写回尺寸并重建 tiles。
            ctx->ItemInputValue("W##tilemap_w", 12);
            ctx->ItemInputValue("H##tilemap_h", 10);
            ctx->ItemClick("Apply##resize_tm");
            ctx->Yield(2);
            {
                auto& tm = Reg().get<TilemapComponent>(ent);
                IM_CHECK(tm.width == 12 && tm.height == 10);
                IM_CHECK(tm.tiles.size() == static_cast<size_t>(12 * 10));
            }

            // 收尾：关面板、复位 state、销毁实体。
            *Services().show_tile_palette = false;
            GetTilemapEditorState().active_tilemap = entt::null;
            GetTilemapEditorState().editing_active = false;
            HideOptionalPanels();
            Reg().destroy(ent);
            ctx->Yield(2);
        };
    }

    // ── ⑦-3 Terrain Brush：切笔刷模式 / Reset Heights / Splat 贴图路径，断言 TerrainComponent ──
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-terrain", "terrain_panel_edit_brush");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);

            // Terrain 的 Add Component 入口 add 回调为 nullptr（"不通过 Add Component 添加"，
            // 见 editor_inspector_panel.cpp），故作为夹具直接挂上并给高度图填非零，
            // 便于验证 Reset Heights 真的清零。
            {
                auto& tr = Reg().emplace<dse::TerrainComponent>(ent);
                tr.height_data.assign(
                    static_cast<size_t>(tr.resolution_x * tr.resolution_z), 5.0f);
            }
            IM_CHECK(Reg().all_of<dse::TerrainComponent>(ent));

            // 开面板并让其接管该 terrain。
            *Services().show_terrain_editor = true;
            ctx->Yield(4);
            IM_CHECK(GetTerrainEditorState().active_terrain == ent);

            DeselectAll(ctx);
            ShowFloatingPanel(ctx, Services().show_terrain_editor, "//Terrain Brush");
            ctx->SetRef("//Terrain Brush");

            // 切笔刷模式 Lower。
            ctx->ItemClick("Lower");
            ctx->Yield(2);
            IM_CHECK(GetTerrainEditorState().brush_mode == TerrainBrushMode::Lower);

            // Reset Heights to 0 → height_data 全 0。
            ctx->ItemClick("Reset Heights to 0");
            ctx->Yield(2);
            {
                auto& tr = Reg().get<dse::TerrainComponent>(ent);
                bool all_zero = !tr.height_data.empty();
                for (float h : tr.height_data) if (h != 0.0f) { all_zero = false; break; }
                IM_CHECK(all_zero);
            }

            // 切到 Splat Paint，给 Layer 0 输入贴图路径（EnterReturnsTrue）→ 写回 ECS。
            ctx->ItemClick("Splat Paint");
            ctx->Yield(2);
            IM_CHECK(GetTerrainEditorState().splat_paint_mode);
            ctx->ItemInputValue("##splat_tex_path", "textures/grass.png");
            ctx->Yield(2);
            IM_CHECK(Reg().get<dse::TerrainComponent>(ent).splat_texture_paths[0]
                     == "textures/grass.png");

            // 收尾：关面板、复位 state、销毁实体。
            *Services().show_terrain_editor = false;
            GetTerrainEditorState().active_terrain = entt::null;
            GetTerrainEditorState().editing_active = false;
            GetTerrainEditorState().splat_paint_mode = false;
            HideOptionalPanels();
            Reg().destroy(ent);
            ctx->Yield(2);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
