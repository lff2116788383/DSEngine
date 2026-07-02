#include "engine/assets/compiler/importer.h"
#include "engine/assets/texture_compressor.h"
#include "engine/mesh/mesh_decimator.h"
#include "engine/render/gi/lightmap_baker.h"
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
        << "  --no-anim-reduce     keep all keyframes (default: error-threshold decimation)\n"
        << "  --decimate <ratio>   decimate mesh to target ratio (e.g. 0.5 = 50%%)\n"
        << "  --lod-levels <n>     auto-generate n LOD levels (e.g. 3 => LOD1=50%%, LOD2=25%%, LOD3=12.5%%)\n\n"
        << "Lightmap baking:\n"
        << "  AssetBuilder --bake-lightmap <scene.gltf/glb> <output.dlightmap> [options]\n"
        << "  --lm-resolution <n>   lightmap resolution (default 512)\n"
        << "  --lm-samples <n>      samples per texel (default 64)\n"
        << "  --lm-bounces <n>      indirect bounces (default 2)\n"
        << "  --lm-no-ao            disable AO baking\n"
        << "  --lm-no-denoise       disable denoising\n\n"
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
        << "  - --texture encodes source images to .dtex: bc1/bc3/bc4/bc5/bc7/astc4x4/astc6x6/astc8x8 (+srgb).\n";
}

bool ParseTextureFormat(const std::string& s, CompressedTextureFormat& out) {
    using F = CompressedTextureFormat;
    if (s == "bc1")         { out = F::BC1_UNORM;       return true; }
    if (s == "bc1srgb")     { out = F::BC1_SRGB;        return true; }
    if (s == "bc3")         { out = F::BC3_UNORM;       return true; }
    if (s == "bc3srgb")     { out = F::BC3_SRGB;        return true; }
    if (s == "bc4")         { out = F::BC4_UNORM;       return true; }
    if (s == "bc5")         { out = F::BC5_UNORM;       return true; }
    if (s == "bc7")         { out = F::BC7_UNORM;       return true; }
    if (s == "bc7srgb")     { out = F::BC7_SRGB;        return true; }
    if (s == "astc4x4")     { out = F::ASTC_4x4_UNORM;  return true; }
    if (s == "astc4x4srgb") { out = F::ASTC_4x4_SRGB;   return true; }
    if (s == "astc6x6")     { out = F::ASTC_6x6_UNORM;  return true; }
    if (s == "astc6x6srgb") { out = F::ASTC_6x6_SRGB;   return true; }
    if (s == "astc8x8")     { out = F::ASTC_8x8_UNORM;  return true; }
    if (s == "astc8x8srgb") { out = F::ASTC_8x8_SRGB;   return true; }
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
    std::string ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".gltf" || ext == ".glb" || ext == ".fbx"
        || ext == ".obj" || ext == ".dae" || ext == ".blend"
        || ext == ".3ds" || ext == ".stl" || ext == ".ply";
}

bool IsGltfInput(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".gltf" || ext == ".glb";
}

bool NeedsAssimpImporter(const std::filesystem::path& path) {
    return HasSupportedInputExtension(path) && !IsGltfInput(path);
}

