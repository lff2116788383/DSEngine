/**
 * @file editor_sequencer_core.cpp
 * @brief Sequencer pure-core implementation — no ImGui / UI dependency.
 *
 * Implements versioned .dsequence serialization and mapping to/from the
 * shared engine CutsceneSequence via cutscene_serialize.
 */

#include "editor_sequencer_core.h"

#include <sstream>
#include <utility>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "engine/core/asset_version_envelope.h"
#include "engine/cutscene/cutscene_serialize.h"

namespace dse::editor {

// ─── Type name conversion ───────────────────────────────────────────────

const char* SeqTrackTypeName(SeqTrackType type) {
    switch (type) {
        case SeqTrackType::Camera:   return "Camera";
        case SeqTrackType::Property: return "Property";
        case SeqTrackType::Event:    return "Event";
        case SeqTrackType::Audio:    return "Audio";
        case SeqTrackType::Video:    return "Video";
        case SeqTrackType::Fade:     return "Fade";
        case SeqTrackType::Group:    return "Group";
    }
    return "Property";
}

SeqTrackType SeqTrackTypeFromName(const std::string& name) {
    if (name == "Camera")   return SeqTrackType::Camera;
    if (name == "Event")    return SeqTrackType::Event;
    if (name == "Audio")    return SeqTrackType::Audio;
    if (name == "Video")    return SeqTrackType::Video;
    if (name == "Fade")     return SeqTrackType::Fade;
    if (name == "Group")    return SeqTrackType::Group;
    return SeqTrackType::Property;
}

// ─── Serialization (.dsequence project format) ──────────────────────────

std::string SerializeSequencerProject(const SequencerState& state) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& a = doc.GetAllocator();
    assets::WriteVersionEnvelope(doc, kSequencerSchemaVersion, a);
    doc.AddMember("duration", state.duration, a);
    doc.AddMember("frame_rate", state.frame_rate, a);

    rapidjson::Value tracks(rapidjson::kArrayType);
    for (const auto& tr : state.tracks) {
        rapidjson::Value tj(rapidjson::kObjectType);
        tj.AddMember("name", rapidjson::Value(tr.name.c_str(), a), a);
        tj.AddMember("type", rapidjson::Value(SeqTrackTypeName(tr.type), a), a);
        tj.AddMember("muted", tr.muted, a);
        tj.AddMember("locked", tr.locked, a);
        tj.AddMember("visible", tr.visible, a);
        tj.AddMember("expanded", tr.expanded, a);
        tj.AddMember("height", tr.height, a);
        tj.AddMember("track_color", static_cast<uint64_t>(tr.track_color), a);
        tj.AddMember("group_index", tr.group_index, a);
        tj.AddMember("target_entity", rapidjson::Value(tr.target_entity.c_str(), a), a);
        tj.AddMember("property_path", rapidjson::Value(tr.property_path.c_str(), a), a);

        rapidjson::Value clips(rapidjson::kArrayType);
        for (const auto& c : tr.clips) {
            rapidjson::Value cj(rapidjson::kObjectType);
            cj.AddMember("name", rapidjson::Value(c.name.c_str(), a), a);
            cj.AddMember("start_time", c.start_time, a);
            cj.AddMember("end_time", c.end_time, a);
            cj.AddMember("color", static_cast<uint64_t>(c.color), a);
            cj.AddMember("asset_path", rapidjson::Value(c.asset_path.c_str(), a), a);
            cj.AddMember("volume", c.volume, a);
            clips.PushBack(cj, a);
        }
        tj.AddMember("clips", clips, a);

        rapidjson::Value kfs(rapidjson::kArrayType);
        for (const auto& k : tr.keyframes) {
            rapidjson::Value kj(rapidjson::kObjectType);
            kj.AddMember("time", k.time, a);
            kj.AddMember("value", k.value, a);
            kj.AddMember("in_tangent", k.in_tangent, a);
            kj.AddMember("out_tangent", k.out_tangent, a);
            kfs.PushBack(kj, a);
        }
        tj.AddMember("keyframes", kfs, a);
        tracks.PushBack(tj, a);
    }
    doc.AddMember("tracks", tracks, a);

    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return std::string(buf.GetString(), buf.GetSize());
}

