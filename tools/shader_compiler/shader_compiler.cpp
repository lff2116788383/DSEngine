/**
 * @file shader_compiler.cpp
 * @brief DSEngine 离线 Shader 交叉编译工具
 *
 * 编译流水线：
 *   GLSL 450 源文件 → glslang → SPIR-V
 *                              → spirv-cross → GLSL 330 (OpenGL)
 *                              → spirv-cross → HLSL SM5.0 (DX11)
 *
 * 输出模式：
 *   --embed: 生成 C++ 头文件，constexpr const char* / const uint32_t[] 内嵌
 *   默认:    生成独立 .spv / .glsl / .hlsl 文件
 */

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <utility>

#ifdef DSE_HAS_D3DCOMPILE
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace fs = std::filesystem;

// ============================================================================
// 工具函数
// ============================================================================

static std::string ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot open: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static bool WriteFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot write: " << path << "\n";
        return false;
    }
    file.write(content.data(), content.size());
    return true;
}

static bool WriteBinary(const fs::path& path, const std::vector<uint32_t>& data) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Cannot write: " << path << "\n";
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint32_t));
    return true;
}

static EShLanguage GetShaderStage(const std::string& ext) {
    if (ext == ".vert") return EShLangVertex;
    if (ext == ".frag") return EShLangFragment;
    if (ext == ".comp") return EShLangCompute;
    if (ext == ".geom") return EShLangGeometry;
    if (ext == ".tesc") return EShLangTessControl;
    if (ext == ".tese") return EShLangTessEvaluation;
    return EShLangVertex;
}

static std::string SanitizeIdentifier(const std::string& name) {
    std::string result = name;
    for (auto& c : result) {
        if (!isalnum(c) && c != '_') c = '_';
    }
    return result;
}

// ============================================================================
// glslang: GLSL 450 → SPIR-V
// ============================================================================

static bool CompileToSpirv(const std::string& source, EShLanguage stage,
                           const std::string& filename,
                           std::vector<uint32_t>& spirv_out,
                           const std::string& preamble = "") {
    const char* src_cstr = source.c_str();
    const char* names[] = { filename.c_str() };

    glslang::TShader shader(stage);
    shader.setStringsWithLengthsAndNames(&src_cstr, nullptr, names, 1);
    if (!preamble.empty()) {
        shader.setPreamble(preamble.c_str());
    }
    // Vulkan 1.1 / SPIR-V 1.3: 支持 DrawParameters built-in (gl_BaseInstance 等)
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 110);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    const TBuiltInResource* resources = GetDefaultResources();
    if (!shader.parse(resources, 100, false, EShMsgDefault)) {
        std::cerr << "[ERROR] glslang parse failed: " << filename << "\n";
        std::cerr << shader.getInfoLog() << "\n";
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        std::cerr << "[ERROR] glslang link failed: " << filename << "\n";
        std::cerr << program.getInfoLog() << "\n";
        return false;
    }

    glslang::SpvOptions spv_options;
    spv_options.generateDebugInfo = false;
    spv_options.disableOptimizer = false;
    spv_options.optimizeSize = false;

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv_out, &spv_options);
    return !spirv_out.empty();
}

// ============================================================================
// spirv-cross: SPIR-V → GLSL 430
// ============================================================================

// 后处理: 将 GLSL 430 中的 "struct PushConstants {...}; uniform PushConstants pc;"
// 展平为独立 uniform 声明，将 "pc.member" 替换为 "member"。
static std::string FlattenPushConstantsInGLSL(const std::string& src) {
    std::string result = src;

    // 查找 "struct PushConstants" block
    auto struct_pos = result.find("struct PushConstants");
    if (struct_pos == std::string::npos) return result;

    // 找到对应的 '{' 和 '}'
    auto brace_open = result.find('{', struct_pos);
    auto brace_close = result.find("};", brace_open);
    if (brace_open == std::string::npos || brace_close == std::string::npos) return result;

    // 提取 struct 成员列表
    std::string members_block = result.substr(brace_open + 1, brace_close - brace_open - 1);

    // 解析每个成员 "TYPE NAME;"
    std::vector<std::pair<std::string, std::string>> members; // (type, name)
    std::istringstream mss(members_block);
    std::string line;
    while (std::getline(mss, line)) {
        // 去空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty() || line[0] == '/') continue;  // 跳过注释/空行
        // 期望 "TYPE NAME;"
        auto semi = line.find(';');
        if (semi == std::string::npos) continue;
        std::string decl = line.substr(0, semi);
        auto last_space = decl.find_last_of(" \t");
        if (last_space == std::string::npos) continue;
        std::string type = decl.substr(0, last_space);
        std::string name = decl.substr(last_space + 1);
        // 去 type 末尾空白
        while (!type.empty() && (type.back() == ' ' || type.back() == '\t'))
            type.pop_back();
        members.push_back({type, name});
    }

    // 生成独立 uniform 声明
    std::string uniform_block;
    for (auto& [type, name] : members) {
        uniform_block += "uniform " + type + " " + name + ";\n";
    }

    // 删除 "struct PushConstants {...};\n"
    auto struct_end = brace_close + 2; // 跳过 "};"
    while (struct_end < result.size() && (result[struct_end] == '\n' || result[struct_end] == '\r'))
        struct_end++;
    result.erase(struct_pos, struct_end - struct_pos);

    // 删除 "uniform PushConstants pc;\n"
    auto uniform_pos = result.find("uniform PushConstants pc;");
    if (uniform_pos != std::string::npos) {
        auto uniform_end = uniform_pos + std::string("uniform PushConstants pc;").size();
        while (uniform_end < result.size() && (result[uniform_end] == '\n' || result[uniform_end] == '\r'))
            uniform_end++;
        result.replace(uniform_pos, uniform_end - uniform_pos, uniform_block);
    }

    // 替换所有 "pc.member" → "member"
    for (auto& [type, name] : members) {
        std::string from = "pc." + name;
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.size(), name);
            pos += name.size();
        }
    }

    return result;
}

static std::string CrossCompileToGLSL430(const std::vector<uint32_t>& spirv,
                                          EShLanguage stage) {
    try {
        spirv_cross::CompilerGLSL compiler(spirv);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 430;
        options.es = false;
        options.vulkan_semantics = false;
        options.enable_420pack_extension = false;
        compiler.set_common_options(options);

        auto resources = compiler.get_shader_resources();

        // 将 push_constant 映射为普通 uniform
        for (auto& pc : resources.push_constant_buffers) {
            compiler.set_decoration(pc.id, spv::DecorationBinding, 0);
        }

        // BoneMatrices / MorphWeights / PointLights / SpotLights 保持为 UBO 块
        // OpenGL 后端将通过 glBindBufferBase 绑定（与 PerFrame/PerScene/PerMaterial 一致）

        std::string glsl = compiler.compile();

        // 后处理: 展平 PushConstants struct → 独立 uniform
        glsl = FlattenPushConstantsInGLSL(glsl);

        return glsl;
    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "[ERROR] spirv-cross GLSL: " << e.what() << "\n";
        return "";
    }
}

// ============================================================================
// spirv-cross: SPIR-V → ESSL 310 (OpenGL ES 3.1, Android)
// ============================================================================

