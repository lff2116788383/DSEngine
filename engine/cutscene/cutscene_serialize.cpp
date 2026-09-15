/**
 * @file cutscene_serialize.cpp
 * @brief CutsceneSequence 的版本化 .dcutscene 序列化实现
 */

#include "engine/cutscene/cutscene_serialize.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "engine/core/asset_version_envelope.h"
#include "engine/core/asset_dto.h"

namespace dse {
namespace cutscene {

namespace {

using Alloc = rapidjson::Document::AllocatorType;

struct CutsceneSequenceDto {
    std::string name;
    float duration = 0.0f;
};

struct CutsceneTrackDto {
    std::string name;
    std::string type;
};

struct CameraKeyframeDto {
    float time = 0.0f;
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 look_at{0.0f, 0.0f, 0.0f};
    float fov = 60.0f;
    std::string interp = "Linear";
};

struct PropertyKeyframeDto {
    float time = 0.0f;
    float value = 0.0f;
    std::string interp = "Linear";
};

struct EventDto {
    float time = 0.0f;
    std::string event_name;
    std::string payload;
};

struct AudioCueDto {
    float time = 0.0f;
    std::string audio_path;
    float volume = 1.0f;
    bool loop = false;
};

struct VideoCueDto {
    float time = 0.0f;
    std::string video_path;
    bool fullscreen = true;
    float opacity = 1.0f;
    float fade_in = 0.0f;
    float fade_out = 0.0f;
};

constexpr dse::assets::FieldDesc kCutsceneSequenceFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(CutsceneSequenceDto, name)},
    {"duration", dse::assets::FieldType::Float, offsetof(CutsceneSequenceDto, duration)},
};

constexpr dse::assets::FieldDesc kCutsceneTrackFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(CutsceneTrackDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(CutsceneTrackDto, type)},
};

constexpr dse::assets::FieldDesc kCameraKeyframeFields[] = {
    {"time", dse::assets::FieldType::Float, offsetof(CameraKeyframeDto, time)},
    {"position", dse::assets::FieldType::Vec3, offsetof(CameraKeyframeDto, position)},
    {"look_at", dse::assets::FieldType::Vec3, offsetof(CameraKeyframeDto, look_at)},
    {"fov", dse::assets::FieldType::Float, offsetof(CameraKeyframeDto, fov)},
    {"interp", dse::assets::FieldType::String, offsetof(CameraKeyframeDto, interp)},
};

constexpr dse::assets::FieldDesc kPropertyKeyframeFields[] = {
    {"time", dse::assets::FieldType::Float, offsetof(PropertyKeyframeDto, time)},
    {"value", dse::assets::FieldType::Float, offsetof(PropertyKeyframeDto, value)},
    {"interp", dse::assets::FieldType::String, offsetof(PropertyKeyframeDto, interp)},
};

constexpr dse::assets::FieldDesc kEventFields[] = {
    {"time", dse::assets::FieldType::Float, offsetof(EventDto, time)},
    {"event_name", dse::assets::FieldType::String, offsetof(EventDto, event_name)},
    {"payload", dse::assets::FieldType::String, offsetof(EventDto, payload)},
};

constexpr dse::assets::FieldDesc kAudioCueFields[] = {
    {"time", dse::assets::FieldType::Float, offsetof(AudioCueDto, time)},
    {"audio_path", dse::assets::FieldType::String, offsetof(AudioCueDto, audio_path)},
    {"volume", dse::assets::FieldType::Float, offsetof(AudioCueDto, volume)},
    {"loop", dse::assets::FieldType::Bool, offsetof(AudioCueDto, loop)},
};

constexpr dse::assets::FieldDesc kVideoCueFields[] = {
    {"time", dse::assets::FieldType::Float, offsetof(VideoCueDto, time)},
    {"video_path", dse::assets::FieldType::String, offsetof(VideoCueDto, video_path)},
    {"fullscreen", dse::assets::FieldType::Bool, offsetof(VideoCueDto, fullscreen)},
    {"opacity", dse::assets::FieldType::Float, offsetof(VideoCueDto, opacity)},
    {"fade_in", dse::assets::FieldType::Float, offsetof(VideoCueDto, fade_in)},
    {"fade_out", dse::assets::FieldType::Float, offsetof(VideoCueDto, fade_out)},
};


}

