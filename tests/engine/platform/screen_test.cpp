#include "catch/catch.hpp"
#include "engine/platform/screen.h"

namespace {

struct ScopedScreenReset {
    ScopedScreenReset() {
        Screen::set_width_height(0, 1);
    }

    ~ScopedScreenReset() {
        Screen::set_width_height(0, 1);
    }
};

} // namespace

TEST_CASE("Given_ScreenSize_When_SetWidthHeight_Then_AccessorsAndAspectRatioUpdate", "[engine][unit][screen]") {
    ScopedScreenReset reset;

    Screen::set_width_height(1920, 1080);

    REQUIRE(Screen::width() == 1920);
    REQUIRE(Screen::height() == 1080);
    REQUIRE(Screen::aspect_ratio() == Approx(1920.0f / 1080.0f));
}

TEST_CASE("Given_OnlyWidthChanges_When_SetWidth_Then_AspectRatioRecomputes", "[engine][unit][screen]") {
    ScopedScreenReset reset;

    Screen::set_width_height(800, 600);
    Screen::set_width(1200);

    REQUIRE(Screen::width() == 1200);
    REQUIRE(Screen::height() == 600);
    REQUIRE(Screen::aspect_ratio() == Approx(2.0f));
}

TEST_CASE("Given_OnlyHeightChanges_When_SetHeight_Then_AspectRatioRecomputes", "[engine][unit][screen]") {
    ScopedScreenReset reset;

    Screen::set_width_height(1280, 720);
    Screen::set_height(640);

    REQUIRE(Screen::width() == 1280);
    REQUIRE(Screen::height() == 640);
    REQUIRE(Screen::aspect_ratio() == Approx(2.0f));
}
