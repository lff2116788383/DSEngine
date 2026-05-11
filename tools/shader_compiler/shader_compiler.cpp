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
                           std::vector<uint32_t>& spirv_out) {
    const char* src_cstr = source.c_str();
    const char* names[] = { filename.c_str() };

    glslang::TShader shader(stage);
    shader.setStringsWithLengthsAndNames(&src_cstr, nullptr, names, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

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
// spirv-cross: SPIR-V → GLSL 330
// ============================================================================

static std::string CrossCompileToGLSL330(const std::vector<uint32_t>& spirv,
                                          EShLanguage stage) {
    try {
        spirv_cross::CompilerGLSL compiler(spirv);
        spirv_cross::CompilerGLSL::Options options;
        options.version = 330;
        options.es = false;
        options.vulkan_semantics = false;
        options.enable_420pack_extension = false;
        compiler.set_common_options(options);

        // 将 push_constant 映射为普通 uniform
        auto resources = compiler.get_shader_resources();
        for (auto& pc : resources.push_constant_buffers) {
            compiler.set_decoration(pc.id, spv::DecorationBinding, 0);
        }

        return compiler.compile();
    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "[ERROR] spirv-cross GLSL: " << e.what() << "\n";
        return "";
    }
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

        return compiler.compile();
    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "[ERROR] spirv-cross HLSL: " << e.what() << "\n";
        return "";
    }
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
                                        const std::string& glsl330,
                                        const std::string& hlsl) {
    std::ostringstream ss;
    std::string id = SanitizeIdentifier(shader_name);
    std::string upper_id = id;
    std::transform(upper_id.begin(), upper_id.end(), upper_id.begin(), ::toupper);

    ss << "// Auto-generated by dse_shader_compiler. DO NOT EDIT.\n";
    ss << "#pragma once\n\n";
    ss << "#include <cstdint>\n\n";
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

    // GLSL 330 — split into chunks to avoid MSVC C2026 (max 16380 bytes per literal)
    ss << "// OpenGL GLSL 330\n";
    EmitStringConstant(ss, "k" + id + "_" + stage_suffix + "_glsl330", glsl330);

    // HLSL SM5 — split into chunks to avoid MSVC C2026
    ss << "// DX11 HLSL SM5.0\n";
    EmitStringConstant(ss, "k" + id + "_" + stage_suffix + "_hlsl", hlsl);

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
    std::string target = "all";  // all, spv, glsl330, hlsl
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
                      << "  --target <all|spv|glsl330|hlsl>\n"
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
        bool do_glsl = (opts.target == "all" || opts.target == "glsl330");
        bool do_hlsl = (opts.target == "all" || opts.target == "hlsl");

        if (do_spv) {
            fs::path spv_path = opts.output_dir / "spv" / (shader_name + "." + stage_suffix + ".spv");
            WriteBinary(spv_path, spirv);
        }

        // Cross-compile to GLSL 330
        std::string glsl330;
        if (do_glsl) {
            glsl330 = CrossCompileToGLSL330(spirv, stage);
            if (glsl330.empty()) {
                std::cerr << "FAILED (glsl330)\n";
                error_count++;
                continue;
            }
            if (!opts.embed) {
                fs::path glsl_path = opts.output_dir / "glsl330" / (shader_name + "." + stage_suffix + ".glsl");
                WriteFile(glsl_path, glsl330);
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
        if (opts.embed) {
            if (glsl330.empty()) glsl330 = CrossCompileToGLSL330(spirv, stage);
            if (hlsl.empty()) hlsl = CrossCompileToHLSL(spirv, stage);

            std::string header = GenerateEmbedHeader(shader_name, stage_suffix, spirv, glsl330, hlsl);
            fs::path header_path = opts.output_dir / "embed" / (shader_name + "_" + stage_suffix + ".gen.h");
            WriteFile(header_path, header);
        }

        std::cout << "OK (spv:" << spirv.size() * 4 << "B";
        if (!glsl330.empty()) std::cout << " glsl:" << glsl330.size() << "B";
        if (!hlsl.empty()) std::cout << " hlsl:" << hlsl.size() << "B";
        std::cout << ")\n";
        success_count++;
    }

    glslang::FinalizeProcess();

    std::cout << "\n[DONE] " << success_count << " shaders compiled, "
              << error_count << " errors.\n";

    return error_count > 0 ? 1 : 0;
}