int RunLightmapBake(int argc, char** argv) {
    // argv[1] == "--bake-lightmap"; expect <scene> <output.dlightmap> [options]
    if (argc < 4) {
        std::cerr << "[AssetBuilder] --bake-lightmap requires <scene.gltf/glb> <output.dlightmap>." << std::endl;
        return 1;
    }
    const std::filesystem::path scene_path = argv[2];
    const std::filesystem::path output_path = argv[3];

    dse::render::LightmapBakeConfig config;
    for (int i = 4; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt == "--lm-resolution" && i + 1 < argc) {
            config.resolution = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (opt == "--lm-samples" && i + 1 < argc) {
            config.samples_per_texel = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (opt == "--lm-bounces" && i + 1 < argc) {
            config.bounces = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (opt == "--lm-no-ao") {
            config.bake_ao = false;
        } else if (opt == "--lm-no-denoise") {
            config.denoise = false;
        }
    }

    // Import scene using GltfImporter (glTF/GLB) or FbxImporter (FBX/OBJ/Blend/DAE/...)
    RawSceneData raw_scene;
    bool import_ok = false;
    if (NeedsAssimpImporter(scene_path)) {
        FbxImporter fbx;
        import_ok = fbx.Import(scene_path.string(), raw_scene);
    } else {
        GltfImporter gltf;
        import_ok = gltf.Import(scene_path.string(), raw_scene);
    }
    if (!import_ok) {
        std::cerr << "[AssetBuilder] Failed to import scene: " << scene_path.string() << std::endl;
        return 1;
    }

    // Build bake scene from imported meshes
    dse::render::BakeScene bake_scene;
    for (const auto& submesh : raw_scene.meshes) {
        if (submesh.indices.empty() || submesh.positions.empty()) continue;
        for (size_t i = 0; i + 2 < submesh.indices.size(); i += 3) {
            uint32_t i0 = submesh.indices[i];
            uint32_t i1 = submesh.indices[i + 1];
            uint32_t i2 = submesh.indices[i + 2];

            dse::render::BakeTriangle tri;
            tri.v0 = submesh.positions[i0];
            tri.v1 = submesh.positions[i1];
            tri.v2 = submesh.positions[i2];

            if (!submesh.normals.empty()) {
                tri.n0 = submesh.normals[i0];
                tri.n1 = submesh.normals[i1];
                tri.n2 = submesh.normals[i2];
            } else {
                glm::vec3 face_n = glm::normalize(glm::cross(tri.v1 - tri.v0, tri.v2 - tri.v0));
                tri.n0 = tri.n1 = tri.n2 = face_n;
            }

            if (!submesh.texcoords.empty()) {
                tri.uv0 = submesh.texcoords[i0];
                tri.uv1 = submesh.texcoords[i1];
                tri.uv2 = submesh.texcoords[i2];
            }

            bake_scene.triangles.push_back(tri);
        }
    }

    // Default directional light if none in scene
    if (bake_scene.lights.empty()) {
        dse::render::BakeLight sun;
        sun.type = dse::render::BakeLight::Type::Directional;
        sun.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
        sun.color = glm::vec3(1.0f, 0.95f, 0.85f);
        sun.intensity = 2.0f;
        bake_scene.lights.push_back(sun);
    }

    std::cout << "[AssetBuilder] Baking lightmap: " << bake_scene.triangles.size() << " triangles, "
              << config.resolution << "x" << config.resolution << ", "
              << config.samples_per_texel << " spp, " << config.bounces << " bounces..." << std::endl;

    dse::render::LightmapBaker baker;
    auto result = baker.Bake(bake_scene, config, [](float p) {
        if (static_cast<int>(p * 100) % 10 == 0) {
            std::cout << "  " << static_cast<int>(p * 100) << "%" << std::endl;
        }
    });

    if (!result.success) {
        std::cerr << "[AssetBuilder] Lightmap bake failed." << std::endl;
        return 1;
    }

    if (!dse::render::LightmapBaker::SaveToFile(result, output_path.string())) {
        std::cerr << "[AssetBuilder] Failed to save lightmap: " << output_path.string() << std::endl;
        return 1;
    }

    std::cout << "[AssetBuilder] Lightmap saved: " << output_path.string()
              << " (" << result.width << "x" << result.height << ")" << std::endl;
    return 0;
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

    if (first_arg == "--bake-lightmap") {
        return RunLightmapBake(argc, argv);
    }

    // Strip recognized flags so the remaining args are positional.
    AnimCompressOptions anim_opts;  // quantize + reduce on by default
    float decimate_ratio = 0.0f;   // 0 = no decimation
    int lod_levels = 0;            // 0 = no LOD generation
    std::vector<std::string> pos_args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--no-anim-compress") { anim_opts.quantize = false; continue; }
        if (a == "--no-anim-reduce")   { anim_opts.reduce_keyframes = false; continue; }
        if (a == "--decimate") {
            if (i + 1 < argc) { decimate_ratio = std::stof(argv[++i]); }
            continue;
        }
        if (a == "--lod-levels") {
            if (i + 1 < argc) { lod_levels = std::stoi(argv[++i]); }
            continue;
        }
        pos_args.push_back(a);
    }

    if (pos_args.size() != 2 && pos_args.size() != 3) {
        std::cerr << "[AssetBuilder] Invalid arguments. Use --help for usage." << std::endl;
        return 1;
    }

    const std::filesystem::path input_path = pos_args[0];
    if (!HasSupportedInputExtension(input_path)) {
        std::cerr << "[AssetBuilder] Unsupported input extension: " << input_path.extension().string()
                  << ". Supported: .gltf/.glb/.fbx/.obj/.dae/.blend/.3ds/.stl/.ply" << std::endl;
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
    if (NeedsAssimpImporter(input_path)) {
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

    // ─── Mesh Decimation ───────────────────────────────────────────────
    if (decimate_ratio > 0.0f && decimate_ratio < 1.0f && !scene.meshes.empty()) {
        std::cout << "[AssetBuilder] Decimating meshes to " << (decimate_ratio * 100.0f)
                  << "% triangles..." << std::endl;
        dse::mesh::MeshDecimator decimator;
        dse::mesh::DecimationConfig dec_cfg;
        dec_cfg.target_ratio = decimate_ratio;

        for (auto& submesh : scene.meshes) {
            uint32_t orig_tris = static_cast<uint32_t>(submesh.indices.size() / 3);
            if (orig_tris < 4) continue;  // 太少不值得减

            dse::mesh::DecimationInput input;
            input.positions = submesh.positions.data();
            input.normals = submesh.normals.empty() ? nullptr : submesh.normals.data();
            input.texcoords = submesh.texcoords.empty() ? nullptr : submesh.texcoords.data();
            input.vertex_count = static_cast<uint32_t>(submesh.positions.size());
            input.indices = submesh.indices.data();
            input.index_count = static_cast<uint32_t>(submesh.indices.size());

            auto result = decimator.Decimate(input, dec_cfg);
            if (result.success) {
                submesh.positions = std::move(result.positions);
                submesh.normals = std::move(result.normals);
                submesh.texcoords = std::move(result.texcoords);
                submesh.indices = std::move(result.indices);
                // tangents/colors/joints 被丢弃（需要后续重新生成 tangent）
                submesh.tangents.clear();
                submesh.colors.clear();
                std::cout << "  [" << submesh.name << "] " << orig_tris << " -> "
                          << result.result_triangle_count << " tris" << std::endl;
            }
        }
    }

    // ─── LOD Generation ────────────────────────────────────────────────
    if (lod_levels > 0 && !scene.meshes.empty()) {
        std::cout << "[AssetBuilder] Generating " << lod_levels << " LOD levels..." << std::endl;
        dse::mesh::MeshDecimator decimator;

        // 对原始 scene（未 decimate 的）的第一个 submesh 生成 LOD
        // 各 LOD 单独输出为 base_name_lod1.dmesh, base_name_lod2.dmesh, ...
        for (int lod = 1; lod <= lod_levels; ++lod) {
            float ratio = 1.0f / static_cast<float>(1 << lod);  // 0.5, 0.25, 0.125...
            RawSceneData lod_scene = scene;  // 拷贝

            dse::mesh::DecimationConfig dec_cfg;
            dec_cfg.target_ratio = ratio;

            for (auto& submesh : lod_scene.meshes) {
                uint32_t orig_tris = static_cast<uint32_t>(submesh.indices.size() / 3);
                if (orig_tris < 4) continue;

                dse::mesh::DecimationInput input;
                input.positions = submesh.positions.data();
                input.normals = submesh.normals.empty() ? nullptr : submesh.normals.data();
                input.texcoords = submesh.texcoords.empty() ? nullptr : submesh.texcoords.data();
                input.vertex_count = static_cast<uint32_t>(submesh.positions.size());
                input.indices = submesh.indices.data();
                input.index_count = static_cast<uint32_t>(submesh.indices.size());

                auto result = decimator.Decimate(input, dec_cfg);
                if (result.success) {
                    submesh.positions = std::move(result.positions);
                    submesh.normals = std::move(result.normals);
                    submesh.texcoords = std::move(result.texcoords);
                    submesh.indices = std::move(result.indices);
                    submesh.tangents.clear();
                    submesh.colors.clear();
                }
            }

            std::string lod_name = base_name + "_lod" + std::to_string(lod);
            std::filesystem::path lod_path = output_dir / (lod_name + ".dmesh");
            MeshCooker lod_cooker;
            if (lod_cooker.CookToDmesh(lod_scene, lod_path.string())) {
                uint32_t total_tris = 0;
                for (auto& m : lod_scene.meshes) total_tris += static_cast<uint32_t>(m.indices.size() / 3);
                std::cout << "  LOD" << lod << ": " << lod_path.filename().string()
                          << " (" << total_tris << " tris, " << (ratio * 100.0f) << "%)" << std::endl;
            }
        }
    }

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
        std::cout << "[AssetBuilder] Cooking " << scene.animations.size() << " animation(s)"
                  << (anim_opts.quantize ? " (v3 quantized" : " (v2 raw")
                  << (anim_opts.quantize && anim_opts.reduce_keyframes ? "+reduced)" : ")")
                  << std::endl;
        for (size_t i = 0; i < scene.animations.size(); ++i) {
            std::cout << "  [" << i << "] " << (scene.animations[i].name.empty() ? "(unnamed)" : scene.animations[i].name)
                      << " (" << scene.animations[i].channels.size() << " channels, "
                      << scene.animations[i].duration << "s)" << std::endl;
        }
        if (!cooker.CookToDanim(scene, output_dir.string(), base_name, anim_opts)) {
            std::cerr << "[AssetBuilder] Failed to cook danim(s)." << std::endl;
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

