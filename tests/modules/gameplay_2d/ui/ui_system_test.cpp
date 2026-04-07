#include "catch/catch.hpp"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/event_bus.h"
#include "box2d/box2d.h"
#include <algorithm>
#include <fstream>

using dse::gameplay2d::UISystem;
using dse::gameplay2d::TilemapSystem;

namespace {
std::size_t CountRenderableGlyphs(const UILabelComponent& label, const std::string& text) {
    const int atlas_cols = label.atlas_cols > 0 ? label.atlas_cols : 1;
    const int atlas_rows = label.atlas_rows > 0 ? label.atlas_rows : 1;
    const int capacity = atlas_cols * atlas_rows;
    return static_cast<std::size_t>(std::count_if(text.begin(), text.end(), [&](char ch) {
        const int glyph_code = static_cast<unsigned char>(ch) - label.ascii_start;
        return glyph_code >= 0 && glyph_code < capacity;
    }));
}

glm::vec2 ComputeUiCenter(const UIRendererComponent& ui, const glm::vec2& screen_size) {
    const glm::vec2 anchor_pos(screen_size.x * ui.anchor_min.x, screen_size.y * ui.anchor_min.y);
    const glm::vec2 scaled_size = ui.size * ui.scale;
    const glm::vec2 pivot_offset(-scaled_size.x * ui.pivot.x, -scaled_size.y * ui.pivot.y);
    const glm::vec2 final_pos = anchor_pos + ui.position + pivot_offset;
    return final_pos + scaled_size * 0.5f;
}
}

// 正向测试：鼠标按下再抬起时应触发按钮点击回调与事件总线点击事件。
TEST_CASE("Given_ClickableUI_When_MousePressRelease_Then_ClickCallbacksAreFired", "[engine][unit][ui]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.position = glm::vec2(0.0f, 0.0f);
    ui.size = glm::vec2(100.0f, 40.0f);
    ui.visible = true;
    ui.interactable = true;
    world.registry().emplace<UIButtonComponent>(entity);

    int click_count = 0;
    ui.on_click = [&](Entity) { ++click_count; };

    int bus_click_count = 0;
    auto handle = dse::core::EventBus::Instance().Subscribe<dse::core::UiClickEvent>([&](const dse::core::UiClickEvent&) {
        ++bus_click_count;
    });

    UISystem system;
    const glm::vec2 screen_size(800.0f, 600.0f);
    const glm::vec2 click_point = ComputeUiCenter(ui, screen_size);
    system.Update(world.registry(), 0.016f, screen_size, click_point, true);
    system.Update(world.registry(), 0.016f, screen_size, click_point, false);

    REQUIRE(click_count == 1);
    REQUIRE(bus_click_count == 1);
    dse::core::EventBus::Instance().Unsubscribe(handle);
}

// 边界测试：数字标签同步时应按字符数量生成运行时字形实体。
TEST_CASE("Given_NumericUILabel_When_Update_Then_RuntimeGlyphsMatchTextLength", "[engine][unit][ui]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    auto& label = world.registry().emplace<UILabelComponent>(entity);
    label.numeric_mode = true;
    label.number_value = 12345;
    label.dirty = true;

    UISystem system;
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(9999.0f, 9999.0f), false);

    REQUIRE(label.text == "12345");
    REQUIRE(label.runtime_glyph_entities.size() == 5);
}

// 反向测试：不可交互元素即使鼠标覆盖也不应进入悬停与按下状态。
TEST_CASE("Given_NonInteractableUI_When_MouseOver_Then_HoverAndPressRemainFalse", "[engine][unit][ui]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.position = glm::vec2(0.0f, 0.0f);
    ui.size = glm::vec2(100.0f, 40.0f);
    ui.visible = true;
    ui.interactable = false;

    UISystem system;
    const glm::vec2 screen_size(800.0f, 600.0f);
    const glm::vec2 hover_point = ComputeUiCenter(ui, screen_size);
    system.Update(world.registry(), 0.016f, screen_size, hover_point, true);

    REQUIRE_FALSE(ui.is_hovered);
    REQUIRE_FALSE(ui.is_pressed);
}

// 回归测试：布局在同一帧刚计算出来时，点击命中应基于最新 runtime_model，而不是上一帧残留值。
TEST_CASE("Given_AnchoredUI_When_ClickOnFirstUpdate_Then_ClickUsesFreshLayout", "[engine][unit][ui]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.anchor_min = glm::vec2(1.0f, 1.0f);
    ui.anchor_max = glm::vec2(1.0f, 1.0f);
    ui.pivot = glm::vec2(0.5f, 0.5f);
    ui.position = glm::vec2(-50.0f, -30.0f);
    ui.size = glm::vec2(100.0f, 60.0f);
    ui.visible = true;
    ui.interactable = true;

    int click_count = 0;
    ui.on_click = [&](Entity) { ++click_count; };

    UISystem system;
    const glm::vec2 screen_size(800.0f, 600.0f);
    const glm::vec2 expected_center(350.0f, 270.0f);
    system.Update(world.registry(), 0.016f, screen_size, expected_center, true);
    system.Update(world.registry(), 0.016f, screen_size, expected_center, false);

    REQUIRE(click_count == 1);
    REQUIRE(ui.is_hovered);
}