bool DeserializeSequencerProject(const std::string& json,
                                 SequencerState& state,
                                 dse::assets::AssetDiagnostics& diag) {
    diag = dse::assets::AssetDiagnostics{};

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) { diag.errors.push_back("JSON parse error"); return false; }
    if (!doc.IsObject()) { diag.errors.push_back("root is not an object"); return false; }

    const int version = dse::assets::ReadVersionEnvelope(
        doc, kSequencerSchemaVersion, ".dsequence", diag);
    const bool legacy = version < kSequencerSchemaVersion;

    SequencerState loaded;
    loaded.initialized = true;
    if (doc.HasMember("duration") && doc["duration"].IsNumber())
        loaded.duration = doc["duration"].GetFloat();
    if (doc.HasMember("frame_rate") && doc["frame_rate"].IsNumber())
        loaded.frame_rate = doc["frame_rate"].GetFloat();
    loaded.view_end = loaded.duration;

    if (doc.HasMember("tracks") && doc["tracks"].IsArray()) {
        for (const auto& tj : doc["tracks"].GetArray()) {
            if (!tj.IsObject()) continue;
            SequencerTrack tr;
            if (tj.HasMember("name") && tj["name"].IsString()) tr.name = tj["name"].GetString();
            if (tj.HasMember("type") && tj["type"].IsString()) tr.type = SeqTrackTypeFromName(tj["type"].GetString());
            if (tj.HasMember("muted") && tj["muted"].IsBool()) tr.muted = tj["muted"].GetBool();
            if (tj.HasMember("locked") && tj["locked"].IsBool()) tr.locked = tj["locked"].GetBool();
            if (tj.HasMember("visible") && tj["visible"].IsBool()) tr.visible = tj["visible"].GetBool();
            if (tj.HasMember("expanded") && tj["expanded"].IsBool()) tr.expanded = tj["expanded"].GetBool();
            if (tj.HasMember("height") && tj["height"].IsNumber()) tr.height = tj["height"].GetFloat();
            if (tj.HasMember("track_color") && tj["track_color"].IsUint64())
                tr.track_color = static_cast<uint32_t>(tj["track_color"].GetUint64());
            if (tj.HasMember("group_index") && tj["group_index"].IsInt())
                tr.group_index = tj["group_index"].GetInt();
            if (tj.HasMember("target_entity") && tj["target_entity"].IsString()) tr.target_entity = tj["target_entity"].GetString();
            if (tj.HasMember("property_path") && tj["property_path"].IsString()) tr.property_path = tj["property_path"].GetString();

            if (tj.HasMember("clips") && tj["clips"].IsArray()) {
                for (const auto& cj : tj["clips"].GetArray()) {
                    if (!cj.IsObject()) continue;
                    SequencerClip c;
                    if (cj.HasMember("name") && cj["name"].IsString()) c.name = cj["name"].GetString();
                    const bool has_start = cj.HasMember("start_time") && cj["start_time"].IsNumber();
                    const bool has_end = cj.HasMember("end_time") && cj["end_time"].IsNumber();
                    if (has_start) c.start_time = cj["start_time"].GetFloat();
                    if (has_end) c.end_time = cj["end_time"].GetFloat();
                    // Legacy migration: pre-v1 clips were points with a single
                    // `time` field instead of a start/end range.
                    if (!has_start && cj.HasMember("time") && cj["time"].IsNumber()) {
                        c.start_time = cj["time"].GetFloat();
                        if (!has_end) c.end_time = c.start_time;
                        diag.migrated = true;
                    }
                    if (cj.HasMember("color") && cj["color"].IsUint64()) c.color = static_cast<uint32_t>(cj["color"].GetUint64());
                    if (cj.HasMember("asset_path") && cj["asset_path"].IsString()) c.asset_path = cj["asset_path"].GetString();
                    if (cj.HasMember("volume") && cj["volume"].IsNumber()) c.volume = cj["volume"].GetFloat();
                    tr.clips.push_back(std::move(c));
                }
            }
            if (tj.HasMember("keyframes") && tj["keyframes"].IsArray()) {
                for (const auto& kj : tj["keyframes"].GetArray()) {
                    if (!kj.IsObject()) continue;
                    SequencerKeyframe k;
                    if (kj.HasMember("time") && kj["time"].IsNumber()) k.time = kj["time"].GetFloat();
                    if (kj.HasMember("value") && kj["value"].IsNumber()) k.value = kj["value"].GetFloat();
                    if (kj.HasMember("in_tangent") && kj["in_tangent"].IsNumber()) k.in_tangent = kj["in_tangent"].GetFloat();
                    if (kj.HasMember("out_tangent") && kj["out_tangent"].IsNumber()) k.out_tangent = kj["out_tangent"].GetFloat();
                    tr.keyframes.push_back(std::move(k));
                }
            }
            loaded.tracks.push_back(std::move(tr));
        }
    }

    if (legacy && diag.migrated) {
        diag.warnings.push_back(
            ".dsequence: migrated legacy point clips to start/end ranges");
    }
    diag.ok = true;
    state = std::move(loaded);
    return true;
}

