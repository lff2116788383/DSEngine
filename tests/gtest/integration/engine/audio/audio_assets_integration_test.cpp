#include <gtest/gtest.h>
#include "engine/assets/asset_manager.h"
#include "engine/audio/audio_system.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/world.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

using namespace dse::gameplay2d;

namespace {
class AudioAssetsTempDir {
public:
    AudioAssetsTempDir()
        : root_(std::filesystem::temp_directory_path() / "dse_audio_assets_integration") {
        std::filesystem::create_directories(root_);
    }

    ~AudioAssetsTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    std::filesystem::path WriteBinary(const std::string& name, const std::vector<uint8_t>& data) const {
        auto path = root_ / name;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return path;
    }

    const std::filesystem::path& Root() const { return root_; }

private:
    std::filesystem::path root_;
};
}

TEST(AudioAssetsIntegrationTest, AssetManager加载AudioClip并缓存弱引用) {
    AudioAssetsTempDir temp;
    temp.WriteBinary("clip.bin", {0x52, 0x49, 0x46, 0x46, 1, 2, 3, 4});

    AssetManager assets;
    assets.ConfigureDataRoot(temp.Root().string());

    auto clip_a = assets.LoadAudioClip("clip.bin");
    ASSERT_NE(clip_a, nullptr);
    EXPECT_NE(clip_a->GetPath().find("clip.bin"), std::string::npos);
    EXPECT_EQ(clip_a->GetData().size(), 8u);

    auto clip_b = assets.LoadAudioClip("clip.bin");
    EXPECT_EQ(clip_a.get(), clip_b.get());
}

TEST(AudioAssetsIntegrationTest, 不存在AudioClip返回nullptr) {
    AudioAssetsTempDir temp;
    AssetManager assets;
    assets.ConfigureDataRoot(temp.Root().string());

    EXPECT_EQ(assets.LoadAudioClip("missing.wav"), nullptr);
}

TEST(AudioAssetsIntegrationTest, AudioSystem初始化拒绝空AssetManager) {
    AudioSystem audio;
    EXPECT_THROW(audio.Initialize(nullptr), std::runtime_error);
}

TEST(AudioAssetsIntegrationTest, 未初始化Update保持AudioSource安全状态) {
    World world;
    AudioSystem audio;

    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    auto& source = world.registry().emplace<AudioSourceComponent>(e);
    source.play_on_awake = true;
    source.is_playing = true;

    EXPECT_NO_THROW(audio.Update(world.registry(), 0.016f));
    EXPECT_EQ(source.runtime_handle, 0u);
}

TEST(AudioAssetsIntegrationTest, 射线回调可注入并在未初始化Update时保持安全) {
    World world;
    AudioSystem audio;
    int raycast_calls = 0;
    audio.SetRaycastFunction([&](const glm::vec3&, const glm::vec3&, float) {
        ++raycast_calls;
        return AudioRaycastResult{true, 1.0f};
    });

    auto listener = world.CreateEntity();
    world.registry().emplace<TransformComponent>(listener);
    world.registry().emplace<AudioListenerComponent>(listener);

    auto src = world.CreateEntity();
    world.registry().emplace<TransformComponent>(src).position = {0.0f, 0.0f, 3.0f};
    auto& audio_src = world.registry().emplace<AudioSourceComponent>(src);
    audio_src.spatial_enabled = true;
    audio_src.occlusion_enabled = true;
    audio_src.play_on_awake = true;

    audio.Update(world.registry(), 0.016f);
    EXPECT_EQ(raycast_calls, 0);
}
