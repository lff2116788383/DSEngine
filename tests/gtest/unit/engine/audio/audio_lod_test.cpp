/**
 * @file audio_lod_test.cpp
 * @brief GTest for P5: Audio LOD System
 */

#include <gtest/gtest.h>
#include "engine/audio/audio_lod.h"

using namespace dse::audio;

class AudioLODTest : public ::testing::Test {
protected:
    void SetUp() override {
        AudioLODConfig cfg;
        cfg.full_distance = 30.0f;
        cfg.reduced_distance = 80.0f;
        cfg.virtual_distance = 200.0f;
        cfg.max_active_sources = 32;
        cfg.max_full_sources = 16;
        cfg.min_audible_volume = 0.01f;
        cfg.update_interval = 0.0f; // Immediate for tests
        system_.Init(cfg);
    }
    void TearDown() override { system_.Shutdown(); }
    AudioLODSystem system_;
};

TEST_F(AudioLODTest, RegisterAndUnregister) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(0, 0, 0), 100.0f);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(system_.GetRegisteredSourceCount(), 1u);
    system_.UnregisterSource(id);
    EXPECT_EQ(system_.GetRegisteredSourceCount(), 0u);
}

TEST_F(AudioLODTest, FullLevel_NearListener) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(10, 0, 0), 200.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Full);
    EXPECT_TRUE(system_.IsSourceAudible(id));
}

TEST_F(AudioLODTest, ReducedLevel_MediumDistance) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(60, 0, 0), 200.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Reduced);
    EXPECT_TRUE(system_.IsSourceAudible(id));
}

TEST_F(AudioLODTest, VirtualLevel_FarDistance) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(150, 0, 0), 300.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Virtual);
    EXPECT_FALSE(system_.IsSourceAudible(id));
}

TEST_F(AudioLODTest, CulledLevel_BeyondMaxDistance) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(300, 0, 0), 250.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Culled);
    EXPECT_FALSE(system_.IsSourceAudible(id));
}

TEST_F(AudioLODTest, VolumeAttenuation_Linear) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(50, 0, 0), 100.0f);
    system_.SetSourceAttenuation(id, AttenuationModel::Linear);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);

    float vol = system_.GetSourceEffectiveVolume(id);
    EXPECT_GT(vol, 0.0f);
    EXPECT_LT(vol, 1.0f);
}

TEST_F(AudioLODTest, VolumeAttenuation_AtReference) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(0.5f, 0, 0), 100.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);

    float vol = system_.GetSourceEffectiveVolume(id);
    EXPECT_FLOAT_EQ(vol, 1.0f); // Within reference distance
}

TEST_F(AudioLODTest, MaxActiveSources_Enforced) {
    AudioLODConfig cfg;
    cfg.full_distance = 30.0f;
    cfg.reduced_distance = 80.0f;
    cfg.virtual_distance = 200.0f;
    cfg.max_active_sources = 3;
    cfg.max_full_sources = 16;
    cfg.update_interval = 0.0f;
    AudioLODSystem sys;
    sys.Init(cfg);

    // Register 5 sources all within active range
    for (int i = 0; i < 5; ++i) {
        sys.RegisterSource("sound.wav", glm::vec3(20.0f + i, 0, 0), 200.0f);
    }
    sys.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);

    EXPECT_LE(sys.GetActiveSourceCount(), 3u);
    EXPECT_GE(sys.GetVirtualSourceCount(), 2u);
    sys.Shutdown();
}

TEST_F(AudioLODTest, MaxFullSources_Enforced) {
    AudioLODConfig cfg;
    cfg.full_distance = 100.0f;
    cfg.reduced_distance = 200.0f;
    cfg.virtual_distance = 300.0f;
    cfg.max_active_sources = 100;
    cfg.max_full_sources = 2;
    cfg.update_interval = 0.0f;
    AudioLODSystem sys;
    sys.Init(cfg);

    // Register 5 sources all within full range
    for (int i = 0; i < 5; ++i) {
        sys.RegisterSource("sound.wav", glm::vec3(10.0f + i, 0, 0), 300.0f);
    }
    sys.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);

    auto stats = sys.GetStats();
    EXPECT_LE(stats.full, 2u);
    EXPECT_GE(stats.reduced, 3u);
    sys.Shutdown();
}

TEST_F(AudioLODTest, PriorityAffectsVirtualization) {
    AudioLODConfig cfg;
    cfg.full_distance = 30.0f;
    cfg.reduced_distance = 80.0f;
    cfg.virtual_distance = 200.0f;
    cfg.max_active_sources = 2;
    cfg.max_full_sources = 16;
    cfg.update_interval = 0.0f;
    AudioLODSystem sys;
    sys.Init(cfg);

    // High priority source farther away
    uint32_t hi = sys.RegisterSource("hi.wav", glm::vec3(25, 0, 0), 200.0f, 100.0f);
    // Low priority sources closer
    uint32_t lo1 = sys.RegisterSource("lo1.wav", glm::vec3(10, 0, 0), 200.0f, 0.0f);
    uint32_t lo2 = sys.RegisterSource("lo2.wav", glm::vec3(15, 0, 0), 200.0f, 0.0f);

    sys.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);

    // High priority source should stay active
    EXPECT_TRUE(sys.IsSourceAudible(hi));
    (void)lo1; (void)lo2;
    sys.Shutdown();
}

TEST_F(AudioLODTest, UpdatePosition) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(10, 0, 0), 200.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Full);

    // Move far away
    system_.UpdateSourcePosition(id, glm::vec3(300, 0, 0));
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Culled);
}

TEST_F(AudioLODTest, RebaseOrigin) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(100, 0, 0), 200.0f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Virtual);

    // Rebase by (90,0,0): source moves to (10,0,0)
    system_.RebaseOrigin(glm::vec3(90, 0, 0));
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    EXPECT_EQ(system_.GetSourceLevel(id), AudioLODLevel::Full);
}

TEST_F(AudioLODTest, Stats) {
    system_.RegisterSource("s1.wav", glm::vec3(10, 0, 0), 300.0f);   // Full
    system_.RegisterSource("s2.wav", glm::vec3(50, 0, 0), 300.0f);   // Reduced
    system_.RegisterSource("s3.wav", glm::vec3(150, 0, 0), 300.0f);  // Virtual
    system_.RegisterSource("s4.wav", glm::vec3(500, 0, 0), 200.0f);  // Culled

    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    auto stats = system_.GetStats();
    EXPECT_EQ(stats.full, 1u);
    EXPECT_EQ(stats.reduced, 1u);
    EXPECT_EQ(stats.virtual_count, 1u);
    EXPECT_EQ(stats.culled, 1u);
}

TEST_F(AudioLODTest, SetVolume) {
    uint32_t id = system_.RegisterSource("sound.wav", glm::vec3(0.5f, 0, 0), 200.0f);
    system_.SetSourceVolume(id, 0.5f);
    system_.Tick(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), 1.0f);
    float vol = system_.GetSourceEffectiveVolume(id);
    EXPECT_NEAR(vol, 0.5f, 0.01f);
}