bool DeserializeSequencerProject(const std::string& json,
                                 SequencerState& state,
                                 std::string& err) {
    dse::assets::AssetDiagnostics diag;
    if (!DeserializeSequencerProject(json, state, diag)) {
        err = diag.errors.empty() ? "deserialize failed" : diag.errors.front();
        return false;
    }
    return true;
}

// ─── Runtime mapping (via shared cutscene serializer) ────────────────────

std::shared_ptr<cutscene::CutsceneSequence>
BakeToRuntimeSequence(const SequencerState& state, const std::string& seq_name) {
    auto seq = std::make_shared<cutscene::CutsceneSequence>(seq_name, state.duration);
    for (const auto& tr : state.tracks) {
        switch (tr.type) {
            case SeqTrackType::Camera: {
                seq->AddTrack(std::make_shared<cutscene::CameraTrack>(tr.name));
                break;
            }
            case SeqTrackType::Property: {
                auto pt = std::make_shared<cutscene::PropertyTrack>(tr.name);
                for (const auto& k : tr.keyframes)
                    pt->AddKeyframe(k.time, k.value);
                seq->AddTrack(pt);
                break;
            }
            case SeqTrackType::Event: {
                auto et = std::make_shared<cutscene::EventTrack>(tr.name);
                for (const auto& c : tr.clips)
                    et->AddEvent(c.start_time, c.name, c.asset_path);
                seq->AddTrack(et);
                break;
            }
            case SeqTrackType::Audio: {
                auto at = std::make_shared<cutscene::AudioTrack>(tr.name);
                for (const auto& c : tr.clips)
                    at->AddCue(c.start_time,
                               c.asset_path.empty() ? c.name : c.asset_path,
                               c.volume, false);
                seq->AddTrack(at);
                break;
            }
            case SeqTrackType::Video: {
                auto vt = std::make_shared<cutscene::VideoTrack>(tr.name);
                for (const auto& c : tr.clips) {
                    cutscene::VideoCue cue;
                    cue.time = c.start_time;
                    cue.video_path = c.asset_path.empty() ? c.name : c.asset_path;
                    vt->AddCue(cue);
                }
                seq->AddTrack(vt);
                break;
            }
            case SeqTrackType::Fade:
            case SeqTrackType::Group:
                break;  // no runtime cutscene equivalent
        }
    }
    return seq;
}

