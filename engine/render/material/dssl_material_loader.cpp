#include "dssl_material_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace dse {
namespace render {

// ============================================================================
// 简化的 DSSL 解析（只需 uniform 信息和 render_mode，不需要函数体）
// ============================================================================

static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool IsSamplerType(const std::string& type) {
    return type.find("sampler") != std::string::npos;
}

static float ParseFloatDefault(const std::string& val) {
    try { return std::stof(val); } catch (...) { return 0.0f; }
}

static glm::vec4 ParseVec4Default(const std::string& val) {
    // 匹配 vec4(x, y, z, w) 或 vec4(x)
    glm::vec4 result(0.0f);
    size_t paren_start = val.find('(');
    size_t paren_end = val.rfind(')');
    if (paren_start == std::string::npos || paren_end == std::string::npos) return result;
    std::string inner = val.substr(paren_start + 1, paren_end - paren_start - 1);
    // 分割逗号
    std::vector<float> components;
    std::istringstream ss(inner);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try { components.push_back(std::stof(Trim(token))); } catch (...) { components.push_back(0.0f); }
    }
    if (components.size() == 1) {
        result = glm::vec4(components[0]);
    } else if (components.size() == 3) {
        result = glm::vec4(components[0], components[1], components[2], 1.0f);
    } else if (components.size() >= 4) {
        result = glm::vec4(components[0], components[1], components[2], components[3]);
    }
    return result;
}

bool DSSLMaterialLoader::ParseDSSLForTemplate(const std::string& source,
                                                const std::string& filepath,
                                                DSSLTemplate& out) {
    std::istringstream stream(source);
    std::string line;

    out.shader_type = DSSLShaderType::Surface;
    out.render_modes = {};

    bool saw_content = false;  // 是否出现任何有效（非空、非注释）内容

    while (std::getline(stream, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || StartsWith(trimmed, "//")) continue;
        saw_content = true;

        // shader_type
        if (StartsWith(trimmed, "shader_type")) {
            if (trimmed.find("unlit") != std::string::npos) out.shader_type = DSSLShaderType::Unlit;
            else if (trimmed.find("particle") != std::string::npos) out.shader_type = DSSLShaderType::Particle;
            else if (trimmed.find("sky") != std::string::npos) out.shader_type = DSSLShaderType::Sky;
            else if (trimmed.find("postprocess") != std::string::npos) out.shader_type = DSSLShaderType::Postprocess;
            else if (trimmed.find("canvas") != std::string::npos) out.shader_type = DSSLShaderType::Canvas;
            continue;
        }

        // render_mode
        if (StartsWith(trimmed, "render_mode")) {
            if (trimmed.find("blend_mix") != std::string::npos) out.render_modes.blend = "mix";
            else if (trimmed.find("blend_add") != std::string::npos) out.render_modes.blend = "add";
            if (trimmed.find("cull_disabled") != std::string::npos) out.render_modes.cull = "disabled";
            else if (trimmed.find("cull_front") != std::string::npos) out.render_modes.cull = "front";
            if (trimmed.find("alpha_test") != std::string::npos) out.render_modes.alpha_test = true;
            if (trimmed.find("shadows_disabled") != std::string::npos) out.render_modes.shadows_enabled = false;
            if (trimmed.find("diffuse_lambert") != std::string::npos) out.render_modes.lighting_model = "toon";
            if (trimmed.find("lighting_model_watercolor") != std::string::npos) out.render_modes.lighting_model = "watercolor";
            continue;
        }

        // uniform
        if (StartsWith(trimmed, "uniform")) {
            // uniform <type> <name> [: hints] [= default];
            DSSLUniformInfo info;

            // 去掉 "uniform "
            std::string rest = trimmed.substr(8);
            // 去掉尾部分号
            if (!rest.empty() && rest.back() == ';') rest.pop_back();

            // 提取默认值
            std::string default_str;
            size_t eq_pos = rest.find('=');
            if (eq_pos != std::string::npos) {
                default_str = Trim(rest.substr(eq_pos + 1));
                rest = Trim(rest.substr(0, eq_pos));
            }

            // 提取 hints
            size_t colon_pos = rest.find(':');
            std::string hints_str;
            if (colon_pos != std::string::npos) {
                hints_str = Trim(rest.substr(colon_pos + 1));
                rest = Trim(rest.substr(0, colon_pos));
            }

            // rest 现在是 "<type> <name>"
            size_t space_pos = rest.find(' ');
            if (space_pos == std::string::npos) continue;
            info.type = Trim(rest.substr(0, space_pos));
            info.name = Trim(rest.substr(space_pos + 1));
            info.is_sampler = IsSamplerType(info.type);
            info.default_value = default_str;

            // 解析 hints
            if (!hints_str.empty()) {
                std::istringstream hint_stream(hints_str);
                std::string hint;
                while (std::getline(hint_stream, hint, ',')) {
                    info.hints.push_back(Trim(hint));
                }
            }

            out.uniform_infos.push_back(info);

            // 解析默认值
            if (!default_str.empty() && !info.is_sampler) {
                if (info.type == "float" || info.type == "int") {
                    out.default_floats[info.name] = ParseFloatDefault(default_str);
                } else if (info.type == "vec4") {
                    out.default_vec4s[info.name] = ParseVec4Default(default_str);
                } else if (info.type == "vec3") {
                    // vec3 复用 vec4 解析：解析器只取括号内分量、忽略类型前缀，
                    // 缺省的第 4 分量按 w=1.0 补齐。
                    out.default_vec4s[info.name] = ParseVec4Default(default_str);
                }
            }
            continue;
        }

        // 函数体开始 → 停止解析 header
        if (StartsWith(trimmed, "void ")) break;
    }

    // 空文件或纯注释文件不是有效的 DSSL 材质，上报失败而非返回一个全默认的空模板。
    if (!saw_content) return false;

    return true;
}