static std::string CrossCompileToESSL310(const std::vector<uint32_t>& spirv,
                                          EShLanguage stage) {
    try {
        spirv_cross::CompilerGLSL compiler(spirv);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 310;
        options.es = true;
        options.vulkan_semantics = false;
        options.enable_420pack_extension = false;
        compiler.set_common_options(options);

        auto resources = compiler.get_shader_resources();
        for (auto& pc : resources.push_constant_buffers) {
            compiler.set_decoration(pc.id, spv::DecorationBinding, 0);
        }

        std::string essl = compiler.compile();
        essl = FlattenPushConstantsInGLSL(essl);
        return essl;
    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "[ERROR] spirv-cross ESSL310: " << e.what() << "\n";
        return "";
    }
}

// ============================================================================
// HLSL 后处理: 合并超限 sampler (SM5.0 最多 16 个 s0-s15)
// ============================================================================

static std::string ConsolidateHLSLSamplers(const std::string& hlsl) {
    // 阶段 1: 按行扫描，收集所有 sampler 声明
    struct SamplerInfo {
        std::string type;  // "SamplerState" or "SamplerComparisonState"
        std::string name;
        int reg;
    };
    std::vector<SamplerInfo> all_samplers;
    std::vector<std::string> lines;

    std::istringstream stream(hlsl);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
        std::string sampler_type;
        size_t type_pos;
        if ((type_pos = line.find("SamplerComparisonState ")) != std::string::npos) {
            sampler_type = "SamplerComparisonState";
        } else if ((type_pos = line.find("SamplerState ")) != std::string::npos) {
            sampler_type = "SamplerState";
        } else {
            continue;
        }
        size_t name_start = type_pos + sampler_type.size() + 1;
        size_t name_end = line.find(' ', name_start);
        if (name_end == std::string::npos) continue;
        std::string name = line.substr(name_start, name_end - name_start);
        // 去除数组后缀 [N]，只保留基名用于引用替换
        auto bracket = name.find('[');
        if (bracket != std::string::npos) name = name.substr(0, bracket);
        size_t reg_pos = line.find("register(s", name_end);
        if (reg_pos == std::string::npos) continue;
        int reg = std::atoi(line.c_str() + reg_pos + 10);
        all_samplers.push_back({sampler_type, name, reg});
    }

    // 检查是否有超限
    bool has_overflow = false;
    for (auto& s : all_samplers) {
        if (s.reg >= 16) { has_overflow = true; break; }
    }
    if (!has_overflow) return hlsl;

    // 找同类型低位 sampler 作为合并目标
    std::string target_sampler, target_cmp_sampler;
    for (auto& s : all_samplers) {
        if (s.reg < 16) {
            if (s.type == "SamplerState" && target_sampler.empty())
                target_sampler = s.name;
            else if (s.type == "SamplerComparisonState" && target_cmp_sampler.empty())
                target_cmp_sampler = s.name;
        }
    }

    // 建立 name → target 映射（仅超限的），同时记录源是否为数组
    struct RemapEntry {
        std::string from;
        std::string to;
        bool source_is_array;
    };
    std::vector<RemapEntry> remap;
    std::vector<std::string> remove_names; // 需要删除声明的 sampler 名
    for (auto& s : all_samplers) {
        if (s.reg < 16) continue;
        std::string target = (s.type == "SamplerComparisonState") ? target_cmp_sampler : target_sampler;
        if (target.empty()) continue;
        // 检查原始声明是否为数组
        bool is_array = false;
        for (auto& ln : lines) {
            if (ln.find(s.name) != std::string::npos &&
                (ln.find("SamplerState ") != std::string::npos || ln.find("SamplerComparisonState ") != std::string::npos) &&
                ln.find("register(s") != std::string::npos) {
                is_array = (ln.find(s.name + "[") != std::string::npos);
                break;
            }
        }
        remap.push_back({s.name, target, is_array});
        remove_names.push_back(s.name);
    }
    // 按名称长度降序排列，避免短名替换到长名内部
    std::sort(remap.begin(), remap.end(),
        [](const auto& a, const auto& b) { return a.from.size() > b.from.size(); });

    // 阶段 2: 按行重建，跳过超限 sampler 声明行，并做名称替换
    auto is_removed_decl = [&](const std::string& ln) {
        for (auto& n : remove_names) {
            if (ln.find(n) != std::string::npos &&
                (ln.find("SamplerState ") != std::string::npos ||
                 ln.find("SamplerComparisonState ") != std::string::npos) &&
                ln.find("register(s") != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    auto replace_names = [&](std::string& ln) {
        for (auto& entry : remap) {
            size_t p = 0;
            while ((p = ln.find(entry.from, p)) != std::string::npos) {
                bool before_ok = (p == 0) || (!isalnum((unsigned char)ln[p-1]) && ln[p-1] != '_');
                size_t ep = p + entry.from.size();
                bool after_ok = (ep >= ln.size()) || (!isalnum((unsigned char)ln[ep]) && ln[ep] != '_');
                if (before_ok && after_ok) {
                    // 数组 sampler → 标量目标：同时移除紧随的 [index]
                    size_t replace_len = entry.from.size();
                    if (entry.source_is_array && ep < ln.size() && ln[ep] == '[') {
                        auto close = ln.find(']', ep);
                        if (close != std::string::npos)
                            replace_len = close + 1 - p;
                    }
                    ln.replace(p, replace_len, entry.to);
                    p += entry.to.size();
                } else {
                    p += entry.from.size();
                }
            }
        }
    };

    std::ostringstream out;
    for (auto& ln : lines) {
        if (is_removed_decl(ln)) continue; // 跳过超限声明
        replace_names(ln);
        out << ln << "\n";
    }
    return out.str();
}

static std::string FindByteAddressBufferName(const std::string& hlsl, int reg) {
    const std::string reg_token = "register(t" + std::to_string(reg) + ")";
    size_t pos = 0;
    while ((pos = hlsl.find("ByteAddressBuffer ", pos)) != std::string::npos) {
        size_t name_start = pos + strlen("ByteAddressBuffer ");
        size_t name_end = hlsl.find(" : ", name_start);
        if (name_end == std::string::npos) break;
        size_t line_end = hlsl.find('\n', name_end);
        if (line_end == std::string::npos) line_end = hlsl.size();
        if (hlsl.substr(pos, line_end - pos).find(reg_token) != std::string::npos)
            return hlsl.substr(name_start, name_end - name_start);
        pos = line_end;
    }
    return {};
}

static void ReplaceBufferDecl(std::string& hlsl, const std::string& name,
                               int reg, const char* type_str) {
    const std::string reg_token = "register(t" + std::to_string(reg) + ")";
    size_t pos = 0;
    while ((pos = hlsl.find("ByteAddressBuffer ", pos)) != std::string::npos) {
        size_t line_end = hlsl.find('\n', pos);
        if (line_end == std::string::npos) line_end = hlsl.size();
        std::string line = hlsl.substr(pos, line_end - pos);
        if (line.find(reg_token) != std::string::npos && line.find(name) != std::string::npos) {
            std::string repl = std::string(type_str) + " " + name + " : " + reg_token + ";";
            hlsl.replace(pos, line_end - pos, repl);
            return;
        }
        pos = line_end;
    }
}

static std::string ExtractBalancedExpr(const std::string& s, size_t start, size_t& close) {
    int depth = 1;
    close = start;
    for (; close < s.size() && depth > 0; ++close) {
        if (s[close] == '(') ++depth;
        else if (s[close] == ')') --depth;
    }
    --close;
    return s.substr(start, close - start);
}

static bool ParseByteOffset(const std::string& expr, int stride,
                              std::string& base, int& offset) {
    const std::string tok = " * " + std::to_string(stride) + " + ";
    size_t pos = expr.rfind(tok);
    if (pos != std::string::npos) {
        base = expr.substr(0, pos);
        const char* p = expr.c_str() + pos + tok.size();
        while (*p == ' ') ++p;
        char* end = nullptr;
        offset = static_cast<int>(strtol(p, &end, 10));
        return end && end != p;
    }
    const std::string tok2 = " * " + std::to_string(stride);
    if (expr.size() >= tok2.size() && expr.compare(expr.size() - tok2.size(), tok2.size(), tok2) == 0) {
        base = expr.substr(0, expr.size() - tok2.size());
        offset = 0;
        return true;
    }
    return false;
}

static void ReplaceLoad4Calls(std::string& hlsl, const std::string& buf,
                                int stride, const char* member) {
    const std::string needle = buf + ".Load4(";
    size_t pos = 0;
    while ((pos = hlsl.find(needle, pos)) != std::string::npos) {
        size_t close;
        std::string byte_expr = ExtractBalancedExpr(hlsl, pos + needle.size(), close);
        std::string base; int off;
        if (ParseByteOffset(byte_expr, stride, base, off) && off % 16 == 0 && off < stride) {
            std::string repl = "asuint(" + buf + "[" + base + "]" + member + "[" + std::to_string(off / 16) + "])";
            hlsl.replace(pos, close + 1 - pos, repl);
            pos += repl.size();
        } else {
            pos = close + 1;
        }
    }
}

static void ReplaceInstScalarLoad(std::string& hlsl, const std::string& buf) {
    const std::string needle = buf + ".Load(";
    size_t pos = 0;
    while ((pos = hlsl.find(needle, pos)) != std::string::npos) {
        size_t close;
        std::string byte_expr = ExtractBalancedExpr(hlsl, pos + needle.size(), close);
        std::string base; int off;
        if (ParseByteOffset(byte_expr, 80, base, off) && off == 64) {
            std::string repl = "uint(" + buf + "[" + base + "].bone_offset)";
            hlsl.replace(pos, close + 1 - pos, repl);
            pos += repl.size();
        } else {
            pos = close + 1;
        }
    }
}

static void CollapseMatrixReads(std::string& hlsl, const std::string& buf, const char* member) {
    const std::string head = "asfloat(uint4x4(asuint(" + buf + "[";
    size_t pos = 0;
    while ((pos = hlsl.find(head, pos)) != std::string::npos) {
        size_t expr_start = pos + head.size();
        int depth = 1;
        size_t i = expr_start;
        for (; i < hlsl.size() && depth > 0; ++i) {
            if (hlsl[i] == '[') ++depth;
            else if (hlsl[i] == ']') --depth;
        }
        std::string expr = hlsl.substr(expr_start, i - 1 - expr_start);
        std::string m(member);
        std::string expected = "asfloat(uint4x4("
            "asuint(" + buf + "[" + expr + "]" + m + "[0]), "
            "asuint(" + buf + "[" + expr + "]" + m + "[1]), "
            "asuint(" + buf + "[" + expr + "]" + m + "[2]), "
            "asuint(" + buf + "[" + expr + "]" + m + "[3])))";
        if (pos + expected.size() <= hlsl.size() &&
            hlsl.compare(pos, expected.size(), expected) == 0) {
            std::string repl = buf + "[" + expr + "]" + m;
            hlsl.replace(pos, expected.size(), repl);
            pos += repl.size();
        } else {
            pos += head.size();
        }
    }
}

static void ApplyInstancedBufferBaseIndex(std::string& hlsl, const std::string& buf) {
    const std::string needle = buf + "[gl_InstanceIndex]";
    const std::string repl = buf + "[(pc_u_bone_offset + gl_InstanceIndex)]";
    size_t pos = 0;
    while ((pos = hlsl.find(needle, pos)) != std::string::npos) {
        hlsl.replace(pos, needle.size(), repl);
        pos += repl.size();
    }
}

static void ReplaceAll(std::string& text, const std::string& needle, const std::string& repl) {
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        text.replace(pos, needle.size(), repl);
        pos += repl.size();
    }
}

static void InjectPreskinnedInstanceModelInput(std::string& hlsl, const std::string& buf) {
    if (hlsl.find("DSEGetInstanceModel") != std::string::npos)
        return;

    ReplaceAll(hlsl, buf + "[(pc_u_bone_offset + gl_InstanceIndex)].model", "DSEGetInstanceModel()");

    const std::string global_anchor = "static int gl_InstanceIndex;\n";
    size_t global_pos = hlsl.find(global_anchor);
    if (global_pos != std::string::npos) {
        global_pos += global_anchor.size();
        hlsl.insert(global_pos,
            "static float4 aInstModel0;\n"
            "static float4 aInstModel1;\n"
            "static float4 aInstModel2;\n"
            "static float4 aInstModel3;\n"
            "\n"
            "float4x4 DSEGetInstanceModel()\n"
            "{\n"
            "    if (pc_u_skinned == 3)\n"
            "    {\n"
            "        return float4x4(aInstModel0, aInstModel1, aInstModel2, aInstModel3);\n"
            "    }\n"
            "    return " + buf + "[(pc_u_bone_offset + gl_InstanceIndex)].model;\n"
            "}\n"
            "\n");
    }

    const std::string input_anchor = "    uint gl_InstanceIndex : SV_InstanceID;\n";
    size_t input_pos = hlsl.find(input_anchor);
    if (input_pos != std::string::npos) {
        input_pos += input_anchor.size();
        hlsl.insert(input_pos,
            "    float4 aInstModel0 : TEXCOORD8;\n"
            "    float4 aInstModel1 : TEXCOORD9;\n"
            "    float4 aInstModel2 : TEXCOORD10;\n"
            "    float4 aInstModel3 : TEXCOORD11;\n");
    }

    const std::string assign_anchor = "    gl_InstanceIndex = int(stage_input.gl_InstanceIndex);\n";
    size_t assign_pos = hlsl.find(assign_anchor);
    if (assign_pos != std::string::npos) {
        assign_pos += assign_anchor.size();
        hlsl.insert(assign_pos,
            "    aInstModel0 = stage_input.aInstModel0;\n"
            "    aInstModel1 = stage_input.aInstModel1;\n"
            "    aInstModel2 = stage_input.aInstModel2;\n"
            "    aInstModel3 = stage_input.aInstModel3;\n");
    }
}

static std::string OptimizeSkinnedVertexHLSLStorageBuffers(std::string hlsl, EShLanguage stage) {
    if (stage != EShLangVertex || hlsl.find("struct DSESkinnedInst") == std::string::npos)
        return hlsl;
    std::string inst_buf = FindByteAddressBufferName(hlsl, 26);
    std::string bone_buf = FindByteAddressBufferName(hlsl, 24);
    if (inst_buf.empty() && bone_buf.empty()) return hlsl;
    if (!bone_buf.empty())
        ReplaceBufferDecl(hlsl, bone_buf, 24, "struct DSEBoneMatrix { row_major float4x4 value; };\nStructuredBuffer<DSEBoneMatrix>");
    if (!inst_buf.empty())
        ReplaceBufferDecl(hlsl, inst_buf, 26, "StructuredBuffer<DSESkinnedInst>");
    if (!bone_buf.empty())  ReplaceLoad4Calls(hlsl, bone_buf, 64, ".value");
    if (!inst_buf.empty())  ReplaceLoad4Calls(hlsl, inst_buf, 80, ".model");
    if (!inst_buf.empty())  ReplaceInstScalarLoad(hlsl, inst_buf);
    if (!bone_buf.empty())  CollapseMatrixReads(hlsl, bone_buf, ".value");
    if (!inst_buf.empty())  CollapseMatrixReads(hlsl, inst_buf, ".model");
    if (!inst_buf.empty())  ApplyInstancedBufferBaseIndex(hlsl, inst_buf);
    if (!inst_buf.empty())  InjectPreskinnedInstanceModelInput(hlsl, inst_buf);
    return hlsl;
}

// ============================================================================
// spirv-cross: SPIR-V → HLSL SM5.0
// ============================================================================

static std::string CrossCompileToHLSL(const std::vector<uint32_t>& spirv,
                                       EShLanguage stage) {
    try {
        spirv_cross::CompilerHLSL compiler(spirv);
        spirv_cross::CompilerHLSL::Options options;
        options.shader_model = 50;
        options.point_size_compat = false;
        compiler.set_hlsl_options(options);

        // 通用 GLSL 选项
        spirv_cross::CompilerGLSL::Options glsl_options;
        glsl_options.vulkan_semantics = false;
        compiler.set_common_options(glsl_options);

        auto resources = compiler.get_shader_resources();
        auto exec_model = compiler.get_execution_model();

        // Helper: get (set, binding) for sorting
        auto get_set_binding = [&](spirv_cross::ID id) -> std::pair<uint32_t, uint32_t> {
            uint32_t s = compiler.get_decoration(id, spv::DecorationDescriptorSet);
            uint32_t b = compiler.get_decoration(id, spv::DecorationBinding);
            return {s, b};
        };

        // ---- Combined image samplers → sequential t0/s0, t1/s1, ... ----
        // 数组纹理占据连续 register slot（array_count 个），避免重叠
        uint32_t tex_slot = 0;
        {
            std::vector<std::pair<std::pair<uint32_t,uint32_t>, spirv_cross::Resource*>> sorted;
            for (auto& r : resources.sampled_images) sorted.push_back({get_set_binding(r.id), &r});
            for (auto& r : resources.separate_images) sorted.push_back({get_set_binding(r.id), &r});
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b){ return a.first < b.first; });
            for (auto& [sb, res] : sorted) {
                spirv_cross::HLSLResourceBinding hlsl_binding{};
                hlsl_binding.stage = exec_model;
                hlsl_binding.desc_set = sb.first;
                hlsl_binding.binding = sb.second;
                hlsl_binding.srv.register_space = 0;
                hlsl_binding.srv.register_binding = tex_slot;
                hlsl_binding.sampler.register_space = 0;
                hlsl_binding.sampler.register_binding = tex_slot;
                hlsl_binding.cbv = {};
                hlsl_binding.uav = {};
                compiler.add_hlsl_resource_binding(hlsl_binding);
                // 数组占据连续 slot
                auto& type = compiler.get_type(res->type_id);
                uint32_t array_count = type.array.empty() ? 1 : type.array[0];
                tex_slot += array_count;
            }
            // Separate samplers (if any)
            uint32_t samp_slot = 0;
            for (auto& r : resources.separate_samplers) {
                auto [s, b] = get_set_binding(r.id);
                spirv_cross::HLSLResourceBinding hlsl_binding{};
                hlsl_binding.stage = exec_model;
                hlsl_binding.desc_set = s;
                hlsl_binding.binding = b;
                hlsl_binding.sampler.register_space = 0;
                hlsl_binding.sampler.register_binding = samp_slot++;
                hlsl_binding.srv = {};
                hlsl_binding.cbv = {};
                hlsl_binding.uav = {};
                compiler.add_hlsl_resource_binding(hlsl_binding);
            }
        }

        // ---- Uniform buffers → sequential b0, b1, ... ----
        // push_constant occupies b0 if present; UBOs start after
        {
            uint32_t cb_slot = resources.push_constant_buffers.empty() ? 0 : 1;
            std::vector<std::pair<std::pair<uint32_t,uint32_t>, spirv_cross::Resource*>> sorted;
            for (auto& r : resources.uniform_buffers) sorted.push_back({get_set_binding(r.id), &r});
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b){ return a.first < b.first; });
            for (auto& [sb, res] : sorted) {
                spirv_cross::HLSLResourceBinding hlsl_binding{};
                hlsl_binding.stage = exec_model;
                hlsl_binding.desc_set = sb.first;
                hlsl_binding.binding = sb.second;
                hlsl_binding.cbv.register_space = 0;
                hlsl_binding.cbv.register_binding = cb_slot;
                hlsl_binding.srv = {};
                hlsl_binding.uav = {};
                hlsl_binding.sampler = {};
                compiler.add_hlsl_resource_binding(hlsl_binding);
                cb_slot++;
            }
        }

        // ---- SSBO (storage buffers) → SRV t{tex_slot}+ 避免与纹理 t-register 冲突 ----
        {
            uint32_t ssbo_base = (tex_slot < 16) ? 16 : tex_slot; // 最低 t16
            for (auto& ssbo : resources.storage_buffers) {
                auto [s, b] = get_set_binding(ssbo.id);
                spirv_cross::HLSLResourceBinding hlsl_binding{};
                hlsl_binding.stage = exec_model;
                hlsl_binding.desc_set = s;
                hlsl_binding.binding = b;
                hlsl_binding.srv.register_space = 0;
                hlsl_binding.srv.register_binding = ssbo_base + b;
                hlsl_binding.uav.register_space = 0;
                hlsl_binding.uav.register_binding = ssbo_base + b;
                hlsl_binding.cbv = {};
                hlsl_binding.sampler = {};
                compiler.add_hlsl_resource_binding(hlsl_binding);
            }
        }

        std::string hlsl_out = compiler.compile();
        hlsl_out = OptimizeSkinnedVertexHLSLStorageBuffers(std::move(hlsl_out), stage);
        // SM5.0 sampler 限制后处理：合并 s16+ 的 sampler 到低位同类型 sampler
        hlsl_out = ConsolidateHLSLSamplers(hlsl_out);
        return hlsl_out;
    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "[ERROR] spirv-cross HLSL: " << e.what() << "\n";
        return "";
    }
}