SequencerState SequenceFromRuntime(const cutscene::CutsceneSequence& seq) {
    SequencerState state;
    state.initialized = true;
    state.duration = seq.GetDuration();
    state.view_end = seq.GetDuration();

    for (const auto& track : seq.GetTracks()) {
        if (!track) continue;
        SequencerTrack tr;
        tr.name = track->GetName();

        switch (track->GetType()) {
            case cutscene::TrackType::Camera: {
                tr.type = SeqTrackType::Camera;
                tr.track_color = MakeSeqColor(180, 80, 80);
                const auto* ct = static_cast<const cutscene::CameraTrack*>(track.get());
                // Camera keyframes have position/look_at/fov which the editor
                // doesn't edit; store as property-like keyframes with time only.
                for (const auto& kf : ct->GetKeyframes()) {
                    SequencerKeyframe ek;
                    ek.time = kf.time;
                    ek.value = kf.fov;  // preserve fov as the scalar value
                    tr.keyframes.push_back(ek);
                }
                break;
            }
            case cutscene::TrackType::Property: {
                tr.type = SeqTrackType::Property;
                tr.track_color = MakeSeqColor(80, 150, 80);
                const auto* pt = static_cast<const cutscene::PropertyTrack*>(track.get());
                for (const auto& kf : pt->GetKeyframes()) {
                    SequencerKeyframe ek;
                    ek.time = kf.time;
                    ek.value = kf.value;
                    tr.keyframes.push_back(ek);
                }
                break;
            }
            case cutscene::TrackType::Event: {
                tr.type = SeqTrackType::Event;
                tr.track_color = MakeSeqColor(200, 180, 60);
                const auto* et = static_cast<const cutscene::EventTrack*>(track.get());
                for (const auto& ev : et->GetEvents()) {
                    SequencerClip clip;
                    clip.name = ev.event_name;
                    clip.start_time = ev.time;
                    clip.end_time = ev.time + 0.1f;  // editor clips need a range
                    clip.asset_path = ev.payload;
                    clip.color = MakeSeqColor(255, 200, 50, 200);
                    tr.clips.push_back(std::move(clip));
                }
                break;
            }
            case cutscene::TrackType::Audio: {
                tr.type = SeqTrackType::Audio;
                tr.track_color = MakeSeqColor(80, 120, 200);
                const auto* at = static_cast<const cutscene::AudioTrack*>(track.get());
                for (const auto& cue : at->GetCues()) {
                    SequencerClip clip;
                    clip.name = cue.audio_path;
                    clip.start_time = cue.time;
                    clip.end_time = cue.time + 1.0f;
                    clip.asset_path = cue.audio_path;
                    clip.volume = cue.volume;
                    clip.color = MakeSeqColor(80, 140, 220, 200);
                    tr.clips.push_back(std::move(clip));
                }
                break;
            }
            case cutscene::TrackType::Video: {
                tr.type = SeqTrackType::Video;
                tr.track_color = MakeSeqColor(120, 80, 180);
                const auto* vt = static_cast<const cutscene::VideoTrack*>(track.get());
                for (const auto& cue : vt->GetCues()) {
                    SequencerClip clip;
                    clip.name = cue.video_path;
                    clip.start_time = cue.time;
                    clip.end_time = cue.time + 1.0f;
                    clip.asset_path = cue.video_path;
                    clip.color = MakeSeqColor(120, 100, 200, 200);
                    tr.clips.push_back(std::move(clip));
                }
                break;
            }
        }
        state.tracks.push_back(std::move(tr));
    }
    return state;
}

// ─── Empty state ─────────────────────────────────────────────────────────

SequencerState MakeEmptySequencerState() {
    SequencerState state;
    state.initialized = true;
    state.duration = 10.0f;
    state.view_end = 10.0f;
    state.tracks.clear();
    return state;
}

}  // namespace dse::editor
