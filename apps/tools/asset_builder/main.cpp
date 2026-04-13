#include "engine/assets/compiler/importer.h"
#include <filesystem>
#include <iostream>
#include <string>

using namespace dse::asset::compiler;

namespace {

void PrintUsage() {
    std::cout
        << "Usage:\n"
        << "  AssetBuilder <input.gltf/glb/fbx> <output.dmesh>\n"
        << "  AssetBuilder <input.gltf/glb/fbx> --out-dir <output_dir>\n\n"
        << "Examples:\n"
        << "  AssetBuilder assets/model.glb cooked/model.dmesh\n"
        << "  AssetBuilder assets/model.fbx --out-dir cooked\n\n"
        << "Notes:\n"
        << "  - The tool imports glTF/GLB/FBX and cooks .dmesh/.dmat/.danim/.dskel in one pass.\n"
        << "  - FBX is imported offline through the asset compiler path; runtime does not read FBX directly.\n"
        << "  - When --out-dir is used, the base file name is derived from the input file stem.\n";
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

    if (argc != 3 && argc != 4) {
        std::cerr << "[AssetBuilder] Invalid arguments. Use --help for usage." << std::endl;
        return 1;
    }

    const std::filesystem::path input_path = argv[1];
    if (!HasSupportedInputExtension(input_path)) {
        std::cerr << "[AssetBuilder] Unsupported input extension: " << input_path.extension().string()
                  << ". Only .gltf/.glb/.fbx are supported." << std::endl;
        return 1;
    }



    std::filesystem::path output_dmesh_path;
    std::filesystem::path output_dir;
    std::string base_name;

    if (argc == 3) {
        output_dmesh_path = argv[2];
        output_dir = output_dmesh_path.parent_path().empty() ? std::filesystem::path(".") : output_dmesh_path.parent_path();
        base_name = output_dmesh_path.stem().string();
    } else {
        const std::string option = argv[2];
        if (option != "--out-dir") {
            std::cerr << "[AssetBuilder] Unknown option: " << option << ". Use --help for usage." << std::endl;
            return 1;
        }
        output_dir = argv[3];
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
    std::cout << "[AssetBuilder] Cooking dmesh: " << output_dmesh_path.string() << std::endl;
    if (!cooker.CookToDmesh(scene, output_dmesh_path.string())) {
        std::cerr << "[AssetBuilder] Failed to cook dmesh: " << output_dmesh_path.string() << std::endl;
        return 1;
    }

    const std::filesystem::path dmat_path = output_dir / (base_name + ".dmat");
    const std::filesystem::path danim_path = output_dir / (base_name + ".danim");
    const std::filesystem::path dskel_path = output_dir / (base_name + ".dskel");

    std::cout << "[AssetBuilder] Cooking dmat: " << dmat_path.string() << std::endl;
    if (!cooker.CookToDmat(scene, output_dir.string(), base_name)) {
        std::cerr << "[AssetBuilder] Failed to cook dmat: " << dmat_path.string() << std::endl;
        return 1;
    }

    if (scene.animations.empty()) {
        std::cout << "[AssetBuilder] Skip danim: no animation data in source." << std::endl;
    } else {
        std::cout << "[AssetBuilder] Cooking danim: " << danim_path.string() << std::endl;
        if (!cooker.CookToDanim(scene, output_dir.string(), base_name)) {
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

