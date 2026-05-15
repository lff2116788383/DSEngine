#include "dssl_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace dssl {

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim)) {
        auto t = Trim(token);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

static bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool IsSamplerType(const std::string& type) {
    return type == "sampler2D" || type == "samplerCube" || type == "sampler3D";
}

// 提取括号匹配的函数体 { ... }
// 返回 body 内容（不含外层 {}），pos 更新到 } 之后
static std::string ExtractBraceBlock(const std::string& source, size_t& pos) {
    // 找到第一个 {
    auto brace_start = source.find('{', pos);
    if (brace_start == std::string::npos) return "";

    int depth = 0;
    size_t body_start = brace_start + 1;
    for (size_t i = brace_start; i < source.size(); ++i) {
        if (source[i] == '{') depth++;
        else if (source[i] == '}') {
            depth--;
            if (depth == 0) {
                pos = i + 1;
                auto body = source.substr(body_start, i - body_start);
                return Trim(body);
            }
        }
    }
    return ""; // 未闭合
}

// 解析 uniform 声明行:
// uniform <type> <name> [: <hints>] [= <default>];
static bool ParseUniform(const std::string& line, UniformDecl& out) {
    // 去掉末尾分号
    std::string s = line;
    if (!s.empty() && s.back() == ';') s.pop_back();
    s = Trim(s);

    // 去掉 "uniform "
    if (!StartsWith(s, "uniform ")) return false;
    s = Trim(s.substr(8));

    // 提取 type
    auto sp = s.find(' ');
    if (sp == std::string::npos) return false;
    out.type = s.substr(0, sp);
    s = Trim(s.substr(sp));

    // 提取 name (到 : 或 = 或行尾)
    size_t name_end = s.size();
    auto colon_pos = s.find(':');
    auto eq_pos = s.find('=');
    if (colon_pos != std::string::npos) name_end = std::min(name_end, colon_pos);
    if (eq_pos != std::string::npos) name_end = std::min(name_end, eq_pos);
    out.name = Trim(s.substr(0, name_end));
    if (out.name.empty()) return false;

    out.is_sampler = IsSamplerType(out.type);

    // 提取 hints (: 之后, = 之前)
    if (colon_pos != std::string::npos) {
        size_t hints_end = (eq_pos != std::string::npos && eq_pos > colon_pos) ? eq_pos : s.size();
        auto hints_str = Trim(s.substr(colon_pos + 1, hints_end - colon_pos - 1));
        out.hints = Split(hints_str, ',');
    }

    // 提取 default (= 之后)
    if (eq_pos != std::string::npos) {
        out.default_value = Trim(s.substr(eq_pos + 1));
    }

    return true;
}

DSSLModule Parse(const std::string& source, const std::string& filepath) {
    DSSLModule mod;
    mod.source_path = filepath;

    // 预处理: 移除行注释, 保留行结构
    std::istringstream stream(source);
    std::string line;
    std::string cleaned;
    while (std::getline(stream, line)) {
        auto comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        cleaned += line + "\n";
    }

    // 移除块注释 /* ... */
    while (true) {
        auto start = cleaned.find("/*");
        if (start == std::string::npos) break;
        auto end = cleaned.find("*/", start + 2);
        if (end == std::string::npos) {
            mod.error = "Unclosed block comment";
            return mod;
        }
        cleaned.erase(start, end + 2 - start);
    }

    // 逐行 + 逐块解析
    size_t pos = 0;
    while (pos < cleaned.size()) {
        // 跳过空白
        while (pos < cleaned.size() && std::isspace((unsigned char)cleaned[pos])) pos++;
        if (pos >= cleaned.size()) break;

        // 找到当前行或语句
        auto line_end = cleaned.find('\n', pos);
        if (line_end == std::string::npos) line_end = cleaned.size();
        std::string cur_line = Trim(cleaned.substr(pos, line_end - pos));

        // shader_type <type>;
        if (StartsWith(cur_line, "shader_type ")) {
            auto semi = cur_line.find(';');
            if (semi == std::string::npos) {
                mod.error = "Missing semicolon after shader_type";
                return mod;
            }
            auto type_str = Trim(cur_line.substr(12, semi - 12));
            if (type_str == "surface")          mod.shader_type = ShaderType::Surface;
            else if (type_str == "unlit")        mod.shader_type = ShaderType::Unlit;
            else if (type_str == "particle")     mod.shader_type = ShaderType::Particle;
            else if (type_str == "sky")          mod.shader_type = ShaderType::Sky;
            else if (type_str == "postprocess")  mod.shader_type = ShaderType::Postprocess;
            else if (type_str == "canvas")       mod.shader_type = ShaderType::Canvas;
            else {
                mod.error = "Unknown shader_type: " + type_str;
                return mod;
            }
            pos = line_end + 1;
            continue;
        }

        // render_mode <mode1>, <mode2>, ...;
        if (StartsWith(cur_line, "render_mode ")) {
            auto semi = cur_line.find(';');
            if (semi == std::string::npos) {
                mod.error = "Missing semicolon after render_mode";
                return mod;
            }
            auto modes_str = cur_line.substr(12, semi - 12);
            auto modes = Split(modes_str, ',');
            for (auto& m : modes) {
                if (m == "blend_mix" || m == "blend_add" || m == "blend_mul" || m == "blend_disabled")
                    mod.render_modes.blend = m;
                else if (m == "cull_back" || m == "cull_front" || m == "cull_disabled")
                    mod.render_modes.cull = m;
                else if (m == "depth_draw_opaque" || m == "depth_draw_always" || m == "depth_draw_disabled")
                    mod.render_modes.depth_draw = m;
                else if (m == "depth_test_disabled")
                    mod.render_modes.depth_test = false;
                else if (m == "diffuse_burley" || m == "diffuse_lambert" || m == "diffuse_half_lambert")
                    mod.render_modes.diffuse = m;
                else if (m == "specular_schlick_ggx" || m == "specular_disabled")
                    mod.render_modes.specular = m;
                else if (m == "shadows_disabled")
                    mod.render_modes.shadows_enabled = false;
                else if (m == "alpha_test")
                    mod.render_modes.alpha_test = true;
                else if (m == "wireframe")
                    mod.render_modes.wireframe = true;
                else if (m == "lighting_model_watercolor")
                    mod.render_modes.diffuse = m;
            }
            pos = line_end + 1;
            continue;
        }

        // uniform 声明
        if (StartsWith(cur_line, "uniform ")) {
            UniformDecl u;
            if (!ParseUniform(cur_line, u)) {
                mod.error = "Failed to parse uniform: " + cur_line;
                return mod;
            }
            mod.uniforms.push_back(std::move(u));
            pos = line_end + 1;
            continue;
        }

        // 函数体: void vertex() { ... }
        if (StartsWith(cur_line, "void vertex()") || StartsWith(cur_line, "void vertex ()")) {
            mod.vertex_body = ExtractBraceBlock(cleaned, pos);
            continue;
        }
        if (StartsWith(cur_line, "void surface()") || StartsWith(cur_line, "void surface ()")) {
            mod.surface_body = ExtractBraceBlock(cleaned, pos);
            continue;
        }
        if (StartsWith(cur_line, "void light()") || StartsWith(cur_line, "void light ()")) {
            mod.light_body = ExtractBraceBlock(cleaned, pos);
            continue;
        }
        if (StartsWith(cur_line, "void postprocess()") || StartsWith(cur_line, "void postprocess ()")) {
            mod.postprocess_body = ExtractBraceBlock(cleaned, pos);
            continue;
        }

        // 空行或无法识别的内容，跳过
        pos = line_end + 1;
    }

    // 验证
    if (mod.shader_type == ShaderType::Surface && mod.surface_body.empty()) {
        mod.error = "surface shader requires a surface() function";
    }
    if (mod.shader_type == ShaderType::Postprocess && mod.postprocess_body.empty()) {
        mod.error = "postprocess shader requires a postprocess() function";
    }

    return mod;
}

} // namespace dssl
