#include "dssl_material_loader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

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

    while (std::getline(stream, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || StartsWith(trimmed, "//")) continue;

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
                    glm::vec4 v = ParseVec4Default("vec4" + default_str.substr(4)); // hack: reuse vec4 parser
                    out.default_vec4s[info.name] = v;
                }
            }
            continue;
        }

        // 函数体开始 → 停止解析 header
        if (StartsWith(trimmed, "void ")) break;
    }

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

} // namespace render
} // namespace dse
