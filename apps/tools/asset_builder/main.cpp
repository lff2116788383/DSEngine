#include "engine/assets/compiler/importer.h"
#include "engine/assets/texture_compressor.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// importer.cpp 用 TINYGLTF_NO_STB_IMAGE，未编译 stb_image 实现；此处提供之。
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

using namespace dse::asset::compiler;

namespace {

void PrintUsage() {
    std::cout
        << "Usage:\n"
        << "  AssetBuilder <input.gltf/glb/fbx> <output.dmesh>\n"
        << "  AssetBuilder <input.gltf/glb/fbx> --out-dir <output_dir>\n"
        << "  AssetBuilder --texture <input.png/jpg/...> <output.dtex> [options]\n\n"
        << "Mesh/animation options:\n"
        << "  --no-anim-compress   write raw v2 .danim (default: v3 quantized)\n"
        << "  --no-anim-reduce     keep all keyframes (default: error-threshold decimation)\n\n"
        << "Texture options:\n"
        << "  --format <bc1|bc1srgb|bc3|bc3srgb|bc4|bc5>   target BCn format (default bc3)\n"
        << "  --no-mips                                    do not generate a mip chain\n"
        << "  --hq                                         high-quality encode (slower)\n\n"
        << "Examples:\n"
        << "  AssetBuilder assets/model.glb cooked/model.dmesh\n"
        << "  AssetBuilder assets/model.fbx --out-dir cooked\n"
        << "  AssetBuilder --texture assets/albedo.png cooked/albedo.dtex --format bc1srgb\n"
        << "  AssetBuilder --texture assets/normal.png cooked/normal.dtex --format bc5\n\n"
        << "Notes:\n"
        << "  - The tool imports glTF/GLB/FBX and cooks .dmesh/.dmat/.danim/.dskel in one pass.\n"
        << "  - FBX is imported offline through the asset compiler path; runtime does not read FBX directly.\n"
        << "  - When --out-dir is used, the base file name is derived from the input file stem.\n"
        << "  - --texture encodes source images to BCn .dtex (BC7/ASTC not yet supported).\n";
}

bool ParseTextureFormat(const std::string& s, CompressedTextureFormat& out) {
    using F = CompressedTextureFormat;
    if (s == "bc1")      { out = F::BC1_UNORM; return true; }
    if (s == "bc1srgb")  { out = F::BC1_SRGB;  return true; }
    if (s == "bc3")      { out = F::BC3_UNORM; return true; }
    if (s == "bc3srgb")  { out = F::BC3_SRGB;  return true; }
    if (s == "bc4")      { out = F::BC4_UNORM; return true; }
    if (s == "bc5")      { out = F::BC5_UNORM; return true; }
    return false;
}

int RunTextureCook(int argc, char** argv) {
    // argv[1] == "--texture"; expect input, output, then options.
    if (argc < 4) {
        std::cerr << "[AssetBuilder] --texture requires <input> <output.dtex>. Use --help." << std::endl;
        return 1;
    }
    const std::filesystem::path input_path = argv[2];
    const std::filesystem::path output_path = argv[3];

    CompressedTextureFormat format = CompressedTextureFormat::BC3_UNORM;
    bool generate_mips = true;
    bool high_quality = false;
    for (int i = 4; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt == "--no-mips") {
            generate_mips = false;
        } else if (opt == "--hq") {
            high_quality = true;
        } else if (opt == "--format") {
            if (i + 1 >= argc || !ParseTextureFormat(argv[i + 1], format)) {
                std::cerr << "[AssetBuilder] Invalid/missing --format value." << std::endl;
                return 1;
            }
            ++i;
        } else {
            std::cerr << "[AssetBuilder] Unknown texture option: " << opt << std::endl;
            return 1;
        }
    }

    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* pixels = stbi_load(input_path.string().c_str(), &w, &h, &channels, 4);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        std::cerr << "[AssetBuilder] Failed to load image: " << input_path.string() << std::endl;
        return 1;
    }

    std::vector<uint8_t> dtex;
    const bool ok = dse::assets::EncodeTextureToDtex(pixels, w, h, format, generate_mips,
                                                     high_quality, dtex);
    stbi_image_free(pixels);
    if (!ok) {
        std::cerr << "[AssetBuilder] Texture encode failed (unsupported format?)." << std::endl;
        return 1;
    }

    std::error_code ec;
    const std::filesystem::path out_dir = output_path.parent_path();
    if (!out_dir.empty()) std::filesystem::create_directories(out_dir, ec);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "[AssetBuilder] Failed to open output: " << output_path.string() << std::endl;
        return 1;
    }
    out.write(reinterpret_cast<const char*>(dtex.data()), static_cast<std::streamsize>(dtex.size()));
    std::cout << "[AssetBuilder] Cooked dtex: " << output_path.string()
              << " (" << w << "x" << h << ", " << dtex.size() << " bytes)" << std::endl;
    return 0;
}

bool HasSupportedInputExtension(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    return ext == ".gltf" || ext == ".glb" || ext == ".fbx"
        || ext == ".GLTF" || ext == ".GLB" || ext == ".FBX";
}


bool IsLikelyFbxInput(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();
    return ext == ".fbx" || ext == ".FBX";
}


} // namespace