const char* InterpModeName(InterpMode mode) {
    switch (mode) {
        case InterpMode::Linear: return "Linear";
        case InterpMode::Step: return "Step";
        case InterpMode::CubicBezier: return "CubicBezier";
    }
    return "Linear";
}

InterpMode InterpModeFromName(const char* name) {
    if (name) {
        if (std::strcmp(name, "Step") == 0) return InterpMode::Step;
        if (std::strcmp(name, "CubicBezier") == 0) return InterpMode::CubicBezier;
    }
    return InterpMode::Linear;
}

const char* TrackTypeName(TrackType type) {
    switch (type) {
        case TrackType::Camera: return "Camera";
        case TrackType::Property: return "Property";
        case TrackType::Event: return "Event";
        case TrackType::Audio: return "Audio";
        case TrackType::Video: return "Video";
    }
    return "Property";
}

void WriteSequenceJson(const CutsceneSequence& seq, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    CutsceneSequenceDto sdto;
    sdto.name = seq.GetName();
    sdto.duration = seq.GetDuration();
    dse::assets::WriteFields(out, alloc, kCutsceneSequenceFields,
                             sizeof(kCutsceneSequenceFields) / sizeof(kCutsceneSequenceFields[0]), &sdto);

    rapidjson::Value tracks(rapidjson::kArrayType);
    for (const auto& track : seq.GetTracks()) {
        if (!track) continue;
        CutsceneTrackDto tdto;
        tdto.name = track->GetName();
        tdto.type = TrackTypeName(track->GetType());
        rapidjson::Value tj(rapidjson::kObjectType);
        dse::assets::WriteFields(tj, alloc, kCutsceneTrackFields,
                                 sizeof(kCutsceneTrackFields) / sizeof(kCutsceneTrackFields[0]), &tdto);

        switch (track->GetType()) {
            case TrackType::Camera: {
                const auto* ct = static_cast<const CameraTrack*>(track.get());
                rapidjson::Value kfs(rapidjson::kArrayType);
                for (const auto& kf : ct->GetKeyframes()) {
                    CameraKeyframeDto kdto;
                    kdto.time = kf.time;
                    kdto.position = kf.position;
                    kdto.look_at = kf.look_at;
                    kdto.fov = kf.fov;
                    kdto.interp = InterpModeName(kf.interp);
                    rapidjson::Value k(rapidjson::kObjectType);
                    dse::assets::WriteFields(k, alloc, kCameraKeyframeFields,
                                             sizeof(kCameraKeyframeFields) / sizeof(kCameraKeyframeFields[0]), &kdto);
                    kfs.PushBack(k, alloc);
                }
                tj.AddMember("keyframes", kfs, alloc);
                break;
            }
            case TrackType::Property: {
                const auto* pt = static_cast<const PropertyTrack*>(track.get());
                rapidjson::Value kfs(rapidjson::kArrayType);
                for (const auto& kf : pt->GetKeyframes()) {
                    PropertyKeyframeDto kdto;
                    kdto.time = kf.time;
                    kdto.value = kf.value;
                    kdto.interp = InterpModeName(kf.interp);
                    rapidjson::Value k(rapidjson::kObjectType);
                    dse::assets::WriteFields(k, alloc, kPropertyKeyframeFields,
                                             sizeof(kPropertyKeyframeFields) / sizeof(kPropertyKeyframeFields[0]), &kdto);
                    kfs.PushBack(k, alloc);
                }
                tj.AddMember("keyframes", kfs, alloc);
                break;
            }
            case TrackType::Event: {
                const auto* et = static_cast<const EventTrack*>(track.get());
                rapidjson::Value events(rapidjson::kArrayType);
                for (const auto& ev : et->GetEvents()) {
                    EventDto edto;
                    edto.time = ev.time;
                    edto.event_name = ev.event_name;
                    edto.payload = ev.payload;
                    rapidjson::Value e(rapidjson::kObjectType);
                    dse::assets::WriteFields(e, alloc, kEventFields,
                                             sizeof(kEventFields) / sizeof(kEventFields[0]), &edto);
                    events.PushBack(e, alloc);
                }
                tj.AddMember("events", events, alloc);
                break;
            }
            case TrackType::Audio: {
                const auto* at = static_cast<const AudioTrack*>(track.get());
                rapidjson::Value cues(rapidjson::kArrayType);
                for (const auto& cue : at->GetCues()) {
                    AudioCueDto cdto;
                    cdto.time = cue.time;
                    cdto.audio_path = cue.audio_path;
                    cdto.volume = cue.volume;
                    cdto.loop = cue.loop;
                    rapidjson::Value c(rapidjson::kObjectType);
                    dse::assets::WriteFields(c, alloc, kAudioCueFields,
                                             sizeof(kAudioCueFields) / sizeof(kAudioCueFields[0]), &cdto);
                    cues.PushBack(c, alloc);
                }
                tj.AddMember("cues", cues, alloc);
                break;
            }
            case TrackType::Video: {
                const auto* vt = static_cast<const VideoTrack*>(track.get());
                rapidjson::Value cues(rapidjson::kArrayType);
                for (const auto& cue : vt->GetCues()) {
                    VideoCueDto cdto;
                    cdto.time = cue.time;
                    cdto.video_path = cue.video_path;
                    cdto.fullscreen = cue.fullscreen;
                    cdto.opacity = cue.opacity;
                    cdto.fade_in = cue.fade_in;
                    cdto.fade_out = cue.fade_out;
                    rapidjson::Value c(rapidjson::kObjectType);
                    dse::assets::WriteFields(c, alloc, kVideoCueFields,
                                             sizeof(kVideoCueFields) / sizeof(kVideoCueFields[0]), &cdto);
                    cues.PushBack(c, alloc);
                }
                tj.AddMember("cues", cues, alloc);
                break;
            }
        }
        tracks.PushBack(tj, alloc);
    }
    out.AddMember("tracks", tracks, alloc);
}