// ============================================================================
// Public API
// ============================================================================

DSSLMaterialLoader& DSSLMaterialLoader::Instance() {
    static DSSLMaterialLoader loader;
    return loader;
}

std::shared_ptr<DSSLMaterialInstance> DSSLMaterialLoader::LoadFromFile(
    const std::string& dssl_path, AssetManager* /*asset_mgr*/) {

    // 检查缓存
    auto cache_it = template_cache_.find(dssl_path);
    if (cache_it == template_cache_.end()) {
        // 读取文件
        std::ifstream f(dssl_path);
        if (!f.is_open()) return nullptr;
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string source = ss.str();

        DSSLTemplate tmpl;
        if (!ParseDSSLForTemplate(source, dssl_path, tmpl)) return nullptr;
        template_cache_[dssl_path] = std::move(tmpl);
        cache_it = template_cache_.find(dssl_path);
    }

    return CreateInstance(dssl_path);
}

std::shared_ptr<DSSLMaterialInstance> DSSLMaterialLoader::CreateInstance(
    const std::string& dssl_path, AssetManager* /*asset_mgr*/) {

    auto cache_it = template_cache_.find(dssl_path);
    if (cache_it == template_cache_.end()) {
        // 尝试加载
        std::ifstream f(dssl_path);
        if (!f.is_open()) return nullptr;
        std::ostringstream ss;
        ss << f.rdbuf();

        DSSLTemplate tmpl;
        if (!ParseDSSLForTemplate(ss.str(), dssl_path, tmpl)) return nullptr;
        template_cache_[dssl_path] = std::move(tmpl);
        cache_it = template_cache_.find(dssl_path);
    }

    const DSSLTemplate& tmpl = cache_it->second;

    unsigned int id = next_id_++;
    auto inst = std::make_shared<DSSLMaterialInstance>(id, dssl_path);
    inst->SetShaderType(tmpl.shader_type);
    inst->SetUniformInfos(tmpl.uniform_infos);
    inst->SetRenderModes(tmpl.render_modes);

    // 设置默认值
    for (auto& [name, val] : tmpl.default_floats) {
        inst->SetFloat(name, val);
    }
    for (auto& [name, val] : tmpl.default_vec4s) {
        inst->SetVec4(name, val);
    }

    instances_[id] = inst;
    return inst;
}

