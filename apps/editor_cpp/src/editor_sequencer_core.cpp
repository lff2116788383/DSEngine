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
#include "engine/core/asset_dto.h"
#include "engine/cutscene/cutscene_serialize.h"

namespace dse::editor {

namespace {

struct SeqProjectDto {
    float duration = 10.0f;
    float frame_rate = 30.0f;
};

struct SeqClipDto {
    std::string name;
    float start_time = 0.0f;
    float end_time = 1.0f;
    uint64_t color = 0;
    std::string asset_path;
    float volume = 1.0f;
};

struct SeqKeyframeDto {
    float time = 0.0f;
    float value = 0.0f;
    float in_tangent = 0.0f;
    float out_tangent = 0.0f;
};

struct SeqTrackDto {
    std::string name;
    std::string type = "Property";
    bool muted = false;
    bool locked = false;
    bool visible = true;
    bool expanded = true;
    float height = 28.0f;
    uint64_t track_color = 0;
    int group_index = -1;
    std::string target_entity;
    std::string property_path;
};

constexpr dse::assets::FieldDesc kSeqProjectFields[] = {
    {"duration", dse::assets::FieldType::Float, offsetof(SeqProjectDto, duration)},
    {"frame_rate", dse::assets::FieldType::Float, offsetof(SeqProjectDto, frame_rate)},
};

constexpr dse::assets::FieldDesc kSeqTrackFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(SeqTrackDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(SeqTrackDto, type)},
    {"muted", dse::assets::FieldType::Bool, offsetof(SeqTrackDto, muted)},
    {"locked", dse::assets::FieldType::Bool, offsetof(SeqTrackDto, locked)},
    {"visible", dse::assets::FieldType::Bool, offsetof(SeqTrackDto, visible)},
    {"expanded", dse::assets::FieldType::Bool, offsetof(SeqTrackDto, expanded)},
    {"height", dse::assets::FieldType::Float, offsetof(SeqTrackDto, height)},
    {"track_color", dse::assets::FieldType::UInt64, offsetof(SeqTrackDto, track_color)},
    {"group_index", dse::assets::FieldType::Int, offsetof(SeqTrackDto, group_index)},
    {"target_entity", dse::assets::FieldType::String, offsetof(SeqTrackDto, target_entity)},
    {"property_path", dse::assets::FieldType::String, offsetof(SeqTrackDto, property_path)},
};

constexpr dse::assets::FieldDesc kSeqClipFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(SeqClipDto, name)},
    {"start_time", dse::assets::FieldType::Float, offsetof(SeqClipDto, start_time)},
    {"end_time", dse::assets::FieldType::Float, offsetof(SeqClipDto, end_time)},
    {"color", dse::assets::FieldType::UInt64, offsetof(SeqClipDto, color)},
    {"asset_path", dse::assets::FieldType::String, offsetof(SeqClipDto, asset_path)},
    {"volume", dse::assets::FieldType::Float, offsetof(SeqClipDto, volume)},
};

