/**
 * @file editor_snapshot.cpp
 * @brief Registry 快照导出与对比实现
 */

#include "editor_snapshot.h"

#include <cmath>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "engine/ecs/transform.h"
#include "editor_shared_components.h"

namespace dse::editor::test {

namespace {

void WriteVec3(rapidjson::Value& parent,
               const char* name,
               const glm::vec3& value,
               rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(value.x, allocator).PushBack(value.y, allocator).PushBack(value.z, allocator);
    parent.AddMember(rapidjson::Value(name, allocator).Move(), arr, allocator);
}

glm::vec3 ReadVec3(const rapidjson::Value& arr) {
    if (!arr.IsArray() || arr.Size() < 3) return glm::vec3(0.0f);
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

bool Vec3Near(const glm::vec3& a, const glm::vec3& b, float tol) {
    return std::fabs(a.x - b.x) <= tol &&
           std::fabs(a.y - b.y) <= tol &&
           std::fabs(a.z - b.z) <= tol;
}

bool QuatNear(const glm::quat& a, const glm::quat& b, float tol) {
    return std::fabs(a.x - b.x) <= tol &&
           std::fabs(a.y - b.y) <= tol &&
           std::fabs(a.z - b.z) <= tol &&
           std::fabs(a.w - b.w) <= tol;
}

} // namespace

std::string ExportRegistrySnapshot(entt::registry& registry) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    rapidjson::Value entities_arr(rapidjson::kArrayType);
    int alive_count = 0;

    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        ++alive_count;

        rapidjson::Value ent_obj(rapidjson::kObjectType);
        ent_obj.AddMember("id", static_cast<uint32_t>(entity), allocator);

        if (registry.all_of<EditorNameComponent>(entity)) {
            const auto& name = registry.get<EditorNameComponent>(entity).name;
            ent_obj.AddMember("name",
                              rapidjson::Value(name.c_str(), allocator).Move(),
                              allocator);
        }

        if (registry.all_of<TransformComponent>(entity)) {
            const auto& t = registry.get<TransformComponent>(entity);
            rapidjson::Value t_obj(rapidjson::kObjectType);
            WriteVec3(t_obj, "position", t.position, allocator);
            {
                rapidjson::Value rot(rapidjson::kArrayType);
                rot.PushBack(t.rotation.x, allocator)
                   .PushBack(t.rotation.y, allocator)
                   .PushBack(t.rotation.z, allocator)
                   .PushBack(t.rotation.w, allocator);
                t_obj.AddMember("rotation", rot, allocator);
            }
            WriteVec3(t_obj, "scale", t.scale, allocator);
            ent_obj.AddMember("transform", t_obj, allocator);
        }

        entities_arr.PushBack(ent_obj, allocator);
    }

    doc.AddMember("entity_count", alive_count, allocator);
    doc.AddMember("entities", entities_arr, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

std::vector<std::string> CompareSnapshot(const std::string& actual_json,
                                         const std::string& expected_json,
                                         float float_tolerance) {
    std::vector<std::string> diffs;

    rapidjson::Document actual_doc, expected_doc;
    actual_doc.Parse(actual_json.c_str());
    expected_doc.Parse(expected_json.c_str());

    if (actual_doc.HasParseError()) {
        diffs.push_back("Failed to parse actual snapshot JSON");
        return diffs;
    }
    if (expected_doc.HasParseError()) {
        diffs.push_back("Failed to parse expected snapshot JSON");
        return diffs;
    }

    // Compare entity count
    int actual_count = actual_doc.HasMember("entity_count") ? actual_doc["entity_count"].GetInt() : -1;
    int expected_count = expected_doc.HasMember("entity_count") ? expected_doc["entity_count"].GetInt() : -1;
    if (actual_count != expected_count) {
        std::ostringstream oss;
        oss << "Entity count mismatch: actual=" << actual_count << " expected=" << expected_count;
        diffs.push_back(oss.str());
    }

    // Compare entities by index
    if (!actual_doc.HasMember("entities") || !expected_doc.HasMember("entities")) {
        if (!actual_doc.HasMember("entities")) diffs.push_back("Actual snapshot missing 'entities' array");
        if (!expected_doc.HasMember("entities")) diffs.push_back("Expected snapshot missing 'entities' array");
        return diffs;
    }

    const auto& actual_ents = actual_doc["entities"].GetArray();
    const auto& expected_ents = expected_doc["entities"].GetArray();

    size_t min_count = std::min(actual_ents.Size(), expected_ents.Size());
    for (size_t i = 0; i < min_count; ++i) {
        const auto& a = actual_ents[static_cast<rapidjson::SizeType>(i)];
        const auto& e = expected_ents[static_cast<rapidjson::SizeType>(i)];
        std::string prefix = "Entity[" + std::to_string(i) + "] ";

        // Compare name
        bool a_has_name = a.HasMember("name");
        bool e_has_name = e.HasMember("name");
        if (a_has_name != e_has_name) {
            diffs.push_back(prefix + "name presence mismatch");
        } else if (a_has_name && std::string(a["name"].GetString()) != std::string(e["name"].GetString())) {
            diffs.push_back(prefix + "name: actual='" + a["name"].GetString() + "' expected='" + e["name"].GetString() + "'");
        }

        // Compare transform
        bool a_has_t = a.HasMember("transform");
        bool e_has_t = e.HasMember("transform");
        if (a_has_t != e_has_t) {
            diffs.push_back(prefix + "transform presence mismatch");
        } else if (a_has_t) {
            const auto& at = a["transform"];
            const auto& et = e["transform"];

            if (at.HasMember("position") && et.HasMember("position")) {
                glm::vec3 ap = ReadVec3(at["position"]);
                glm::vec3 ep = ReadVec3(et["position"]);
                if (!Vec3Near(ap, ep, float_tolerance)) {
                    std::ostringstream oss;
                    oss << prefix << "position: actual=(" << ap.x << "," << ap.y << "," << ap.z
                        << ") expected=(" << ep.x << "," << ep.y << "," << ep.z << ")";
                    diffs.push_back(oss.str());
                }
            }

            if (at.HasMember("scale") && et.HasMember("scale")) {
                glm::vec3 as = ReadVec3(at["scale"]);
                glm::vec3 es = ReadVec3(et["scale"]);
                if (!Vec3Near(as, es, float_tolerance)) {
                    std::ostringstream oss;
                    oss << prefix << "scale: actual=(" << as.x << "," << as.y << "," << as.z
                        << ") expected=(" << es.x << "," << es.y << "," << es.z << ")";
                    diffs.push_back(oss.str());
                }
            }
        }
    }

    return diffs;
}

size_t CountAliveEntities(entt::registry& registry) {
    size_t count = 0;
    for (auto entity : registry.storage<entt::entity>()) {
        if (registry.valid(entity)) ++count;
    }
    return count;
}

} // namespace dse::editor::test