std::shared_ptr<DSSLMaterialInstance> DSSLMaterialLoader::GetInstance(unsigned int id) {
    auto it = instances_.find(id);
    return it != instances_.end() ? it->second : nullptr;
}

void DSSLMaterialLoader::Clear() {
    instances_.clear();
    template_cache_.clear();
}

// ============================================================================
// .dmat JSON → DSSLMaterialInstance binding
// ============================================================================

// Minimal JSON value parser (no external dependency)
namespace {

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    double AsNumber(double fallback = 0.0) const { return type == Number ? num_val : fallback; }
    const std::string& AsString() const { return str_val; }
    bool AsBool() const { return type == Bool ? bool_val : false; }
    const JsonValue* Get(const std::string& key) const {
        if (type != Object) return nullptr;
        for (auto& [k, v] : obj) { if (k == key) return &v; }
        return nullptr;
    }
    const JsonValue* At(size_t i) const { return (type == Array && i < arr.size()) ? &arr[i] : nullptr; }
    size_t Size() const { return type == Array ? arr.size() : 0; }
};

static void SkipWS(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
}

static JsonValue ParseJsonValue(const std::string& s, size_t& i);

static std::string ParseJsonString(const std::string& s, size_t& i) {
    if (i >= s.size() || s[i] != '"') return "";
    ++i;
    std::string result;
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) { result += s[i + 1]; i += 2; }
        else { result += s[i]; ++i; }
    }
    if (i < s.size()) ++i; // skip closing "
    return result;
}

static JsonValue ParseJsonValue(const std::string& s, size_t& i) {
    SkipWS(s, i);
    if (i >= s.size()) return {};

    JsonValue val;
    if (s[i] == '"') {
        val.type = JsonValue::String;
        val.str_val = ParseJsonString(s, i);
    } else if (s[i] == '{') {
        val.type = JsonValue::Object;
        ++i;
        SkipWS(s, i);
        while (i < s.size() && s[i] != '}') {
            SkipWS(s, i);
            std::string key = ParseJsonString(s, i);
            SkipWS(s, i);
            if (i < s.size() && s[i] == ':') ++i;
            val.obj.push_back({key, ParseJsonValue(s, i)});
            SkipWS(s, i);
            if (i < s.size() && s[i] == ',') ++i;
        }
        if (i < s.size()) ++i;
    } else if (s[i] == '[') {
        val.type = JsonValue::Array;
        ++i;
        SkipWS(s, i);
        while (i < s.size() && s[i] != ']') {
            val.arr.push_back(ParseJsonValue(s, i));
            SkipWS(s, i);
            if (i < s.size() && s[i] == ',') ++i;
        }
        if (i < s.size()) ++i;
    } else if (s[i] == 't') {
        val.type = JsonValue::Bool; val.bool_val = true; i += 4;
    } else if (s[i] == 'f') {
        val.type = JsonValue::Bool; val.bool_val = false; i += 5;
    } else if (s[i] == 'n') {
        i += 4;
    } else {
        val.type = JsonValue::Number;
        size_t start = i;
        while (i < s.size() && (s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E'
               || (s[i] >= '0' && s[i] <= '9'))) ++i;
        try { val.num_val = std::stod(s.substr(start, i - start)); } catch (...) {}
    }
    return val;
}

} // anonymous namespace