// ============================================================================
// D3DCompile: HLSL → DXBC 字节码（仅 Windows）
// ============================================================================

#ifdef DSE_HAS_D3DCOMPILE
static std::vector<uint8_t> CompileHLSLToDXBC(const std::string& hlsl,
                                               const std::string& entry_point,
                                               const std::string& target) {
    ID3DBlob* blob = nullptr;
    ID3DBlob* error_blob = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

    HRESULT hr = D3DCompile(hlsl.c_str(), hlsl.size(), nullptr, nullptr, nullptr,
                             entry_point.c_str(), target.c_str(), flags, 0,
                             &blob, &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            // Reformat error blob to avoid MSBuild regex matching "file(line): error ..."
            // which would cause MSBuild to report a false build failure (MSB8066).
            std::string msg(static_cast<const char*>(error_blob->GetBufferPointer()),
                            error_blob->GetBufferSize());
            // Neutralize MSBuild error patterns: replace ": error " with ": err "
            for (size_t p = 0; (p = msg.find(": error ", p)) != std::string::npos; )
                msg.replace(p, 8, ": err_ ", 7), p += 7;
            std::cerr << "[WARN] D3DCompile (" << target << "): " << msg << "\n";
            error_blob->Release();
        }
        return {};
    }
    if (error_blob) error_blob->Release();

    std::vector<uint8_t> result(static_cast<uint8_t*>(blob->GetBufferPointer()),
                                 static_cast<uint8_t*>(blob->GetBufferPointer()) + blob->GetBufferSize());
    blob->Release();
    return result;
}
#endif

