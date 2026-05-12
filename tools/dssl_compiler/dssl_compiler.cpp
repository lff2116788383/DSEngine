#include "dssl_parser.h"
#include "dssl_codegen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static void PrintUsage() {
    std::cout << "Usage: dse_dssl_compiler [options]\n"
              << "\n"
              << "Options:\n"
              << "  --input <file.dssl>    Compile a single .dssl file\n"
              << "  --input-dir <path>     Compile all .dssl files in directory\n"
              << "  --output-dir <path>    Output directory for generated GLSL 450\n"
              << "  --meta                 Also output .meta.json files\n"
              << "  --verbose              Show detailed output\n"
              << "  --help                 Show this help\n"
              << "\n"
              << "Output:\n"
              << "  For each <name>.dssl, generates:\n"
              << "    <output-dir>/<name>.vert  (GLSL 450 vertex shader)\n"
              << "    <output-dir>/<name>.frag  (GLSL 450 fragment shader)\n"
              << "    <output-dir>/<name>.meta.json  (uniform metadata, with --meta)\n"
              << "\n"
              << "  The generated GLSL 450 files can then be compiled with\n"
              << "  dse_shader_compiler to produce SPIR-V / GLSL 330 / HLSL.\n";
}

static std::string ReadFile(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool WriteFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

struct CompileResult {
    std::string name;
    bool success;
    std::string error;
    size_t vert_size;
    size_t frag_size;
};

static CompileResult CompileDSSL(const fs::path& input_path, const fs::path& output_dir,
                                  bool emit_meta, bool verbose) {
    CompileResult result;
    result.name = input_path.stem().string();
    result.success = false;
    result.vert_size = 0;
    result.frag_size = 0;

    // 读取源文件
    std::string source = ReadFile(input_path);
    if (source.empty()) {
        result.error = "Cannot read file: " + input_path.string();
        return result;
    }

    // 解析
    auto mod = dssl::Parse(source, input_path.string());
    if (!mod.error.empty()) {
        result.error = "Parse error: " + mod.error;
        return result;
    }

    if (verbose) {
        const char* type_names[] = {"surface", "unlit", "particle", "sky", "postprocess", "canvas"};
        std::cout << "  Parsed: shader_type=" << type_names[(int)mod.shader_type]
                  << ", uniforms=" << mod.uniforms.size()
                  << ", has_vertex=" << (!mod.vertex_body.empty())
                  << ", has_surface=" << (!mod.surface_body.empty())
                  << ", has_light=" << (!mod.light_body.empty()) << "\n";
    }

    // 代码生成
    auto output = dssl::Generate(mod);
    if (!output.error.empty()) {
        result.error = "CodeGen error: " + output.error;
        return result;
    }

    // 写出文件
    if (!output.vert_glsl.empty()) {
        auto vert_path = output_dir / (result.name + ".vert");
        if (!WriteFile(vert_path, output.vert_glsl)) {
            result.error = "Cannot write: " + vert_path.string();
            return result;
        }
        result.vert_size = output.vert_glsl.size();
    }

    if (!output.frag_glsl.empty()) {
        auto frag_path = output_dir / (result.name + ".frag");
        if (!WriteFile(frag_path, output.frag_glsl)) {
            result.error = "Cannot write: " + frag_path.string();
            return result;
        }
        result.frag_size = output.frag_glsl.size();
    }

    if (emit_meta && !output.meta_json.empty()) {
        auto meta_path = output_dir / (result.name + ".meta.json");
        WriteFile(meta_path, output.meta_json);
    }

    result.success = true;
    return result;
}

int main(int argc, char** argv) {
    std::string input_file;
    std::string input_dir;
    std::string output_dir;
    bool emit_meta = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { PrintUsage(); return 0; }
        else if (arg == "--input" && i + 1 < argc) input_file = argv[++i];
        else if (arg == "--input-dir" && i + 1 < argc) input_dir = argv[++i];
        else if (arg == "--output-dir" && i + 1 < argc) output_dir = argv[++i];
        else if (arg == "--meta") emit_meta = true;
        else if (arg == "--verbose") verbose = true;
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage();
            return 1;
        }
    }

    if (input_file.empty() && input_dir.empty()) {
        std::cerr << "Error: specify --input or --input-dir\n";
        PrintUsage();
        return 1;
    }
    if (output_dir.empty()) {
        std::cerr << "Error: specify --output-dir\n";
        return 1;
    }

    // 收集输入文件
    std::vector<fs::path> files;
    if (!input_file.empty()) {
        files.push_back(fs::path(input_file));
    }
    if (!input_dir.empty()) {
        for (auto& entry : fs::directory_iterator(input_dir)) {
            if (entry.path().extension() == ".dssl") {
                files.push_back(entry.path());
            }
        }
    }

    if (files.empty()) {
        std::cerr << "No .dssl files found.\n";
        return 1;
    }

    int success_count = 0;
    int error_count = 0;

    for (auto& file : files) {
        std::cout << "[DSSL] \"" << file.filename().string() << "\" ... ";
        auto result = CompileDSSL(file, fs::path(output_dir), emit_meta, verbose);
        if (result.success) {
            std::cout << "OK (vert:" << result.vert_size << "B frag:" << result.frag_size << "B)\n";
            success_count++;
        } else {
            std::cout << "FAILED\n  " << result.error << "\n";
            error_count++;
        }
    }

    std::cout << "\n[DONE] " << success_count << " shaders compiled, "
              << error_count << " errors.\n";

    return error_count > 0 ? 1 : 0;
}