constexpr dse::assets::FieldDesc kSeqKeyframeFields[] = {
    {"time", dse::assets::FieldType::Float, offsetof(SeqKeyframeDto, time)},
    {"value", dse::assets::FieldType::Float, offsetof(SeqKeyframeDto, value)},
    {"in_tangent", dse::assets::FieldType::Float, offsetof(SeqKeyframeDto, in_tangent)},
    {"out_tangent", dse::assets::FieldType::Float, offsetof(SeqKeyframeDto, out_tangent)},
};

}  // namespace


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

    SeqProjectDto project_dto;
    project_dto.duration = state.duration;
    project_dto.frame_rate = state.frame_rate;
    dse::assets::WriteFields(doc, a, kSeqProjectFields,
                             sizeof(kSeqProjectFields) / sizeof(kSeqProjectFields[0]), &project_dto);

    rapidjson::Value tracks(rapidjson::kArrayType);
    for (const auto& tr : state.tracks) {
        SeqTrackDto tdto;
        tdto.name = tr.name;
        tdto.type = SeqTrackTypeName(tr.type);
        tdto.muted = tr.muted;
        tdto.locked = tr.locked;
        tdto.visible = tr.visible;
        tdto.expanded = tr.expanded;
        tdto.height = tr.height;
        tdto.track_color = static_cast<uint64_t>(tr.track_color);
        tdto.group_index = tr.group_index;
        tdto.target_entity = tr.target_entity;
        tdto.property_path = tr.property_path;

        rapidjson::Value tj(rapidjson::kObjectType);
        dse::assets::WriteFields(tj, a, kSeqTrackFields,
                                 sizeof(kSeqTrackFields) / sizeof(kSeqTrackFields[0]), &tdto);

        rapidjson::Value clips(rapidjson::kArrayType);
        for (const auto& c : tr.clips) {
            SeqClipDto cdto;
            cdto.name = c.name;
            cdto.start_time = c.start_time;
            cdto.end_time = c.end_time;
            cdto.color = static_cast<uint64_t>(c.color);
            cdto.asset_path = c.asset_path;
            cdto.volume = c.volume;
            rapidjson::Value cj(rapidjson::kObjectType);
            dse::assets::WriteFields(cj, a, kSeqClipFields,
                                     sizeof(kSeqClipFields) / sizeof(kSeqClipFields[0]), &cdto);
            clips.PushBack(cj, a);
        }
        tj.AddMember("clips", clips, a);

        rapidjson::Value kfs(rapidjson::kArrayType);
        for (const auto& k : tr.keyframes) {
            SeqKeyframeDto kdto;
            kdto.time = k.time;
            kdto.value = k.value;
            kdto.in_tangent = k.in_tangent;
            kdto.out_tangent = k.out_tangent;
            rapidjson::Value kj(rapidjson::kObjectType);
            dse::assets::WriteFields(kj, a, kSeqKeyframeFields,
                                     sizeof(kSeqKeyframeFields) / sizeof(kSeqKeyframeFields[0]), &kdto);
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

    // ADR-3: .dsequence read path uses unified DTO/field table.
    SeqProjectDto project_dto;
    dse::assets::ReadFields(doc, kSeqProjectFields,
                            sizeof(kSeqProjectFields) / sizeof(kSeqProjectFields[0]), &project_dto);

    SequencerState loaded;
    loaded.initialized = true;
    loaded.duration = project_dto.duration;
    loaded.frame_rate = project_dto.frame_rate;
    loaded.view_end = loaded.duration;

    if (doc.HasMember("tracks") && doc["tracks"].IsArray()) {
        for (const auto& tj : doc["tracks"].GetArray()) {
            if (!tj.IsObject()) continue;
            SeqTrackDto tdto;
            dse::assets::ReadFields(tj, kSeqTrackFields,
                                    sizeof(kSeqTrackFields) / sizeof(kSeqTrackFields[0]), &tdto);

            SequencerTrack tr;
            tr.name = std::move(tdto.name);
            tr.type = SeqTrackTypeFromName(tdto.type);
            tr.muted = tdto.muted;
            tr.locked = tdto.locked;
            tr.visible = tdto.visible;
            tr.expanded = tdto.expanded;
            tr.height = tdto.height;
            tr.track_color = static_cast<uint32_t>(tdto.track_color);
            tr.group_index = tdto.group_index;
            tr.target_entity = std::move(tdto.target_entity);
            tr.property_path = std::move(tdto.property_path);

            if (tj.HasMember("clips") && tj["clips"].IsArray()) {
                for (const auto& cj : tj["clips"].GetArray()) {
                    if (!cj.IsObject()) continue;
                    SeqClipDto cdto;
                    dse::assets::ReadFields(cj, kSeqClipFields,
                                            sizeof(kSeqClipFields) / sizeof(kSeqClipFields[0]), &cdto);
                    const bool has_start = cj.HasMember("start_time") && cj["start_time"].IsNumber();
                    const bool has_end = cj.HasMember("end_time") && cj["end_time"].IsNumber();

                    SequencerClip c;
                    c.name = std::move(cdto.name);
                    c.start_time = cdto.start_time;
                    c.end_time = cdto.end_time;
                    c.color = static_cast<uint32_t>(cdto.color);
                    c.asset_path = std::move(cdto.asset_path);
                    c.volume = cdto.volume;

                    // Legacy migration: pre-v1 clips were points with a single
                    // `time` field instead of a start/end range.
                    if (!has_start && cj.HasMember("time") && cj["time"].IsNumber()) {
                        c.start_time = cj["time"].GetFloat();
                        if (!has_end) c.end_time = c.start_time;
                        diag.migrated = true;
                    }
                    tr.clips.push_back(std::move(c));
                }
            }
            if (tj.HasMember("keyframes") && tj["keyframes"].IsArray()) {
                for (const auto& kj : tj["keyframes"].GetArray()) {
                    if (!kj.IsObject()) continue;
                    SeqKeyframeDto kdto;
                    dse::assets::ReadFields(kj, kSeqKeyframeFields,
                                            sizeof(kSeqKeyframeFields) / sizeof(kSeqKeyframeFields[0]), &kdto);
                    SequencerKeyframe k;
                    k.time = kdto.time;
                    k.value = kdto.value;
                    k.in_tangent = kdto.in_tangent;
                    k.out_tangent = kdto.out_tangent;
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