// ============================================================================
// SPIR-V Reflection → *_reflect.gen.h 生成
// ============================================================================

struct ReflectedVertexInput {
    std::string name;
    uint32_t location;
    std::string base_type;  // "BaseType::Vec3" etc.
    uint32_t vec_size;
    uint32_t columns;
    uint32_t byte_size;
};

struct ReflectedResource {
    std::string name;
    std::string resource_type;  // "ResourceType::UniformBuffer" etc.
    uint32_t set;
    uint32_t binding;
    uint32_t size;
    uint32_t array_count;
    std::string image_dim;      // "ImageDimension::Dim2D" etc.
    bool depth_compare;
};

struct ReflectedPushConstant {
    std::string name;
    uint32_t offset;
    uint32_t size;
    std::string base_type;
    uint32_t array_size;
};

struct StageReflectionData {
    std::vector<ReflectedResource> uniform_buffers;
    std::vector<ReflectedResource> storage_buffers;
    std::vector<ReflectedResource> sampled_images;
    std::vector<ReflectedVertexInput> inputs;
    std::vector<ReflectedPushConstant> push_constants;
    uint32_t push_constant_size = 0;
};

static std::string SpirvBaseTypeToEnum(const spirv_cross::SPIRType& type) {
    if (type.basetype == spirv_cross::SPIRType::Float) {
        if (type.columns > 1) {
            if (type.columns == 4) return "BaseType::Mat4";
            if (type.columns == 3) return "BaseType::Mat3";
        }
        switch (type.vecsize) {
            case 1: return "BaseType::Float";
            case 2: return "BaseType::Vec2";
            case 3: return "BaseType::Vec3";
            case 4: return "BaseType::Vec4";
        }
    }
    if (type.basetype == spirv_cross::SPIRType::Int) {
        if (type.vecsize == 4) return "BaseType::IVec4";
        return "BaseType::Int";
    }
    if (type.basetype == spirv_cross::SPIRType::UInt) {
        if (type.vecsize == 4) return "BaseType::UVec4";
        return "BaseType::UInt";
    }
    if (type.basetype == spirv_cross::SPIRType::Boolean)
        return "BaseType::Bool";
    if (type.basetype == spirv_cross::SPIRType::Double)
        return "BaseType::Double";
    if (type.basetype == spirv_cross::SPIRType::SampledImage ||
        type.basetype == spirv_cross::SPIRType::Image) {
        if (type.image.dim == spv::DimCube) return "BaseType::SamplerCube";
        if (type.image.depth) return "BaseType::Sampler2DShadow";
        return "BaseType::Sampler2D";
    }
    return "BaseType::Float";
}

