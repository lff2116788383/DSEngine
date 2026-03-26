#include "catch/catch.hpp"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/event_bus.h"

using dse::gameplay2d::UISystem;

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