int main(int argc, char** argv) {
    if (argc <= 1) {
        PrintUsage();
        return 1;
    }

    const std::string first_arg = argv[1];
    if (first_arg == "--help" || first_arg == "-h") {
        PrintUsage();
        return 0;
    }

    if (first_arg == "--texture") {
        return RunTextureCook(argc, argv);
    }

    // Strip recognized animation-compression flags so the remaining args are positional.
    AnimCompressOptions anim_opts;  // quantize + reduce on by default
    std::vector<std::string> pos_args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--no-anim-compress") { anim_opts.quantize = false; continue; }
        if (a == "--no-anim-reduce")   { anim_opts.reduce_keyframes = false; continue; }
        pos_args.push_back(a);
    }

    if (pos_args.size() != 2 && pos_args.size() != 3) {
        std::cerr << "[AssetBuilder] Invalid arguments. Use --help for usage." << std::endl;
        return 1;
    }

    const std::filesystem::path input_path = pos_args[0];
    if (!HasSupportedInputExtension(input_path)) {
        std::cerr << "[AssetBuilder] Unsupported input extension: " << input_path.extension().string()
                  << ". Only .gltf/.glb/.fbx are supported." << std::endl;
        return 1;
    }



    std::filesystem::path output_dmesh_path;
    std::filesystem::path output_dir;
    std::string base_name;

    if (pos_args.size() == 2) {
        output_dmesh_path = pos_args[1];
        output_dir = output_dmesh_path.parent_path().empty() ? std::filesystem::path(".") : output_dmesh_path.parent_path();
        base_name = output_dmesh_path.stem().string();
    } else {
        const std::string option = pos_args[1];
        if (option != "--out-dir") {
            std::cerr << "[AssetBuilder] Unknown option: " << option << ". Use --help for usage." << std::endl;
            return 1;
        }
        output_dir = pos_args[2];
        base_name = input_path.stem().string();
        output_dmesh_path = output_dir / (base_name + ".dmesh");
    }

    if (base_name.empty()) {
        std::cerr << "[AssetBuilder] Failed to resolve output base name." << std::endl;
        return 1;
    }

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "[AssetBuilder] Failed to create output directory: " << output_dir.string() << std::endl;
        return 1;
    }

    RawSceneData scene;

    std::cout << "[AssetBuilder] Importing: " << input_path.string() << std::endl;
    bool import_ok = false;
    if (IsLikelyFbxInput(input_path)) {
        FbxImporter importer;
        import_ok = importer.Import(input_path.string(), scene);
    } else {
        GltfImporter importer;
        import_ok = importer.Import(input_path.string(), scene);
    }
    if (!import_ok) {
        std::cerr << "[AssetBuilder] Failed to import file: " << input_path.string() << std::endl;
        return 1;
    }


    std::cout << "[AssetBuilder] Imported successfully. Meshes=" << scene.meshes.size()
              << ", Materials=" << scene.materials.size()
              << ", SkeletonBones=" << scene.skeleton.size()
              << ", Animations=" << scene.animations.size() << std::endl;

    MeshCooker cooker;

    const std::filesystem::path dmat_path = output_dir / (base_name + ".dmat");
    const std::filesystem::path danim_path = output_dir / (base_name + ".danim");
    const std::filesystem::path dskel_path = output_dir / (base_name + ".dskel");

    if (scene.meshes.empty()) {
        std::cout << "[AssetBuilder] Skip dmesh/dmat: no mesh data in source." << std::endl;
    } else {
        std::cout << "[AssetBuilder] Cooking dmesh: " << output_dmesh_path.string() << std::endl;
        if (!cooker.CookToDmesh(scene, output_dmesh_path.string())) {
            std::cerr << "[AssetBuilder] Failed to cook dmesh: " << output_dmesh_path.string() << std::endl;
            return 1;
        }
        std::cout << "[AssetBuilder] Cooking dmat: " << dmat_path.string() << std::endl;
        if (!cooker.CookToDmat(scene, output_dir.string(), base_name)) {
            std::cerr << "[AssetBuilder] Warning: no material data, skipping dmat." << std::endl;
        }
    }

    if (scene.animations.empty()) {
        std::cout << "[AssetBuilder] Skip danim: no animation data in source." << std::endl;
    } else {
        std::cout << "[AssetBuilder] Cooking danim: " << danim_path.string()
                  << (anim_opts.quantize ? " (v3 quantized" : " (v2 raw")
                  << (anim_opts.quantize && anim_opts.reduce_keyframes ? "+reduced)" : ")")
                  << std::endl;
        if (!cooker.CookToDanim(scene, output_dir.string(), base_name, anim_opts)) {
            std::cerr << "[AssetBuilder] Failed to cook danim: " << danim_path.string() << std::endl;
            return 1;
        }
    }

    if (scene.skeleton.empty()) {
        std::cout << "[AssetBuilder] Skip dskel: no skeleton data in source." << std::endl;
    } else {
        std::cout << "[AssetBuilder] Cooking dskel: " << dskel_path.string() << std::endl;
        if (!cooker.CookToDskel(scene, output_dir.string(), base_name)) {
            std::cerr << "[AssetBuilder] Failed to cook dskel: " << dskel_path.string() << std::endl;
            return 1;
        }
    }


    std::cout << "[AssetBuilder] Cooked successfully." << std::endl;
    return 0;
}

