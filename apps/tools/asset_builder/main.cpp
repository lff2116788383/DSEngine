#include "engine/assets/compiler/importer.h"
#include <iostream>
#include <string>

using namespace dse::asset::compiler;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: AssetBuilder <input.gltf/glb> <output.dmesh>" << std::endl;
        return 1;
    }
    
    std::string input_path = argv[1];
    std::string output_path = argv[2];
    
    GltfImporter importer;
    RawSceneData scene;
    
    std::cout << "Importing: " << input_path << std::endl;
    if (!importer.Import(input_path, scene)) {
        std::cerr << "Failed to import file." << std::endl;
        return 1;
    }
    
    std::cout << "Imported successfully. Meshes: " << scene.meshes.size() 
              << ", Materials: " << scene.materials.size() << std::endl;
              
    MeshCooker cooker;
    std::cout << "Cooking dmesh to: " << output_path << std::endl;
    if (!cooker.CookToDmesh(scene, output_path)) {
        std::cerr << "Failed to cook dmesh." << std::endl;
        return 1;
    }
    
    // Cook materials
    size_t last_slash = output_path.find_last_of("/\\");
    std::string output_dir = (last_slash != std::string::npos) ? output_path.substr(0, last_slash) : ".";
    std::string base_name = (last_slash != std::string::npos) ? output_path.substr(last_slash + 1) : output_path;
    size_t last_dot = base_name.find_last_of('.');
    if (last_dot != std::string::npos) {
        base_name = base_name.substr(0, last_dot);
    }
    
    std::cout << "Cooking dmat to: " << output_dir << "/" << base_name << ".dmat" << std::endl;
    cooker.CookToDmat(scene, output_dir, base_name);
    
    std::cout << "Cooking danim to: " << output_dir << "/" << base_name << ".danim" << std::endl;
    cooker.CookToDanim(scene, output_dir, base_name);

    std::cout << "Cooking dskel to: " << output_dir << "/" << base_name << ".dskel" << std::endl;
    cooker.CookToDskel(scene, output_dir, base_name);
    
    std::cout << "Cooked successfully!" << std::endl;
    return 0;
}