static std::string ImageDimToEnum(const spirv_cross::SPIRType& type) {
    if (type.image.dim == spv::DimCube) return "ImageDimension::DimCube";
    if (type.image.dim == spv::Dim3D) return "ImageDimension::Dim3D";
    if (type.image.dim == spv::Dim1D) return "ImageDimension::Dim1D";
    return "ImageDimension::Dim2D";
}

static StageReflectionData ExtractReflection(const std::vector<uint32_t>& spirv,
                                              EShLanguage stage) {
    StageReflectionData result;
    try {
        spirv_cross::CompilerGLSL compiler(spirv);
        auto resources = compiler.get_shader_resources();

        // --- Vertex inputs (只对 vertex shader) ---
        if (stage == EShLangVertex) {
            for (auto& input : resources.stage_inputs) {
                auto& type = compiler.get_type(input.type_id);
                ReflectedVertexInput vi;
                vi.name = input.name;
                vi.location = compiler.get_decoration(input.id, spv::DecorationLocation);
                vi.base_type = SpirvBaseTypeToEnum(type);
                vi.vec_size = type.vecsize;
                vi.columns = type.columns;
                // byte size: vecsize * columns * base_size
                uint32_t base_size = 4; // float/int
                if (type.basetype == spirv_cross::SPIRType::Double) base_size = 8;
                vi.byte_size = type.vecsize * type.columns * base_size;
                result.inputs.push_back(std::move(vi));
            }
            // 按 location 排序
            std::sort(result.inputs.begin(), result.inputs.end(),
                [](const ReflectedVertexInput& a, const ReflectedVertexInput& b) {
                    return a.location < b.location;
                });
        }

        // --- Uniform buffers ---
        for (auto& ubo : resources.uniform_buffers) {
            auto& type = compiler.get_type(ubo.base_type_id);
            ReflectedResource rb;
            rb.name = ubo.name;
            rb.resource_type = "ResourceType::UniformBuffer";
            rb.set = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
            rb.binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);
            rb.size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
            rb.array_count = 1;
            rb.image_dim = "ImageDimension::Dim2D";
            rb.depth_compare = false;
            result.uniform_buffers.push_back(std::move(rb));
        }

        // --- Storage buffers (SSBO) ---
        for (auto& ssbo : resources.storage_buffers) {
            ReflectedResource rb;
            rb.name = ssbo.name;
            rb.resource_type = "ResourceType::StorageBuffer";
            rb.set = compiler.get_decoration(ssbo.id, spv::DecorationDescriptorSet);
            rb.binding = compiler.get_decoration(ssbo.id, spv::DecorationBinding);
            rb.size = 0; // runtime-sized
            rb.array_count = 1;
            rb.image_dim = "ImageDimension::Dim2D";
            rb.depth_compare = false;
            result.storage_buffers.push_back(std::move(rb));
        }

        // --- Sampled images (combined image samplers) ---
        for (auto& img : resources.sampled_images) {
            auto& type = compiler.get_type(img.type_id);
            ReflectedResource rb;
            rb.name = img.name;
            rb.resource_type = "ResourceType::SampledImage";
            rb.set = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
            rb.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
            rb.size = 0;
            // 检查数组
            rb.array_count = 1;
            if (!type.array.empty()) {
                rb.array_count = type.array[0];
            }
            rb.image_dim = ImageDimToEnum(type);
            rb.depth_compare = type.image.depth;
            result.sampled_images.push_back(std::move(rb));
        }

        // --- Separate images ---
        for (auto& img : resources.separate_images) {
            auto& type = compiler.get_type(img.type_id);
            ReflectedResource rb;
            rb.name = img.name;
            rb.resource_type = "ResourceType::SeparateImage";
            rb.set = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
            rb.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
            rb.size = 0;
            rb.array_count = 1;
            if (!type.array.empty()) rb.array_count = type.array[0];
            rb.image_dim = ImageDimToEnum(type);
            rb.depth_compare = type.image.depth;
            result.sampled_images.push_back(std::move(rb));
        }

        // --- Push constants ---
        for (auto& pc : resources.push_constant_buffers) {
            auto& type = compiler.get_type(pc.base_type_id);
            uint32_t total_size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
            result.push_constant_size = total_size;

            for (uint32_t m = 0; m < type.member_types.size(); ++m) {
                auto& member_type = compiler.get_type(type.member_types[m]);
                ReflectedPushConstant pm;
                pm.name = compiler.get_member_name(pc.base_type_id, m);
                pm.offset = compiler.type_struct_member_offset(type, m);
                pm.size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(type, m));
                pm.base_type = SpirvBaseTypeToEnum(member_type);
                pm.array_size = 0;
                if (!member_type.array.empty()) pm.array_size = member_type.array[0];
                result.push_constants.push_back(std::move(pm));
            }
        }

    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "[WARN] Reflection extraction failed: " << e.what() << "\n";
    }
    return result;
}

