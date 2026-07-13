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

namespace dse {
namespace cutscene {

namespace {

using Alloc = rapidjson::Document::AllocatorType;

rapidjson::Value Str(const std::string& s, Alloc& alloc) {
    return rapidjson::Value(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
}

void WriteVec3(const glm::vec3& v, rapidjson::Value& out, Alloc& alloc) {
    out.SetArray();
    out.PushBack(v.x, alloc).PushBack(v.y, alloc).PushBack(v.z, alloc);
}

bool ReadVec3(const rapidjson::Value& in, glm::vec3& out) {
    if (!in.IsArray() || in.Size() != 3) return false;
    for (rapidjson::SizeType i = 0; i < 3; ++i) {
        if (!in[i].IsNumber()) return false;
    }
    out = glm::vec3(in[0].GetFloat(), in[1].GetFloat(), in[2].GetFloat());
    return true;
}

float ReadFloat(const rapidjson::Value& obj, const char* key, float fallback) {
    if (obj.HasMember(key) && obj[key].IsNumber()) return obj[key].GetFloat();
    return fallback;
}

bool ReadBool(const rapidjson::Value& obj, const char* key, bool fallback) {
    if (obj.HasMember(key) && obj[key].IsBool()) return obj[key].GetBool();
    return fallback;
}

std::string ReadString(const rapidjson::Value& obj, const char* key) {
    if (obj.HasMember(key) && obj[key].IsString()) return obj[key].GetString();
    return std::string();
}

InterpMode ReadInterp(const rapidjson::Value& obj) {
    if (obj.HasMember("interp") && obj["interp"].IsString()) {
        return InterpModeFromName(obj["interp"].GetString());
    }
    return InterpMode::Linear;
}

}  // namespace

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
    out.AddMember("name", Str(seq.GetName(), alloc), alloc);
    out.AddMember("duration", seq.GetDuration(), alloc);