// 回归测试：当父级遮罩启用且鼠标位于遮罩外时，子按钮不应收到点击。
TEST_CASE("Given_MaskedChildUI_When_PointerOutsideMask_Then_ClickIsBlocked", "[engine][unit][ui]") {
    World world;

    auto mask_entity = world.CreateEntity();
    auto& mask_ui = world.registry().emplace<UIRendererComponent>(mask_entity);
    mask_ui.position = glm::vec2(-50.0f, -50.0f);
    mask_ui.size = glm::vec2(100.0f, 100.0f);
    mask_ui.visible = true;
    mask_ui.interactable = false;
    auto& mask = world.registry().emplace<UIMaskComponent>(mask_entity);
    mask.enabled = true;
    mask.block_outside_input = true;
    mask.size = glm::vec2(100.0f, 100.0f);

    auto child = world.CreateEntity();
    auto& child_ui = world.registry().emplace<UIRendererComponent>(child);
    child_ui.position = glm::vec2(-40.0f, -20.0f);
    child_ui.size = glm::vec2(80.0f, 40.0f);
    child_ui.visible = true;
    child_ui.interactable = true;
    world.registry().emplace<ParentComponent>(child).parent = mask_entity;

    int click_count = 0;
    child_ui.on_click = [&](Entity) { ++click_count; };

    UISystem system;
    const glm::vec2 screen_size(800.0f, 600.0f);
    const glm::vec2 outside_mask_point(620.0f, 320.0f);
    system.Update(world.registry(), 0.016f, screen_size, outside_mask_point, true);
    system.Update(world.registry(), 0.016f, screen_size, outside_mask_point, false);

    REQUIRE(click_count == 0);
    REQUIRE_FALSE(child_ui.is_hovered);
    REQUIRE_FALSE(child_ui.is_pressed);
}

TEST_CASE("Given_LocalizedUILabel_When_LanguageChanges_Then_TextAndGlyphsRefresh", "[engine][unit][ui][localization]") {
    using dse::gameplay2d::LocalizationSystem;

    const std::string en_path = "ui_localization_test_en.json";
    const std::string zh_path = "ui_localization_test_zh.json";
    {
        std::ofstream en(en_path, std::ios::trunc);
        en << R"({"ui":{"greeting":"Hello {name}"}})";
    }
    {
        std::ofstream zh(zh_path, std::ios::trunc);
        zh << R"({"ui":{"greeting":"你好，{name}"}})";
    }

    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    loc.Clear();
    REQUIRE(loc.LoadLanguage("en", en_path));
    REQUIRE(loc.LoadLanguage("zh", zh_path));
    REQUIRE(loc.SetCurrentLanguage("en"));

    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    auto& label = world.registry().emplace<UILabelComponent>(entity);
    label.use_localization = true;
    label.localization_key = "ui.greeting";
    label.fallback_text = "Fallback {name}";
    label.localization_params = {{"name", "Alice"}};
    label.dirty = true;

    UISystem system;
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(9999.0f), false);

    REQUIRE(label.text == "Hello Alice");
    REQUIRE(label.runtime_glyph_entities.size() == CountRenderableGlyphs(label, label.text));

    REQUIRE(loc.SetCurrentLanguage("zh"));
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(9999.0f), false);

    REQUIRE(label.text == "你好，Alice");
    REQUIRE(label.runtime_glyph_entities.size() == CountRenderableGlyphs(label, label.text));

    std::remove(en_path.c_str());
    std::remove(zh_path.c_str());
    loc.Clear();
}

TEST_CASE("Given_LocalizedUILabel_When_KeyMissing_Then_FallbackTextIsUsed", "[engine][unit][ui][localization]") {
    using dse::gameplay2d::LocalizationSystem;

    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    loc.Clear();

    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.visible = true;
    auto& label = world.registry().emplace<UILabelComponent>(entity);
    label.use_localization = true;
    label.localization_key = "ui.missing";
    label.fallback_text = "Fallback {name}";
    label.localization_params = {{"name", "Bob"}};
    label.dirty = true;

    UISystem system;
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(9999.0f), false);

    REQUIRE(label.text == "Fallback Bob");
    REQUIRE(label.runtime_glyph_entities.size() == CountRenderableGlyphs(label, label.text));

    loc.Clear();
}

