#include "catch/catch.hpp"
#include "engine/audio/audio_system.h"

using dse::gameplay2d::AudioSystem;

TEST_CASE("Given_AudioSystemWithoutAssetManager_When_InitializeCalled_Then_Throws", "[engine][unit][audio]") {
    AudioSystem system;
    REQUIRE_THROWS(system.Initialize(nullptr));
}

TEST_CASE("Given_AudioSystemWithoutInitialization_When_ControlMethodsCalled_Then_NoCrash", "[engine][unit][audio]") {
    AudioSystem system;

    REQUIRE_NOTHROW(system.PlaySound("missing.wav", 0.5f));
    REQUIRE_NOTHROW(system.PlaySfx("missing.wav", 0.5f, false));
    REQUIRE_NOTHROW(system.PlayBgm("missing.ogg", 0.5f, true));
    REQUIRE_NOTHROW(system.PauseBgm());
    REQUIRE_NOTHROW(system.ResumeBgm());
    REQUIRE_NOTHROW(system.StopBgm());
    REQUIRE_NOTHROW(system.StopAllSfx());
    REQUIRE_NOTHROW(system.Shutdown());
}

TEST_CASE("Given_AudioSystem_When_VolumeAndPolicyApisCalledBeforeInit_Then_NoCrash", "[engine][unit][audio]") {
    AudioSystem system;

    REQUIRE_NOTHROW(system.SetMasterVolume(-1.0f));
    REQUIRE_NOTHROW(system.SetMasterVolume(2.0f));
    REQUIRE_NOTHROW(system.SetBgmVolume(-2.0f));
    REQUIRE_NOTHROW(system.SetBgmVolume(2.0f));
    REQUIRE_NOTHROW(system.SetSfxVolume(-3.0f));
    REQUIRE_NOTHROW(system.SetSfxVolume(3.0f));
    REQUIRE_NOTHROW(system.SetEntityPitch(42, 0.0f));
    REQUIRE_NOTHROW(system.SetEntityPitch(42, 2.0f));
    REQUIRE_NOTHROW(system.SetMaxConcurrentSfxPerClip(0));
    REQUIRE_NOTHROW(system.SetMaxConcurrentSfxPerClip(8));
    REQUIRE_NOTHROW(system.SetSfxTriggerCooldownMs(0));
    REQUIRE_NOTHROW(system.SetSfxTriggerCooldownMs(100));
}