std::shared_ptr<CutsceneSequence> ReadSequenceJson(const rapidjson::Value& in,
                                                   CutsceneDiagnostics& diag) {
    if (!in.IsObject()) {
        diag.errors.push_back("sequence body is not an object");
        return nullptr;
    }

    // ADR-3: .dcutscene read path uses unified DTO/field table.
    CutsceneSequenceDto sdto;
    dse::assets::ReadFields(in, kCutsceneSequenceFields,
                            sizeof(kCutsceneSequenceFields) / sizeof(kCutsceneSequenceFields[0]), &sdto);
    auto seq = std::make_shared<CutsceneSequence>(std::move(sdto.name), sdto.duration);

    if (!in.HasMember("tracks") || !in["tracks"].IsArray()) {
        diag.warnings.push_back("no tracks array; produced empty sequence");
        diag.ok = true;
        return seq;
    }

    for (const auto& tj : in["tracks"].GetArray()) {
        if (!tj.IsObject()) {
            diag.warnings.push_back("skipped non-object track entry");
            continue;
        }
        CutsceneTrackDto tdto;
        dse::assets::ReadFields(tj, kCutsceneTrackFields,
                                sizeof(kCutsceneTrackFields) / sizeof(kCutsceneTrackFields[0]), &tdto);
        const std::string& type_name = tdto.type;

        if (type_name == "Camera") {
            auto ct = std::make_shared<CameraTrack>(std::move(tdto.name));
            if (tj.HasMember("keyframes") && tj["keyframes"].IsArray()) {
                for (const auto& k : tj["keyframes"].GetArray()) {
                    if (!k.IsObject()) continue;
                    CameraKeyframeDto kdto;
                    dse::assets::ReadFields(k, kCameraKeyframeFields,
                                            sizeof(kCameraKeyframeFields) / sizeof(kCameraKeyframeFields[0]), &kdto);
                    CameraKeyframe kf;
                    kf.time = kdto.time;
                    kf.position = kdto.position;
                    kf.look_at = kdto.look_at;
                    kf.fov = kdto.fov;
                    kf.interp = InterpModeFromName(kdto.interp.c_str());
                    ct->AddKeyframe(kf);
                }
            }
            seq->AddTrack(ct);
        } else if (type_name == "Property") {
            auto pt = std::make_shared<PropertyTrack>(std::move(tdto.name));
            if (tj.HasMember("keyframes") && tj["keyframes"].IsArray()) {
                for (const auto& k : tj["keyframes"].GetArray()) {
                    if (!k.IsObject()) continue;
                    PropertyKeyframeDto kdto;
                    dse::assets::ReadFields(k, kPropertyKeyframeFields,
                                            sizeof(kPropertyKeyframeFields) / sizeof(kPropertyKeyframeFields[0]), &kdto);
                    pt->AddKeyframe(kdto.time, kdto.value, InterpModeFromName(kdto.interp.c_str()));
                }
            }
            seq->AddTrack(pt);
        } else if (type_name == "Event") {
            auto et = std::make_shared<EventTrack>(std::move(tdto.name));
            if (tj.HasMember("events") && tj["events"].IsArray()) {
                for (const auto& e : tj["events"].GetArray()) {
                    if (!e.IsObject()) continue;
                    EventDto edto;
                    dse::assets::ReadFields(e, kEventFields,
                                            sizeof(kEventFields) / sizeof(kEventFields[0]), &edto);
                    et->AddEvent(edto.time, std::move(edto.event_name), std::move(edto.payload));
                }
            }
            seq->AddTrack(et);
        } else if (type_name == "Audio") {
            auto at = std::make_shared<AudioTrack>(std::move(tdto.name));
            if (tj.HasMember("cues") && tj["cues"].IsArray()) {
                for (const auto& c : tj["cues"].GetArray()) {
                    if (!c.IsObject()) continue;
                    AudioCueDto cdto;
                    dse::assets::ReadFields(c, kAudioCueFields,
                                            sizeof(kAudioCueFields) / sizeof(kAudioCueFields[0]), &cdto);
                    at->AddCue(cdto.time, std::move(cdto.audio_path), cdto.volume, cdto.loop);
                }
            }
            seq->AddTrack(at);
        } else if (type_name == "Video") {
            auto vt = std::make_shared<VideoTrack>(std::move(tdto.name));
            if (tj.HasMember("cues") && tj["cues"].IsArray()) {
                for (const auto& c : tj["cues"].GetArray()) {
                    if (!c.IsObject()) continue;
                    VideoCueDto cdto;
                    dse::assets::ReadFields(c, kVideoCueFields,
                                            sizeof(kVideoCueFields) / sizeof(kVideoCueFields[0]), &cdto);
                    VideoCue cue;
                    cue.time = cdto.time;
                    cue.video_path = std::move(cdto.video_path);
                    cue.fullscreen = cdto.fullscreen;
                    cue.opacity = cdto.opacity;
                    cue.fade_in = cdto.fade_in;
                    cue.fade_out = cdto.fade_out;
                    vt->AddCue(cue);
                }
            }
            seq->AddTrack(vt);
        } else {
            diag.warnings.push_back("unknown track type '" + type_name + "' skipped");
        }
    }

    diag.ok = true;
    return seq;
}

