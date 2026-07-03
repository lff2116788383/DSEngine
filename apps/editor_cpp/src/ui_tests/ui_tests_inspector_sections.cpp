/**
 * @file ui_tests_inspector_sections.cpp
 * @brief Inspector component section interaction tests.
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <entt/entt.hpp>
#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "../editor_icons.h"
#include "../editor_selection.h"
#include "../editor_ai_config.h"

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/audio.h"

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

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
        if (!seen) { SelectionManager::Get().SetSingle(en); return en; }
    }
    return entt::null;
}

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

void DeleteSelectedEntity(ImGuiTestContext* ctx) {
    ctx->WindowFocus("//Hierarchy");
    ctx->KeyPress(ImGuiKey_Delete);
    ctx->Yield(2);
}

} // namespace

void RegisterInspectorSectionTests(ImGuiTestEngine* engine) {
    // --- Audio Source: Play/Pause ---
    ImGuiTest* t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "audio_source_play_pause");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Audio Source");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        // Play button has icon prefix
        ctx->ItemClick(MDI_ICON_PLAY " Play");
        ctx->Yield();
        ctx->ItemClick(MDI_ICON_PAUSE " Pause");
        DeleteSelectedEntity(ctx);
    };

    // --- Audio Source: Volume Slider ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "audio_source_volume_slider");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Audio Source");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        ctx->ItemInputValue("##audio_volume", 0.5f);
        DeleteSelectedEntity(ctx);
    };

    // --- Audio Listener Section ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "audio_listener_section");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Audio Listener");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        // Verify section exists by checking a widget
        IM_CHECK(ctx->ItemExists("##listener_enabled"));
        DeleteSelectedEntity(ctx);
    };

    // --- C# Script: Class Name Input ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "csharp_class_name_input");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "C# Script");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        ctx->ItemInputValue("##csharp_class", "TestScript");
        DeleteSelectedEntity(ctx);
    };

    // --- C# Script: Build Button (verify enabled checkbox in section) ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "csharp_build_button");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "C# Script");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        ctx->ItemClick("##csharp_enabled");
        DeleteSelectedEntity(ctx);
    };

    // --- Particle: Curves Section ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "particle_curves_section");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Particle System 3D");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        IM_CHECK(ctx->ItemExists("##ps3d_rate"));
        DeleteSelectedEntity(ctx);
    };

    // --- Particle: Emission Rate ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "particle_emission_rate");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Particle System 3D");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        ctx->ItemInputValue("##ps3d_rate", 1.0f);
        DeleteSelectedEntity(ctx);
    };

    // --- Physics Debug Overlay ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "physics_debug_overlay");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "RigidBody 3D");
        ctx->Yield(4);
        ctx->WindowFocus("//Inspector");
        ctx->SetRef("//Inspector");
        IM_CHECK(ctx->ItemExists("##RigidBody3DComponent.mass"));
        DeleteSelectedEntity(ctx);
    };

    // --- AI Config: Window Open ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "ai_config_window_open");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AIConfigManager::Instance().ShowConfigWindow(true);
        ctx->Yield(4);
        ImGuiWindow* w = FindActiveWindow("AI Configuration");
        IM_CHECK(w != nullptr);
        AIConfigManager::Instance().ShowConfigWindow(false);
        ctx->Yield(2);
    };

    // --- AI Config: Model Select ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-inspector-sections", "ai_config_model_select");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AIConfigManager::Instance().ShowConfigWindow(true);
        ctx->Yield(4);
        ctx->WindowFocus("//AI Configuration");
        ctx->SetRef("//AI Configuration");
        IM_CHECK(ctx->ItemExists("Model"));
        AIConfigManager::Instance().ShowConfigWindow(false);
        ctx->Yield(2);
    };
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