TEST_CASE("Given_PressedUI_When_PointerLeavesBeforeRelease_Then_ClickIsCancelled", "[engine][unit][ui]") {
    World world;
    auto entity = world.CreateEntity();
    auto& ui = world.registry().emplace<UIRendererComponent>(entity);
    ui.position = glm::vec2(0.0f, 0.0f);
    ui.size = glm::vec2(120.0f, 60.0f);
    ui.visible = true;
    ui.interactable = true;
    world.registry().emplace<UIButtonComponent>(entity);

    int click_count = 0;
    ui.on_click = [&](Entity) { ++click_count; };

    UISystem system;
    const glm::vec2 screen_size(800.0f, 600.0f);
    const glm::vec2 press_point = ComputeUiCenter(ui, screen_size);
    system.Update(world.registry(), 0.016f, screen_size, press_point, true);
    REQUIRE(ui.is_hovered);
    REQUIRE(ui.is_pressed);

    system.Update(world.registry(), 0.016f, screen_size, glm::vec2(10.0f, 10.0f), true);
    REQUIRE_FALSE(ui.is_hovered);
    REQUIRE_FALSE(ui.is_pressed);

    system.Update(world.registry(), 0.016f, screen_size, glm::vec2(10.0f, 10.0f), false);

    REQUIRE(click_count == 0);
}

TEST_CASE("Given_TransformDirtyTilemap_When_Update_Then_RuntimeTilesRebuildUsingNewOrigin", "[engine][unit][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    transform.dirty = true;
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 2.0f;
    tilemap.tiles = {1, 1};
    tilemap.dirty = false;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 2);
    const Entity first_before = tilemap.runtime_tile_entities.front();
    REQUIRE(world.registry().valid(first_before));
    const auto first_before_pos = world.registry().get<TransformComponent>(first_before).position;
    REQUIRE(first_before_pos.x == Approx(-1.0f));

    transform.position = glm::vec3(10.0f, 5.0f, 0.0f);
    transform.dirty = true;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 2);
    REQUIRE_FALSE(world.registry().valid(first_before));
    const Entity first_after = tilemap.runtime_tile_entities.front();
    const auto first_after_pos = world.registry().get<TransformComponent>(first_after).position;
    REQUIRE(first_after_pos.x == Approx(9.0f));
    REQUIRE(first_after_pos.y == Approx(5.0f));
    REQUIRE_FALSE(tilemap.dirty);
}

TEST_CASE("Given_ParticleEmitterAtCapacity_When_BurstRequested_Then_NewParticlesAreNotAdded", "[engine][unit][particle]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.emitting = false;
    emitter.max_particles = 2;
    emitter.start_life_time = 3.0f;
    emitter.particles.resize(2);
    emitter.particles[0].life_time = 3.0f;
    emitter.particles[0].life_remaining = 3.0f;
    emitter.particles[1].life_time = 3.0f;
    emitter.particles[1].life_remaining = 3.0f;
    emitter.pending_burst = 3;

    ParticleSystem system;
    system.Update(world, 0.0f);

    REQUIRE(emitter.particles.size() == 2);
    REQUIRE(emitter.pending_burst == 3);
}

TEST_CASE("Given_ParticleWithExpiredLife_When_Update_Then_ExpiredParticleIsRemovedBeforeBurstSpawn", "[engine][unit][particle]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    emitter.emitting = false;
    emitter.max_particles = 1;
    emitter.start_life_time = 2.0f;
    emitter.pending_burst = 1;

    Particle2D stale_particle;
    stale_particle.life_time = 1.0f;
    stale_particle.life_remaining = 0.0f;
    emitter.particles.push_back(stale_particle);

    ParticleSystem system;
    system.Update(world, 0.0f);

    REQUIRE(emitter.particles.size() == 1);
    REQUIRE(emitter.pending_burst == 0);
    REQUIRE(emitter.particles.front().life_time == Approx(2.0f));
    REQUIRE(emitter.particles.front().life_remaining == Approx(2.0f));
}

TEST_CASE("Given_TriggerBodies_When_TheySeparate_Then_TriggerExitCallbackFires", "[engine][unit][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto trigger = world.CreateEntity();
    auto& trigger_tf = world.registry().emplace<TransformComponent>(trigger);
    trigger_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& trigger_rb = world.registry().emplace<RigidBody2DComponent>(trigger);
    trigger_rb.type = RigidBody2DType::Static;
    auto& trigger_box = world.registry().emplace<BoxCollider2DComponent>(trigger);
    trigger_box.size = glm::vec2(4.0f, 4.0f);
    trigger_box.is_trigger = true;

    auto mover = world.CreateEntity();
    auto& mover_tf = world.registry().emplace<TransformComponent>(mover);
    mover_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& mover_rb = world.registry().emplace<RigidBody2DComponent>(mover);
    mover_rb.type = RigidBody2DType::Dynamic;
    mover_rb.gravity_scale = 0.0f;
    auto& mover_box = world.registry().emplace<BoxCollider2DComponent>(mover);
    mover_box.size = glm::vec2(1.0f, 1.0f);

    int enter_count = 0;
    int exit_count = 0;
    mover_rb.on_trigger_enter = [&](Entity other) {
        if (other == trigger) {
            ++enter_count;
        }
    };
    mover_rb.on_trigger_exit = [&](Entity other) {
        if (other == trigger) {
            ++exit_count;
        }
    };

    physics_system.FixedUpdate(world, 1.0f / 60.0f);
    REQUIRE(enter_count >= 1);

    mover_rb.runtime_body->SetTransform(b2Vec2(10.0f, 0.0f), 0.0f);
    physics_system.FixedUpdate(world, 1.0f / 60.0f);

    REQUIRE(exit_count >= 1);
}