static std::string GenerateReflectHeader(const std::string& shader_name,
                                          const std::string& stage_suffix,
                                          const StageReflectionData& data) {
    std::ostringstream ss;
    std::string id = SanitizeIdentifier(shader_name);

    ss << "// Auto-generated by dse_shader_compiler. DO NOT EDIT.\n";
    ss << "#pragma once\n\n";
    ss << "#include \"engine/render/shader_reflection_types.h\"\n\n";
    ss << "namespace dse {\n";
    ss << "namespace render {\n";
    ss << "namespace generated_shaders {\n";
    ss << "namespace reflect {\n\n";

    // --- Vertex inputs ---
    if (!data.inputs.empty()) {
        ss << "static constexpr shader_reflect::VertexInput k" << id << "_" << stage_suffix << "_inputs[] = {\n";
        for (auto& vi : data.inputs) {
            ss << "    {\"" << vi.name << "\", " << vi.location << ", "
               << "shader_reflect::" << vi.base_type << ", "
               << vi.vec_size << ", " << vi.columns << ", " << vi.byte_size << "},\n";
        }
        ss << "};\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_input_count = "
           << data.inputs.size() << ";\n\n";
    } else {
        ss << "static constexpr shader_reflect::VertexInput* k" << id << "_" << stage_suffix << "_inputs = nullptr;\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_input_count = 0;\n\n";
    }

    // --- Uniform buffers ---
    if (!data.uniform_buffers.empty()) {
        ss << "static constexpr shader_reflect::ResourceBinding k" << id << "_" << stage_suffix << "_ubos[] = {\n";
        for (auto& rb : data.uniform_buffers) {
            ss << "    {\"" << rb.name << "\", shader_reflect::" << rb.resource_type << ", "
               << rb.set << ", " << rb.binding << ", " << rb.size << ", " << rb.array_count << ", "
               << "shader_reflect::" << rb.image_dim << ", " << (rb.depth_compare ? "true" : "false") << "},\n";
        }
        ss << "};\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_ubo_count = "
           << data.uniform_buffers.size() << ";\n\n";
    } else {
        ss << "static constexpr shader_reflect::ResourceBinding* k" << id << "_" << stage_suffix << "_ubos = nullptr;\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_ubo_count = 0;\n\n";
    }

    // --- Storage buffers ---
    if (!data.storage_buffers.empty()) {
        ss << "static constexpr shader_reflect::ResourceBinding k" << id << "_" << stage_suffix << "_ssbos[] = {\n";
        for (auto& rb : data.storage_buffers) {
            ss << "    {\"" << rb.name << "\", shader_reflect::" << rb.resource_type << ", "
               << rb.set << ", " << rb.binding << ", " << rb.size << ", " << rb.array_count << ", "
               << "shader_reflect::" << rb.image_dim << ", " << (rb.depth_compare ? "true" : "false") << "},\n";
        }
        ss << "};\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_ssbo_count = "
           << data.storage_buffers.size() << ";\n\n";
    } else {
        ss << "static constexpr shader_reflect::ResourceBinding* k" << id << "_" << stage_suffix << "_ssbos = nullptr;\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_ssbo_count = 0;\n\n";
    }

    // --- Sampled images ---
    if (!data.sampled_images.empty()) {
        ss << "static constexpr shader_reflect::ResourceBinding k" << id << "_" << stage_suffix << "_textures[] = {\n";
        for (auto& rb : data.sampled_images) {
            ss << "    {\"" << rb.name << "\", shader_reflect::" << rb.resource_type << ", "
               << rb.set << ", " << rb.binding << ", " << rb.size << ", " << rb.array_count << ", "
               << "shader_reflect::" << rb.image_dim << ", " << (rb.depth_compare ? "true" : "false") << "},\n";
        }
        ss << "};\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_texture_count = "
           << data.sampled_images.size() << ";\n\n";
    } else {
        ss << "static constexpr shader_reflect::ResourceBinding* k" << id << "_" << stage_suffix << "_textures = nullptr;\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_texture_count = 0;\n\n";
    }

    // --- Push constants ---
    if (!data.push_constants.empty()) {
        ss << "static constexpr shader_reflect::PushConstantMember k" << id << "_" << stage_suffix << "_push_constants[] = {\n";
        for (auto& pm : data.push_constants) {
            ss << "    {\"" << pm.name << "\", " << pm.offset << ", " << pm.size << ", "
               << "shader_reflect::" << pm.base_type << ", " << pm.array_size << "},\n";
        }
        ss << "};\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_push_constant_count = "
           << data.push_constants.size() << ";\n";
    } else {
        ss << "static constexpr shader_reflect::PushConstantMember* k" << id << "_" << stage_suffix << "_push_constants = nullptr;\n";
        ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_push_constant_count = 0;\n";
    }
    ss << "static constexpr uint32_t k" << id << "_" << stage_suffix << "_push_constant_size = "
       << data.push_constant_size << ";\n\n";

    // --- Aggregate StageReflection struct ---
    ss << "static constexpr shader_reflect::StageReflection k" << id << "_" << stage_suffix << "_reflection = {\n";
    ss << "    k" << id << "_" << stage_suffix << "_ubos, k" << id << "_" << stage_suffix << "_ubo_count,\n";
    ss << "    k" << id << "_" << stage_suffix << "_ssbos, k" << id << "_" << stage_suffix << "_ssbo_count,\n";
    ss << "    k" << id << "_" << stage_suffix << "_textures, k" << id << "_" << stage_suffix << "_texture_count,\n";
    ss << "    k" << id << "_" << stage_suffix << "_inputs, k" << id << "_" << stage_suffix << "_input_count,\n";
    ss << "    k" << id << "_" << stage_suffix << "_push_constants, k" << id << "_" << stage_suffix << "_push_constant_count,\n";
    ss << "    k" << id << "_" << stage_suffix << "_push_constant_size,\n";
    ss << "};\n\n";

    ss << "} // namespace reflect\n";
    ss << "} // namespace generated_shaders\n";
    ss << "} // namespace render\n";
    ss << "} // namespace dse\n";

    return ss.str();
}

// ============================================================================
// 内嵌头文件生成
// ============================================================================

// MSVC limits string literals to 16380 bytes. For long shaders we split
// into multiple concatenated raw-string chunks.
static void EmitStringConstant(std::ostringstream& ss,
                                const std::string& var_name,
                                const std::string& content) {
    const size_t kMaxChunk = 15000; // well under MSVC limit
    if (content.size() <= kMaxChunk) {
        // Short enough for a single raw string literal
        ss << "static const char* " << var_name << " = R\"(" << content << ")\";\n\n";
    } else {
        // Split into concatenated string literals
        ss << "static const char " << var_name << "[] =\n";
        size_t pos = 0;
        while (pos < content.size()) {
            size_t end = std::min(pos + kMaxChunk, content.size());
            // Try to break at a newline to keep readability
            if (end < content.size()) {
                size_t nl = content.rfind('\n', end);
                if (nl != std::string::npos && nl > pos) {
                    end = nl + 1;
                }
            }
            std::string chunk = content.substr(pos, end - pos);
            ss << "R\"(" << chunk << ")\"\n";
            pos = end;
        }
        ss << ";\n\n";
    }
}

