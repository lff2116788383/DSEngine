/**
 * @file audio_component_test.cpp
 * @brief AudioSourceComponent 音频源组件的单元测试
 *
 * 覆盖场景：
 * - 默认值合理性
 * - 字段可修改
 */

#include <gtest/gtest.h>
#include "engine/ecs/audio.h"

TEST(AudioSourceComponentTest, DefaultValues) {
    AudioSourceComponent audio;
    EXPECT_EQ(audio.clip, nullptr);
    EXPECT_TRUE(audio.play_on_awake);
    EXPECT_FALSE(audio.loop);
    EXPECT_FLOAT_EQ(audio.volume, 1.0f);
    EXPECT_FLOAT_EQ(audio.pitch, 1.0f);
    EXPECT_FALSE(audio.is_playing);
    EXPECT_FALSE(audio.restart_requested);
    EXPECT_EQ(audio.runtime_handle, 0u);
}

TEST(AudioSourceComponentTest, FieldModification) {
    AudioSourceComponent audio;
    audio.loop = true;
    audio.volume = 0.5f;
    audio.pitch = 1.5f;
    audio.play_on_awake = false;
    EXPECT_TRUE(audio.loop);
    EXPECT_FLOAT_EQ(audio.volume, 0.5f);
    EXPECT_FLOAT_EQ(audio.pitch, 1.5f);
    EXPECT_FALSE(audio.play_on_awake);
}