std::vector<std::shared_ptr<DSSLMaterialInstance>> DSSLMaterialLoader::LoadFromDmat(
    const std::string& dmat_path, const std::string& dssl_search_dir, AssetManager* asset_mgr) {

    std::vector<std::shared_ptr<DSSLMaterialInstance>> results;

    std::ifstream f(dmat_path);
    if (!f.is_open()) return results;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string json_str = ss.str();

    size_t pos = 0;
    JsonValue root = ParseJsonValue(json_str, pos);
    const JsonValue* materials = root.Get("materials");
    if (!materials || materials->type != JsonValue::Array) return results;

    // Resolve DSSL template search path
    std::string search_dir = dssl_search_dir;
    if (search_dir.empty()) {
        // Default: look for dssl files relative to the dmat file's directory
        std::filesystem::path p(dmat_path);
        search_dir = p.parent_path().string();
    }

    for (size_t mi = 0; mi < materials->Size(); ++mi) {
        const JsonValue* mat = materials->At(mi);
        if (!mat || mat->type != JsonValue::Object) continue;

        // Get DSSL template name
        const JsonValue* tmpl_val = mat->Get("dssl_template");
        std::string tmpl_name = tmpl_val ? tmpl_val->AsString() : "pbr_default";

        // Find .dssl file: search in dssl_search_dir, then engine shaders dir
        std::string dssl_path;
        std::vector<std::string> search_paths = {
            search_dir + "/" + tmpl_name + ".dssl",
            search_dir + "/shaders/dssl/" + tmpl_name + ".dssl",
        };
        // Also try engine built-in shaders path
        std::filesystem::path engine_dssl = std::filesystem::path(dmat_path).parent_path();
        // Walk up to find engine/render/shaders/dssl
        for (int depth = 0; depth < 6; ++depth) {
            std::filesystem::path candidate = engine_dssl / "engine" / "render" / "shaders" / "dssl" / (tmpl_name + ".dssl");
            if (std::filesystem::exists(candidate)) {
                search_paths.push_back(candidate.string());
                break;
            }
            engine_dssl = engine_dssl.parent_path();
        }

        for (auto& sp : search_paths) {
            if (std::filesystem::exists(sp)) { dssl_path = sp; break; }
        }

        if (dssl_path.empty()) {
            // Fallback: create instance with default PBR settings (no .dssl file needed)
            unsigned int id = next_id_++;
            auto inst = std::make_shared<DSSLMaterialInstance>(id, tmpl_name);
            inst->SetShaderType(DSSLShaderType::Surface);
            instances_[id] = inst;
            // Still bind parameters below
            results.push_back(inst);
        } else {
            auto inst = CreateInstance(dssl_path, asset_mgr);
            if (!inst) continue;
            results.push_back(inst);
        }

        auto& inst = results.back();

        // Bind PBR parameters from .dmat JSON to DSSLMaterialInstance uniforms
        const JsonValue* bc = mat->Get("base_color");
        if (bc && bc->Size() >= 4) {
            inst->SetVec4("albedo_color", glm::vec4(
                static_cast<float>(bc->At(0)->AsNumber(1.0)),
                static_cast<float>(bc->At(1)->AsNumber(1.0)),
                static_cast<float>(bc->At(2)->AsNumber(1.0)),
                static_cast<float>(bc->At(3)->AsNumber(1.0))));
        }

        const JsonValue* met = mat->Get("metallic");
        if (met) inst->SetFloat("metallic", static_cast<float>(met->AsNumber(0.0)));

        const JsonValue* rough = mat->Get("roughness");
        if (rough) inst->SetFloat("roughness", static_cast<float>(rough->AsNumber(0.5)));

        const JsonValue* em = mat->Get("emissive");
        if (em && em->Size() >= 3) {
            float er = static_cast<float>(em->At(0)->AsNumber());
            float eg = static_cast<float>(em->At(1)->AsNumber());
            float eb = static_cast<float>(em->At(2)->AsNumber());
            float strength = std::max({er, eg, eb});
            if (strength > 0.01f) {
                inst->SetVec4("emission_color", glm::vec4(er, eg, eb, 1.0f));
                inst->SetFloat("emission_strength", strength);
            }
        }

        const JsonValue* ac = mat->Get("alpha_cutoff");
        if (ac) inst->SetFloat("alpha_cutoff", static_cast<float>(ac->AsNumber(0.5)));

        // Set render modes based on material properties
        const JsonValue* ds = mat->Get("double_sided");
        const JsonValue* at = mat->Get("alpha_test");
        if (ds && ds->AsBool()) {
            DSSLMaterialInstance::RenderModes modes = inst->GetRenderModes();
            modes.cull = "disabled";
            inst->SetRenderModes(modes);
        }
        if (at && at->AsBool()) {
            DSSLMaterialInstance::RenderModes modes = inst->GetRenderModes();
            modes.alpha_test = true;
            inst->SetRenderModes(modes);
        }
    }

    return results;
}

} // namespace render
} // namespace dse