std::string SerializeSequence(const CutsceneSequence& seq) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();
    dse::assets::WriteVersionEnvelope(doc, kCutsceneSchemaVersion, alloc);
    rapidjson::Value body(rapidjson::kObjectType);
    WriteSequenceJson(seq, body, alloc);
    doc.AddMember("sequence", body, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

std::shared_ptr<CutsceneSequence> DeserializeSequence(const std::string& json,
                                                      CutsceneDiagnostics& diag) {
    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        diag.errors.push_back("JSON parse error");
        return nullptr;
    }
    if (!doc.IsObject()) {
        diag.errors.push_back("root is not an object");
        return nullptr;
    }

    dse::assets::ReadVersionEnvelope(doc, kCutsceneSchemaVersion, ".dcutscene", diag);

    const rapidjson::Value* body = nullptr;
    if (doc.HasMember("sequence") && doc["sequence"].IsObject()) {
        body = &doc["sequence"];
    } else {
        body = &doc;
        if (diag.source_version == 0) diag.migrated = true;
    }

    return ReadSequenceJson(*body, diag);
}

bool SaveSequenceToFile(const CutsceneSequence& seq, const std::string& path,
                        CutsceneDiagnostics& diag) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        diag.errors.push_back("cannot open file for write: " + path);
        return false;
    }
    std::string json = SerializeSequence(seq);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!out.good()) {
        diag.errors.push_back("write failed: " + path);
        return false;
    }
    diag.ok = true;
    return true;
}

std::shared_ptr<CutsceneSequence> LoadSequenceFromFile(const std::string& path,
                                                       CutsceneDiagnostics& diag) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        diag.errors.push_back("cannot open file for read: " + path);
        return nullptr;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return DeserializeSequence(ss.str(), diag);
}

}  // namespace cutscene
}  // namespace dse
