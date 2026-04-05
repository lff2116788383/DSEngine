e#include "catch/catch.hpp"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/event_bus.h"
#include <algorithm>
#include <fstream>

using dse::gameplay2d::UISystem;

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
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(0.0f, 0.0f), true);
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(0.0f, 0.0f), false);

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
    system.Update(world.registry(), 0.016f, glm::vec2(800.0f, 600.0f), glm::vec2(0.0f, 0.0f), true);

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
    mask_ui.position = glm::vec2(0.0f, 0.0f);
    mask_ui.size = glm::vec2(100.0f, 100.0f);
    mask_ui.visible = true;
    mask_ui.interactable = false;
    auto& mask = world.registry().emplace<UIMaskComponent>(mask_entity);
    mask.enabled = true;
    mask.block_outside_input = true;
    mask.size = glm::vec2(100.0f, 100.0f);

    auto child = world.CreateEntity();
    auto& child_ui = world.registry().emplace<UIRendererComponent>(child);
    child_ui.position = glm::vec2(0.0f, 0.0f);
    child_ui.size = glm::vec2(80.0f, 40.0f);
    child_ui.visible = true;
    child_ui.interactable = true;
    world.registry().emplace<ParentComponent>(child).parent = mask_entity;

    int click_count = 0;
    child_ui.on_click = [&](Entity) { ++click_count; };

    UISystem system;
    const glm::vec2 screen_size(800.0f, 600.0f);
    const glm::vec2 outside_mask_point(120.0f, 0.0f);
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