    rapidjson::Value tracks(rapidjson::kArrayType);
    for (const auto& track : seq.GetTracks()) {
        if (!track) continue;
        rapidjson::Value tj(rapidjson::kObjectType);
        tj.AddMember("name", Str(track->GetName(), alloc), alloc);
        tj.AddMember("type", Str(TrackTypeName(track->GetType()), alloc), alloc);

        switch (track->GetType()) {
            case TrackType::Camera: {
                const auto* ct = static_cast<const CameraTrack*>(track.get());
                rapidjson::Value kfs(rapidjson::kArrayType);
                for (const auto& kf : ct->GetKeyframes()) {
                    rapidjson::Value k(rapidjson::kObjectType);
                    k.AddMember("time", kf.time, alloc);
                    rapidjson::Value pos, look;
                    WriteVec3(kf.position, pos, alloc);
                    WriteVec3(kf.look_at, look, alloc);
                    k.AddMember("position", pos, alloc);
                    k.AddMember("look_at", look, alloc);
                    k.AddMember("fov", kf.fov, alloc);
                    k.AddMember("interp", Str(InterpModeName(kf.interp), alloc), alloc);
                    kfs.PushBack(k, alloc);
                }
                tj.AddMember("keyframes", kfs, alloc);
                break;
            }
            case TrackType::Property: {
                const auto* pt = static_cast<const PropertyTrack*>(track.get());
                rapidjson::Value kfs(rapidjson::kArrayType);
                for (const auto& kf : pt->GetKeyframes()) {
                    rapidjson::Value k(rapidjson::kObjectType);
                    k.AddMember("time", kf.time, alloc);
                    k.AddMember("value", kf.value, alloc);
                    k.AddMember("interp", Str(InterpModeName(kf.interp), alloc), alloc);
                    kfs.PushBack(k, alloc);
                }
                tj.AddMember("keyframes", kfs, alloc);
                break;
            }
            case TrackType::Event: {
                const auto* et = static_cast<const EventTrack*>(track.get());
                rapidjson::Value evs(rapidjson::kArrayType);
                for (const auto& ev : et->GetEvents()) {
                    rapidjson::Value e(rapidjson::kObjectType);
                    e.AddMember("time", ev.time, alloc);
                    e.AddMember("event_name", Str(ev.event_name, alloc), alloc);
                    e.AddMember("payload", Str(ev.payload, alloc), alloc);
                    evs.PushBack(e, alloc);
                }
                tj.AddMember("events", evs, alloc);
                break;
            }
            case TrackType::Audio: {
                const auto* at = static_cast<const AudioTrack*>(track.get());
                rapidjson::Value cues(rapidjson::kArrayType);
                for (const auto& cue : at->GetCues()) {
                    rapidjson::Value c(rapidjson::kObjectType);
                    c.AddMember("time", cue.time, alloc);
                    c.AddMember("audio_path", Str(cue.audio_path, alloc), alloc);
                    c.AddMember("volume", cue.volume, alloc);
                    c.AddMember("loop", cue.loop, alloc);
                    cues.PushBack(c, alloc);
                }
                tj.AddMember("cues", cues, alloc);
                break;
            }
            case TrackType::Video: {
                const auto* vt = static_cast<const VideoTrack*>(track.get());
                rapidjson::Value cues(rapidjson::kArrayType);
                for (const auto& cue : vt->GetCues()) {
                    rapidjson::Value c(rapidjson::kObjectType);
                    c.AddMember("time", cue.time, alloc);
                    c.AddMember("video_path", Str(cue.video_path, alloc), alloc);
                    c.AddMember("fullscreen", cue.fullscreen, alloc);
                    c.AddMember("opacity", cue.opacity, alloc);
                    c.AddMember("fade_in", cue.fade_in, alloc);
                    c.AddMember("fade_out", cue.fade_out, alloc);
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

    std::string name = ReadString(in, "name");
    float duration = ReadFloat(in, "duration", 0.0f);
    auto seq = std::make_shared<CutsceneSequence>(name, duration);

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
        std::string track_name = ReadString(tj, "name");
        std::string type_name = ReadString(tj, "type");

        if (type_name == "Camera") {
            auto ct = std::make_shared<CameraTrack>(track_name);
            if (tj.HasMember("keyframes") && tj["keyframes"].IsArray()) {
                for (const auto& k : tj["keyframes"].GetArray()) {
                    if (!k.IsObject()) continue;
                    CameraKeyframe kf;
                    kf.time = ReadFloat(k, "time", 0.0f);
                    if (k.HasMember("position")) ReadVec3(k["position"], kf.position);
                    if (k.HasMember("look_at")) ReadVec3(k["look_at"], kf.look_at);
                    kf.fov = ReadFloat(k, "fov", 60.0f);
                    kf.interp = ReadInterp(k);
                    ct->AddKeyframe(kf);
                }
            }
            seq->AddTrack(ct);
        } else if (type_name == "Property") {
            auto pt = std::make_shared<PropertyTrack>(track_name);
            if (tj.HasMember("keyframes") && tj["keyframes"].IsArray()) {
                for (const auto& k : tj["keyframes"].GetArray()) {
                    if (!k.IsObject()) continue;
                    pt->AddKeyframe(ReadFloat(k, "time", 0.0f),
                                    ReadFloat(k, "value", 0.0f),
                                    ReadInterp(k));
                }
            }
            seq->AddTrack(pt);
        } else if (type_name == "Event") {
            auto et = std::make_shared<EventTrack>(track_name);
            if (tj.HasMember("events") && tj["events"].IsArray()) {
                for (const auto& e : tj["events"].GetArray()) {
                    if (!e.IsObject()) continue;
                    et->AddEvent(ReadFloat(e, "time", 0.0f),
                                 ReadString(e, "event_name"),
                                 ReadString(e, "payload"));
                }
            }
            seq->AddTrack(et);
        } else if (type_name == "Audio") {
            auto at = std::make_shared<AudioTrack>(track_name);
            if (tj.HasMember("cues") && tj["cues"].IsArray()) {
                for (const auto& c : tj["cues"].GetArray()) {
                    if (!c.IsObject()) continue;
                    at->AddCue(ReadFloat(c, "time", 0.0f),
                               ReadString(c, "audio_path"),
                               ReadFloat(c, "volume", 1.0f),
                               ReadBool(c, "loop", false));
                }
            }
            seq->AddTrack(at);
        } else if (type_name == "Video") {
            auto vt = std::make_shared<VideoTrack>(track_name);
            if (tj.HasMember("cues") && tj["cues"].IsArray()) {
                for (const auto& c : tj["cues"].GetArray()) {
                    if (!c.IsObject()) continue;
                    VideoCue cue;
                    cue.time = ReadFloat(c, "time", 0.0f);
                    cue.video_path = ReadString(c, "video_path");
                    cue.fullscreen = ReadBool(c, "fullscreen", true);
                    cue.opacity = ReadFloat(c, "opacity", 1.0f);
                    cue.fade_in = ReadFloat(c, "fade_in", 0.0f);
                    cue.fade_out = ReadFloat(c, "fade_out", 0.0f);
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
    doc.AddMember("version", kCutsceneSchemaVersion, alloc);
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

    diag.source_version = (doc.HasMember("version") && doc["version"].IsInt())
                              ? doc["version"].GetInt()
                              : 0;
    dse::assets::NoteForwardCompat(diag.source_version, kCutsceneSchemaVersion, ".dcutscene", diag);

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