static std::string GenerateEmbedHeader(const std::string& shader_name,
                                        const std::string& stage_suffix,
                                        const std::vector<uint32_t>& spirv,
                                        const std::string& glsl430,
                                        const std::string& essl310,
                                        const std::string& hlsl,
                                        const std::vector<uint8_t>& dxbc = {}) {
    std::ostringstream ss;
    std::string id = SanitizeIdentifier(shader_name);
    std::string upper_id = id;
    std::transform(upper_id.begin(), upper_id.end(), upper_id.begin(), ::toupper);

    ss << "// Auto-generated by dse_shader_compiler. DO NOT EDIT.\n";
    ss << "#pragma once\n\n";
    ss << "#include <cstdint>\n";
    ss << "#include <cstddef>\n\n";
    ss << "namespace dse {\n";
    ss << "namespace render {\n";
    ss << "namespace generated_shaders {\n\n";

    // SPIR-V binary
    ss << "// SPIR-V binary (" << spirv.size() << " words, " << spirv.size() * 4 << " bytes)\n";
    ss << "static const uint32_t k" << id << "_" << stage_suffix << "_spv[] = {\n    ";
    for (size_t i = 0; i < spirv.size(); ++i) {
        ss << "0x" << std::hex << spirv[i] << std::dec;
        if (i + 1 < spirv.size()) ss << ",";
        if ((i + 1) % 8 == 0) ss << "\n    ";
    }
    ss << "\n};\n";
    ss << "static const size_t k" << id << "_" << stage_suffix << "_spv_size = "
       << spirv.size() << ";\n\n";

    // GLSL 430 (OpenGL desktop) — split into chunks to avoid MSVC C2026
    ss << "// OpenGL GLSL 430\n";
    EmitStringConstant(ss, "k" + id + "_" + stage_suffix + "_glsl430", glsl430);

    // ESSL 310 (OpenGL ES 3.1 / Android)
    ss << "// OpenGL ES ESSL 310\n";
    EmitStringConstant(ss, "k" + id + "_" + stage_suffix + "_essl310", essl310);

    // HLSL SM5 — split into chunks to avoid MSVC C2026
    ss << "// DX11 HLSL SM5.0\n";
    EmitStringConstant(ss, "k" + id + "_" + stage_suffix + "_hlsl", hlsl);

    // DXBC pre-compiled bytecode (if available)
    if (!dxbc.empty()) {
        ss << "// DX11 DXBC pre-compiled bytecode (" << dxbc.size() << " bytes)\n";
        ss << "static const uint8_t k" << id << "_" << stage_suffix << "_dxbc[] = {\n    ";
        for (size_t i = 0; i < dxbc.size(); ++i) {
            ss << "0x" << std::hex << static_cast<int>(dxbc[i]) << std::dec;
            if (i + 1 < dxbc.size()) ss << ",";
            if ((i + 1) % 16 == 0) ss << "\n    ";
        }
        ss << "\n};\n";
        ss << "static const size_t k" << id << "_" << stage_suffix << "_dxbc_size = "
           << dxbc.size() << ";\n\n";
    }

    ss << "} // namespace generated_shaders\n";
    ss << "} // namespace render\n";
    ss << "} // namespace dse\n";

    return ss.str();
}

// ============================================================================
// Main
// ============================================================================

struct Options {
    fs::path input_dir;
    fs::path output_dir;
    std::string target = "all";  // all, spv, glsl430, hlsl
    bool embed = false;
};

static Options ParseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input-dir" && i + 1 < argc) opts.input_dir = argv[++i];
        else if (arg == "--output-dir" && i + 1 < argc) opts.output_dir = argv[++i];
        else if (arg == "--target" && i + 1 < argc) opts.target = argv[++i];
        else if (arg == "--embed") opts.embed = true;
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: dse_shader_compiler [options]\n"
                      << "  --input-dir <path>   GLSL 450 source directory\n"
                      << "  --output-dir <path>  Output directory\n"
                      << "  --target <all|spv|glsl430|hlsl>\n"
                      << "  --embed              Generate C++ embed headers\n";
            exit(0);
        }
    }
    return opts;
}

