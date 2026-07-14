/**
 * @file editor_sequencer_version_test.cpp
 * @brief P1-8 .dsequence versioned-format closure: edit -> versioned save/load
 *        round-trip -> legacy migration -> runtime consumption.
 *
 * Exercises the sequencer editor format now that it shares the engine version
 * envelope (ReadVersionEnvelope / WriteVersionEnvelope) and AssetDiagnostics:
 *   - serialize embeds the current schema "version"
 *   - deserialize round-trips edited state and reports source_version
 *   - a legacy (pre-v1) point-clip project migrates to start/end ranges
 *   - a forward-version file loads leniently with a warning
 *   - the loaded state bakes into a runtime CutsceneSequence (runtime consumer)
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "editor_sequencer_core.h"
#include "engine/cutscene/cutscene_track.h"

using namespace dse::editor;

namespace {

SequencerState MakeEditedState() {
    SequencerState s = MakeEmptySequencerState();
    s.duration = 12.0f;
    s.frame_rate = 24.0f;

    SequencerTrack prop;
    prop.name = "Prop";
    prop.type = SeqTrackType::Property;
    prop.target_entity = "hero";
    prop.property_path = "transform.position.x";
    prop.keyframes.push_back({0.0f, 1.0f, 0.0f, 0.0f});
    prop.keyframes.push_back({2.0f, 5.0f, 0.0f, 0.0f});
    s.tracks.push_back(prop);

    SequencerTrack ev;
    ev.name = "Events";
    ev.type = SeqTrackType::Event;
    SequencerClip clip;
    clip.name = "spawn";
    clip.start_time = 3.0f;
    clip.end_time = 3.5f;
    clip.asset_path = "fx/spawn.lua";
    ev.clips.push_back(clip);
    s.tracks.push_back(ev);
    return s;
}

}  // namespace

TEST(SequencerVersion, SerializeEmbedsVersionAndRoundTrips) {
    SequencerState src = MakeEditedState();
    std::string json = SerializeSequencerProject(src);
    EXPECT_NE(json.find("\"version\":1"), std::string::npos) << json;

    SequencerState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(DeserializeSequencerProject(json, loaded, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, kSequencerSchemaVersion);
    EXPECT_FALSE(diag.migrated);
    EXPECT_TRUE(diag.warnings.empty());

    ASSERT_EQ(loaded.tracks.size(), 2u);
    EXPECT_FLOAT_EQ(loaded.duration, 12.0f);
    EXPECT_FLOAT_EQ(loaded.frame_rate, 24.0f);
    EXPECT_EQ(loaded.tracks[0].type, SeqTrackType::Property);
    ASSERT_EQ(loaded.tracks[0].keyframes.size(), 2u);
    EXPECT_FLOAT_EQ(loaded.tracks[0].keyframes[1].value, 5.0f);
    ASSERT_EQ(loaded.tracks[1].clips.size(), 1u);
    EXPECT_EQ(loaded.tracks[1].clips[0].asset_path, "fx/spawn.lua");
}

TEST(SequencerVersion, LegacyPointClipMigratesToRange) {
    // Pre-v1 project: no version envelope, event clip stored a single "time".
    const std::string legacy = R"({
        "duration": 8.0,
        "tracks": [
            { "name": "Events", "type": "Event",
              "clips": [ { "name": "hit", "time": 4.25, "asset_path": "fx/hit.lua" } ] }
        ]
    })";

    SequencerState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(DeserializeSequencerProject(legacy, loaded, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    ASSERT_EQ(loaded.tracks.size(), 1u);
    ASSERT_EQ(loaded.tracks[0].clips.size(), 1u);
    EXPECT_FLOAT_EQ(loaded.tracks[0].clips[0].start_time, 4.25f);
    EXPECT_FLOAT_EQ(loaded.tracks[0].clips[0].end_time, 4.25f);
}

TEST(SequencerVersion, ForwardVersionLoadsLenientlyWithWarning) {
    const std::string future =
        "{\"version\":999,\"duration\":5.0,\"tracks\":[]}";
    SequencerState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(DeserializeSequencerProject(future, loaded, diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());
}

TEST(SequencerVersion, LoadedStateBakesIntoRuntimeSequence) {
    SequencerState src = MakeEditedState();
    std::string json = SerializeSequencerProject(src);

    SequencerState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(DeserializeSequencerProject(json, loaded, diag));

    auto seq = BakeToRuntimeSequence(loaded, "cutscene");
    ASSERT_NE(seq, nullptr);
    EXPECT_FLOAT_EQ(seq->GetDuration(), 12.0f);

    bool found_property = false;
    bool found_event = false;
    for (const auto& t : seq->GetTracks()) {
        if (!t) continue;
        if (t->GetType() == dse::cutscene::TrackType::Property) {
            found_property = true;
            const auto* pt = static_cast<const dse::cutscene::PropertyTrack*>(t.get());
            EXPECT_EQ(pt->GetKeyframes().size(), 2u);
        } else if (t->GetType() == dse::cutscene::TrackType::Event) {
            found_event = true;
        }
    }
    EXPECT_TRUE(found_property);
    EXPECT_TRUE(found_event);
}
