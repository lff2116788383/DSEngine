/**
 * @file editor_sequencer_test.cpp
 * @brief Sequencer pure-core (editor_sequencer_core) headless tests.
 *
 * Coverage:
 * - MakeEmptySequencerState: empty, initialized, no demo data
 * - .dsequence project round-trip: serialize → deserialize → compare
 *   (tracks, clips, keyframes, all editor-only fields)
 * - Corrupt JSON produces explicit diagnostics (no silent fallback)
 * - BakeToRuntimeSequence: editor → CutsceneSequence mapping
 * - Shared serializer round-trip: editor → bake → SerializeSequence
 *   → DeserializeSequence → SequenceFromRuntime → compare
 * - Explicit diagnostics for missing/corrupt files
 *
 * No ImGui / GLFW / UI dependency.
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "editor_sequencer_core.h"

#include "engine/cutscene/cutscene_serialize.h"
#include "engine/cutscene/cutscene_track.h"

using namespace dse::editor;
using namespace dse::cutscene;

namespace {

/// Build a rich sample SequencerState with all track types and data.
SequencerState MakeSampleState() {
    SequencerState s;
    s.initialized = true;
    s.duration = 12.5f;
    s.frame_rate = 24.0f;
    s.view_end = 12.5f;

    // Camera track
    {
        SequencerTrack t;
        t.name = "Main Camera";
        t.type = SeqTrackType::Camera;
        t.track_color = MakeSeqColor(180, 80, 80);
        t.muted = false;
        t.locked = true;
        SequencerClip c1;
        c1.name = "Dolly Shot";
        c1.start_time = 0.0f;
        c1.end_time = 4.0f;
        c1.color = MakeSeqColor(200, 100, 100, 200);
        t.clips.push_back(c1);
        SequencerClip c2;
        c2.name = "Pan Right";
        c2.start_time = 4.5f;
        c2.end_time = 7.0f;
        c2.color = MakeSeqColor(200, 100, 100, 200);
        t.clips.push_back(c2);
        s.tracks.push_back(std::move(t));
    }

    // Property track with keyframes + bindings
    {
        SequencerTrack t;
        t.name = "Hero - Transform";
        t.type = SeqTrackType::Property;
        t.target_entity = "Hero";
        t.property_path = "Transform.Position";
        t.track_color = MakeSeqColor(80, 150, 80);
        t.muted = true;
        t.locked = false;
        SequencerClip clip;
        clip.name = "Walk Forward";
        clip.start_time = 0.5f;
        clip.end_time = 5.0f;
        clip.color = MakeSeqColor(100, 200, 100, 200);
        t.clips.push_back(clip);
        t.keyframes.push_back({0.5f, 0.0f, 0.0f, 0.5f});
        t.keyframes.push_back({2.5f, 5.0f, 0.5f, 0.5f});
        t.keyframes.push_back({5.0f, 10.0f, 0.5f, 0.0f});
        s.tracks.push_back(std::move(t));
    }

    // Event track
    {
        SequencerTrack t;
        t.name = "Events";
        t.type = SeqTrackType::Event;
        t.track_color = MakeSeqColor(200, 180, 60);
        SequencerClip e1;
        e1.name = "PlayFX: Explosion";
        e1.start_time = 3.0f;
        e1.end_time = 3.1f;
        e1.asset_path = "fx/explosion";
        e1.color = MakeSeqColor(255, 200, 50, 200);
        t.clips.push_back(e1);
        SequencerClip e2;
        e2.name = "Trigger: Door Open";
        e2.start_time = 6.0f;
        e2.end_time = 6.1f;
        e2.asset_path = "triggers/door";
        e2.color = MakeSeqColor(255, 200, 50, 200);
        t.clips.push_back(e2);
        s.tracks.push_back(std::move(t));
    }

    // Audio track
    {
        SequencerTrack t;
        t.name = "BGM";
        t.type = SeqTrackType::Audio;
        t.track_color = MakeSeqColor(80, 120, 200);
        SequencerClip c;
        c.name = "epic_theme.wav";
        c.start_time = 0.0f;
        c.end_time = 10.0f;
        c.asset_path = "audio/epic_theme.wav";
        c.volume = 0.8f;
        c.color = MakeSeqColor(80, 140, 220, 200);
        t.clips.push_back(c);
        s.tracks.push_back(std::move(t));
    }

    // Fade track (editor-only, no runtime equivalent)
    {
        SequencerTrack t;
        t.name = "Fade";
        t.type = SeqTrackType::Fade;
        t.track_color = MakeSeqColor(60, 60, 60);
        SequencerClip fin;
        fin.name = "Fade In";
        fin.start_time = 0.0f;
        fin.end_time = 1.0f;
        fin.color = MakeSeqColor(40, 40, 40, 200);
        t.clips.push_back(fin);
        SequencerClip fout;
        fout.name = "Fade Out";
        fout.start_time = 11.0f;
        fout.end_time = 12.0f;
        fout.color = MakeSeqColor(40, 40, 40, 200);
        t.clips.push_back(fout);
        s.tracks.push_back(std::move(t));
    }

    return s;
}

} // namespace

// ============================================================
// MakeEmptySequencerState
// ============================================================

TEST(SequencerCore, EmptyStateIsInitialized) {
    auto s = MakeEmptySequencerState();
    EXPECT_TRUE(s.initialized);
}

TEST(SequencerCore, EmptyStateHasNoTracks) {
    auto s = MakeEmptySequencerState();
    EXPECT_TRUE(s.tracks.empty());
}

TEST(SequencerCore, EmptyStateHasNoDemoData) {
    // Regression: the old InitDemoSequencer() filled 6 tracks with demo clips.
    auto s = MakeEmptySequencerState();
    EXPECT_EQ(s.tracks.size(), 0u);
    EXPECT_GT(s.duration, 0.0f);  // has a sensible default duration
}

// ============================================================
// .dsequence project round-trip
// ============================================================

TEST(SequencerCore, ProjectRoundTripPreservesTracks) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);
    ASSERT_FALSE(json.empty());

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err))
        << "deserialize failed: " << err;
    EXPECT_TRUE(err.empty());

    EXPECT_EQ(dst.tracks.size(), src.tracks.size());
}

TEST(SequencerCore, ProjectRoundTripPreservesDuration) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));
    EXPECT_FLOAT_EQ(dst.duration, src.duration);
}

TEST(SequencerCore, ProjectRoundTripPreservesFrameRate) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));
    EXPECT_FLOAT_EQ(dst.frame_rate, src.frame_rate);
}

TEST(SequencerCore, ProjectRoundTripPreservesTrackNames) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));
    ASSERT_EQ(dst.tracks.size(), src.tracks.size());
    for (size_t i = 0; i < src.tracks.size(); ++i) {
        EXPECT_EQ(dst.tracks[i].name, src.tracks[i].name)
            << "track " << i << " name mismatch";
    }
}

TEST(SequencerCore, ProjectRoundTripPreservesTrackTypes) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));
    ASSERT_EQ(dst.tracks.size(), src.tracks.size());
    for (size_t i = 0; i < src.tracks.size(); ++i) {
        EXPECT_EQ(dst.tracks[i].type, src.tracks[i].type)
            << "track " << i << " type mismatch";
    }
}

TEST(SequencerCore, ProjectRoundTripPreservesClips) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));

    // Check the Audio track's clip (has volume + asset_path)
    const auto& src_audio = src.tracks[3]; // BGM
    const auto& dst_audio = dst.tracks[3];
    ASSERT_EQ(dst_audio.clips.size(), src_audio.clips.size());
    EXPECT_EQ(dst_audio.clips[0].name, src_audio.clips[0].name);
    EXPECT_FLOAT_EQ(dst_audio.clips[0].start_time, src_audio.clips[0].start_time);
    EXPECT_FLOAT_EQ(dst_audio.clips[0].end_time, src_audio.clips[0].end_time);
    EXPECT_EQ(dst_audio.clips[0].asset_path, src_audio.clips[0].asset_path);
    EXPECT_FLOAT_EQ(dst_audio.clips[0].volume, src_audio.clips[0].volume);
    EXPECT_EQ(dst_audio.clips[0].color, src_audio.clips[0].color);
}

TEST(SequencerCore, ProjectRoundTripPreservesKeyframes) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));

    // Check the Property track's keyframes
    const auto& src_prop = src.tracks[1]; // Hero - Transform
    const auto& dst_prop = dst.tracks[1];
    ASSERT_EQ(dst_prop.keyframes.size(), src_prop.keyframes.size());
    for (size_t i = 0; i < src_prop.keyframes.size(); ++i) {
        EXPECT_FLOAT_EQ(dst_prop.keyframes[i].time, src_prop.keyframes[i].time);
        EXPECT_FLOAT_EQ(dst_prop.keyframes[i].value, src_prop.keyframes[i].value);
        EXPECT_FLOAT_EQ(dst_prop.keyframes[i].in_tangent, src_prop.keyframes[i].in_tangent);
        EXPECT_FLOAT_EQ(dst_prop.keyframes[i].out_tangent, src_prop.keyframes[i].out_tangent);
    }
}

TEST(SequencerCore, ProjectRoundTripPreservesBindings) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));

    // Property track bindings
    const auto& src_prop = src.tracks[1];
    const auto& dst_prop = dst.tracks[1];
    EXPECT_EQ(dst_prop.target_entity, src_prop.target_entity);
    EXPECT_EQ(dst_prop.property_path, src_prop.property_path);
}

TEST(SequencerCore, ProjectRoundTripPreservesMutedLocked) {
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));

    // Camera track: locked = true
    EXPECT_TRUE(dst.tracks[0].locked);
    EXPECT_FALSE(dst.tracks[0].muted);
    // Property track: muted = true
    EXPECT_TRUE(dst.tracks[1].muted);
    EXPECT_FALSE(dst.tracks[1].locked);
}

TEST(SequencerCore, ProjectRoundTripPreservesFadeTrack) {
    // Fade track is editor-only (no runtime equivalent); must survive round-trip.
    auto src = MakeSampleState();
    std::string json = SerializeSequencerProject(src);

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));

    // Last track should be Fade
    const auto& fade = dst.tracks.back();
    EXPECT_EQ(fade.type, SeqTrackType::Fade);
    EXPECT_EQ(fade.clips.size(), 2u);
    EXPECT_EQ(fade.clips[0].name, "Fade In");
    EXPECT_EQ(fade.clips[1].name, "Fade Out");
}

TEST(SequencerCore, EmptyProjectRoundTrip) {
    auto src = MakeEmptySequencerState();
    std::string json = SerializeSequencerProject(src);
    ASSERT_FALSE(json.empty());

    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(json, dst, err));
    EXPECT_TRUE(dst.tracks.empty());
    EXPECT_TRUE(dst.initialized);
}

// ============================================================
// Explicit diagnostics (no silent fallback)
// ============================================================

TEST(SequencerCore, CorruptJsonProducesError) {
    SequencerState dst;
    std::string err;
    EXPECT_FALSE(DeserializeSequencerProject("{not valid json", dst, err));
    EXPECT_FALSE(err.empty());
}

TEST(SequencerCore, NonObjectRootProducesError) {
    SequencerState dst;
    std::string err;
    EXPECT_FALSE(DeserializeSequencerProject("[1,2,3]", dst, err));
    EXPECT_FALSE(err.empty());
}

TEST(SequencerCore, EmptyStringProducesError) {
    SequencerState dst;
    std::string err;
    EXPECT_FALSE(DeserializeSequencerProject("", dst, err));
    EXPECT_FALSE(err.empty());
}

TEST(SequencerCore, MissingTracksProducesEmptyState) {
    // A valid JSON object without "tracks" → empty state, not an error.
    SequencerState dst;
    std::string err;
    ASSERT_TRUE(DeserializeSequencerProject(R"({"version":1,"duration":5.0})", dst, err));
    EXPECT_TRUE(dst.tracks.empty());
    EXPECT_FLOAT_EQ(dst.duration, 5.0f);
    EXPECT_TRUE(dst.initialized);
}

// ============================================================
// BakeToRuntimeSequence (editor → runtime mapping)
// ============================================================

TEST(SequencerCore, BakeProducesCorrectTrackCount) {
    auto src = MakeSampleState();
    auto seq = BakeToRuntimeSequence(src, "TestSeq");
    ASSERT_NE(seq, nullptr);
    // 5 editor tracks: Camera, Property, Event, Audio, Fade
    // Fade has no runtime equivalent → 4 runtime tracks
    EXPECT_EQ(seq->GetTracks().size(), 4u);
}

TEST(SequencerCore, BakePreservesNameAndDuration) {
    auto src = MakeSampleState();
    auto seq = BakeToRuntimeSequence(src, "MySequence");
    EXPECT_EQ(seq->GetName(), "MySequence");
    EXPECT_FLOAT_EQ(seq->GetDuration(), src.duration);
}

TEST(SequencerCore, BakeMapsPropertyKeyframes) {
    auto src = MakeSampleState();
    auto seq = BakeToRuntimeSequence(src, "S");

    // Find the Property track
    const PropertyTrack* pt = nullptr;
    for (const auto& t : seq->GetTracks()) {
        if (t->GetType() == TrackType::Property) {
            pt = static_cast<const PropertyTrack*>(t.get());
            break;
        }
    }
    ASSERT_NE(pt, nullptr);
    EXPECT_EQ(pt->GetKeyframes().size(), 3u);
    EXPECT_FLOAT_EQ(pt->GetKeyframes()[0].time, 0.5f);
    EXPECT_FLOAT_EQ(pt->GetKeyframes()[1].value, 5.0f);
}

TEST(SequencerCore, BakeMapsEventClips) {
    auto src = MakeSampleState();
    auto seq = BakeToRuntimeSequence(src, "S");

    const EventTrack* et = nullptr;
    for (const auto& t : seq->GetTracks()) {
        if (t->GetType() == TrackType::Event) {
            et = static_cast<const EventTrack*>(t.get());
            break;
        }
    }
    ASSERT_NE(et, nullptr);
    EXPECT_EQ(et->GetEvents().size(), 2u);
    EXPECT_EQ(et->GetEvents()[0].event_name, "PlayFX: Explosion");
    EXPECT_EQ(et->GetEvents()[0].payload, "fx/explosion");
}

TEST(SequencerCore, BakeMapsAudioClips) {
    auto src = MakeSampleState();
    auto seq = BakeToRuntimeSequence(src, "S");

    const AudioTrack* at = nullptr;
    for (const auto& t : seq->GetTracks()) {
        if (t->GetType() == TrackType::Audio) {
            at = static_cast<const AudioTrack*>(t.get());
            break;
        }
    }
    ASSERT_NE(at, nullptr);
    EXPECT_EQ(at->GetCues().size(), 1u);
    EXPECT_EQ(at->GetCues()[0].audio_path, "audio/epic_theme.wav");
    EXPECT_FLOAT_EQ(at->GetCues()[0].volume, 0.8f);
}

// ============================================================
// Shared serializer round-trip (via cutscene_serialize)
// ============================================================

TEST(SequencerCore, SharedSerializerRoundTrip) {
    auto src = MakeSampleState();
    auto baked = BakeToRuntimeSequence(src, "Shared");

    // Serialize via shared serializer
    std::string json = SerializeSequence(*baked);
    ASSERT_FALSE(json.empty());

    // Deserialize via shared serializer
    CutsceneDiagnostics diag;
    auto loaded = DeserializeSequence(json, diag);
    ASSERT_NE(loaded, nullptr);
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(loaded->GetName(), "Shared");
    EXPECT_FLOAT_EQ(loaded->GetDuration(), src.duration);
    EXPECT_EQ(loaded->GetTracks().size(), baked->GetTracks().size());
}

TEST(SequencerCore, SharedSerializerFileRoundTrip) {
    auto src = MakeSampleState();
    auto baked = BakeToRuntimeSequence(src, "FileRT");

    std::string path = std::string(::testing::TempDir()) + "dse_seq_core_rt.dcutscene";

    CutsceneDiagnostics save_diag;
    ASSERT_TRUE(SaveSequenceToFile(*baked, path, save_diag));

    CutsceneDiagnostics load_diag;
    auto loaded = LoadSequenceFromFile(path, load_diag);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->GetTracks().size(), baked->GetTracks().size());
    std::remove(path.c_str());
}

// ============================================================
// SequenceFromRuntime (runtime → editor mapping)
// ============================================================

TEST(SequencerCore, SequenceFromRuntimeMapsPropertyTrack) {
    auto src = MakeSampleState();
    auto baked = BakeToRuntimeSequence(src, "RT");

    // Round-trip through shared serializer
    CutsceneDiagnostics diag;
    std::string json = SerializeSequence(*baked);
    auto loaded = DeserializeSequence(json, diag);
    ASSERT_NE(loaded, nullptr);

    // Map back to editor state
    SequencerState restored = SequenceFromRuntime(*loaded);
    EXPECT_TRUE(restored.initialized);
    EXPECT_FLOAT_EQ(restored.duration, src.duration);

    // Find the Property track
    bool found_prop = false;
    for (const auto& t : restored.tracks) {
        if (t.type == SeqTrackType::Property) {
            found_prop = true;
            EXPECT_EQ(t.keyframes.size(), 3u);
            EXPECT_FLOAT_EQ(t.keyframes[0].time, 0.5f);
            EXPECT_FLOAT_EQ(t.keyframes[1].value, 5.0f);
            break;
        }
    }
    EXPECT_TRUE(found_prop);
}

TEST(SequencerCore, SequenceFromRuntimeMapsEventTrack) {
    auto src = MakeSampleState();
    auto baked = BakeToRuntimeSequence(src, "RT");

    CutsceneDiagnostics diag;
    std::string json = SerializeSequence(*baked);
    auto loaded = DeserializeSequence(json, diag);
    ASSERT_NE(loaded, nullptr);

    SequencerState restored = SequenceFromRuntime(*loaded);

    bool found_event = false;
    for (const auto& t : restored.tracks) {
        if (t.type == SeqTrackType::Event) {
            found_event = true;
            EXPECT_EQ(t.clips.size(), 2u);
            EXPECT_EQ(t.clips[0].name, "PlayFX: Explosion");
            break;
        }
    }
    EXPECT_TRUE(found_event);
}

TEST(SequencerCore, SequenceFromRuntimeMapsAudioTrack) {
    auto src = MakeSampleState();
    auto baked = BakeToRuntimeSequence(src, "RT");

    CutsceneDiagnostics diag;
    std::string json = SerializeSequence(*baked);
    auto loaded = DeserializeSequence(json, diag);
    ASSERT_NE(loaded, nullptr);

    SequencerState restored = SequenceFromRuntime(*loaded);

    bool found_audio = false;
    for (const auto& t : restored.tracks) {
        if (t.type == SeqTrackType::Audio) {
            found_audio = true;
            EXPECT_EQ(t.clips.size(), 1u);
            EXPECT_FLOAT_EQ(t.clips[0].volume, 0.8f);
            break;
        }
    }
    EXPECT_TRUE(found_audio);
}

// ============================================================
// Type name conversion
// ============================================================

TEST(SequencerCore, TrackTypeNameRoundTrip) {
    for (auto t : {SeqTrackType::Camera, SeqTrackType::Property, SeqTrackType::Event,
                   SeqTrackType::Audio, SeqTrackType::Video, SeqTrackType::Fade,
                   SeqTrackType::Group}) {
        const char* name = SeqTrackTypeName(t);
        EXPECT_NE(name, nullptr);
        EXPECT_EQ(SeqTrackTypeFromName(name), t)
            << "round-trip failed for " << name;
    }
}

TEST(SequencerCore, UnknownTypeNameDefaultsToProperty) {
    EXPECT_EQ(SeqTrackTypeFromName("Nonexistent"), SeqTrackType::Property);
}

// ============================================================
// Color helper
// ============================================================

TEST(SequencerCore, MakeSeqColorProducesCorrectValue) {
    // Equivalent to IM_COL32(r,g,b,a): (a<<24)|(b<<16)|(g<<8)|r
    uint32_t c = MakeSeqColor(0xFF, 0x00, 0x00, 0xFF); // red, opaque
    EXPECT_EQ(c, 0xFF0000FFu);

    c = MakeSeqColor(0, 0, 0xFF, 0x80); // blue, half alpha
    EXPECT_EQ(c, 0x80FF0000u);
}