int main(int argc, char* argv[]) {
    Options opts = ParseArgs(argc, argv);

    if (opts.input_dir.empty() || opts.output_dir.empty()) {
        std::cerr << "[ERROR] --input-dir and --output-dir are required.\n";
        return 1;
    }

    if (!fs::exists(opts.input_dir)) {
        std::cerr << "[ERROR] Input directory does not exist: " << opts.input_dir << "\n";
        return 1;
    }

    // Initialize glslang
    glslang::InitializeProcess();

    int success_count = 0;
    int error_count = 0;

    // Process all shader files
    for (const auto& entry : fs::directory_iterator(opts.input_dir)) {
        if (!entry.is_regular_file()) continue;

        fs::path filepath = entry.path();
        std::string ext = filepath.extension().string();

        // Only process known shader extensions
        if (ext != ".vert" && ext != ".frag" && ext != ".comp" &&
            ext != ".geom" && ext != ".tesc" && ext != ".tese") {
            continue;
        }

        std::string shader_name = filepath.stem().string();
        std::string stage_suffix = ext.substr(1); // remove leading dot
        EShLanguage stage = GetShaderStage(ext);

        std::cout << "[COMPILE] " << filepath.filename() << " ... ";

        // Read source
        std::string source = ReadFile(filepath);
        if (source.empty()) {
            std::cerr << "FAILED (read)\n";
            error_count++;
            continue;
        }

        // Compile to SPIR-V
        std::vector<uint32_t> spirv;
        if (!CompileToSpirv(source, stage, filepath.filename().string(), spirv)) {
            std::cerr << "FAILED (glslang)\n";
            error_count++;
            continue;
        }

        // Write SPIR-V
        bool do_spv = (opts.target == "all" || opts.target == "spv");
        bool do_glsl = (opts.target == "all" || opts.target == "glsl430");
        bool do_hlsl = (opts.target == "all" || opts.target == "hlsl");

        if (do_spv) {
            fs::path spv_path = opts.output_dir / "spv" / (shader_name + "." + stage_suffix + ".spv");
            WriteBinary(spv_path, spirv);
        }

        // Cross-compile to GLSL 430
        std::string glsl430;
        if (do_glsl) {
            glsl430 = CrossCompileToGLSL430(spirv, stage);
            if (glsl430.empty()) {
                std::cerr << "FAILED (glsl430)\n";
                error_count++;
                continue;
            }
            if (!opts.embed) {
                fs::path glsl_path = opts.output_dir / "glsl430" / (shader_name + "." + stage_suffix + ".glsl");
                WriteFile(glsl_path, glsl430);
            }
        }

        // Cross-compile to HLSL
        std::string hlsl;
        if (do_hlsl) {
            hlsl = CrossCompileToHLSL(spirv, stage);
            if (hlsl.empty()) {
                std::cerr << "FAILED (hlsl)\n";
                error_count++;
                continue;
            }
            if (!opts.embed) {
                fs::path hlsl_path = opts.output_dir / "hlsl" / (shader_name + "." + stage_suffix + ".hlsl");
                WriteFile(hlsl_path, hlsl);
            }
        }

        // Generate embed header
        std::string essl310;
        if (opts.embed) {
            if (glsl430.empty()) glsl430 = CrossCompileToGLSL430(spirv, stage);
            essl310 = CrossCompileToESSL310(spirv, stage);
            if (hlsl.empty()) hlsl = CrossCompileToHLSL(spirv, stage);

            // Compile HLSL to DXBC bytecode (Windows only)
            std::vector<uint8_t> dxbc;
#ifdef DSE_HAS_D3DCOMPILE
            if (!hlsl.empty()) {
                std::string dxbc_target = (stage == EShLangVertex) ? "vs_5_0" :
                                          (stage == EShLangFragment) ? "ps_5_0" :
                                          (stage == EShLangCompute) ? "cs_5_0" : "";
                if (!dxbc_target.empty()) {
                    dxbc = CompileHLSLToDXBC(hlsl, "main", dxbc_target);
                    if (dxbc.empty()) {
                        std::cerr << "[WARN] DXBC compilation failed for " << shader_name << "." << stage_suffix << "\n";
                    }
                }
            }
#endif

            std::string header = GenerateEmbedHeader(shader_name, stage_suffix, spirv, glsl430, essl310, hlsl, dxbc);
            fs::path header_path = opts.output_dir / "embed" / (shader_name + "_" + stage_suffix + ".gen.h");
            WriteFile(header_path, header);

            // Generate reflection metadata header
            StageReflectionData reflect_data = ExtractReflection(spirv, stage);
            std::string reflect_header = GenerateReflectHeader(shader_name, stage_suffix, reflect_data);
            fs::path reflect_path = opts.output_dir / "embed" / (shader_name + "_" + stage_suffix + "_reflect.gen.h");
            WriteFile(reflect_path, reflect_header);
        }

        std::cout << "OK (spv:" << spirv.size() * 4 << "B";
        if (!glsl430.empty()) std::cout << " glsl:" << glsl430.size() << "B";
        if (!essl310.empty() && opts.embed) std::cout << " essl310:" << essl310.size() << "B";
        if (!hlsl.empty()) std::cout << " hlsl:" << hlsl.size() << "B";
        std::cout << ")\n";
        success_count++;

        // ---- 处理 @VARIANTS 多变体编译 ----
        {
            const std::string sentinel = "// @VARIANTS:";
            auto vpos = source.find(sentinel);
            if (vpos != std::string::npos) {
                auto line_end = source.find('\n', vpos);
                std::string variant_line = source.substr(
                    vpos + sentinel.size(),
                    line_end == std::string::npos
                        ? source.size() - vpos - sentinel.size()
                        : line_end - vpos - sentinel.size());

                std::istringstream vss(variant_line);
                std::string vtok;
                while (std::getline(vss, vtok, ',')) {
                    size_t vstart = vtok.find_first_not_of(" \t");
                    size_t vend   = vtok.find_last_not_of(" \t\r\n");
                    if (vstart == std::string::npos) continue;
                    std::string variant_define = vtok.substr(vstart, vend - vstart + 1);

                    std::string variant_lower = variant_define;
                    std::transform(variant_lower.begin(), variant_lower.end(),
                                   variant_lower.begin(), ::tolower);
                    std::string variant_name    = shader_name + "_" + variant_lower;
                    std::string variant_preamble = "#define " + variant_define + " 1\n";

                    std::cout << "[VARIANT] " << variant_name << "." << stage_suffix << " ... ";

                    std::vector<uint32_t> v_spirv;
                    if (!CompileToSpirv(source, stage, filepath.filename().string(),
                                        v_spirv, variant_preamble)) {
                        std::cerr << "FAILED (glslang)\n";
                        error_count++;
                        continue;
                    }

                    bool v_do_spv  = (opts.target == "all" || opts.target == "spv");
                    bool v_do_glsl = (opts.target == "all" || opts.target == "glsl430");
                    bool v_do_hlsl = (opts.target == "all" || opts.target == "hlsl");

                    if (v_do_spv) {
                        fs::path spv_path = opts.output_dir / "spv" /
                            (variant_name + "." + stage_suffix + ".spv");
                        WriteBinary(spv_path, v_spirv);
                    }

                    std::string v_glsl430;
                    if (v_do_glsl) {
                        v_glsl430 = CrossCompileToGLSL430(v_spirv, stage);
                        if (v_glsl430.empty()) {
                            std::cerr << "FAILED (glsl430)\n";
                            error_count++;
                            continue;
                        }
                        if (!opts.embed) {
                            fs::path glsl_path = opts.output_dir / "glsl430" /
                                (variant_name + "." + stage_suffix + ".glsl");
                            WriteFile(glsl_path, v_glsl430);
                        }
                    }

                    std::string v_hlsl;
                    if (v_do_hlsl) {
                        v_hlsl = CrossCompileToHLSL(v_spirv, stage);
                        if (v_hlsl.empty()) {
                            std::cerr << "FAILED (hlsl)\n";
                            error_count++;
                            continue;
                        }
                        if (!opts.embed) {
                            fs::path hlsl_path = opts.output_dir / "hlsl" /
                                (variant_name + "." + stage_suffix + ".hlsl");
                            WriteFile(hlsl_path, v_hlsl);
                        }
                    }

                    if (opts.embed) {
                        if (v_glsl430.empty()) v_glsl430 = CrossCompileToGLSL430(v_spirv, stage);
                        std::string v_essl310 = CrossCompileToESSL310(v_spirv, stage);
                        if (v_hlsl.empty()) v_hlsl = CrossCompileToHLSL(v_spirv, stage);

                        std::vector<uint8_t> v_dxbc;
#ifdef DSE_HAS_D3DCOMPILE
                        if (!v_hlsl.empty()) {
                            std::string dxbc_target =
                                (stage == EShLangVertex)   ? "vs_5_0" :
                                (stage == EShLangFragment) ? "ps_5_0" :
                                (stage == EShLangCompute)  ? "cs_5_0" : "";
                            if (!dxbc_target.empty()) {
                                v_dxbc = CompileHLSLToDXBC(v_hlsl, "main", dxbc_target);
                            }
                        }
#endif
                        std::string v_header = GenerateEmbedHeader(
                            variant_name, stage_suffix, v_spirv,
                            v_glsl430, v_essl310, v_hlsl, v_dxbc);
                        fs::path v_hdr_path = opts.output_dir / "embed" /
                            (variant_name + "_" + stage_suffix + ".gen.h");
                        WriteFile(v_hdr_path, v_header);

                        StageReflectionData v_reflect = ExtractReflection(v_spirv, stage);
                        std::string v_reflect_hdr = GenerateReflectHeader(
                            variant_name, stage_suffix, v_reflect);
                        fs::path v_ref_path = opts.output_dir / "embed" /
                            (variant_name + "_" + stage_suffix + "_reflect.gen.h");
                        WriteFile(v_ref_path, v_reflect_hdr);
                    }

                    std::cout << "OK (spv:" << v_spirv.size() * 4 << "B";
                    if (!v_glsl430.empty()) std::cout << " glsl:" << v_glsl430.size() << "B";
                    std::cout << ")\n";
                    success_count++;
                }
            }
        }
    }

    glslang::FinalizeProcess();

    std::cout << "\n[DONE] " << success_count << " shaders compiled, "
              << error_count << " errors.\n";

    return error_count > 0 ? 1 : 0;
}
