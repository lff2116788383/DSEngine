#ifndef DSE_PREFAB_H
#define DSE_PREFAB_H

#include "resource/resource.h"
#include "component/game_object.h"
#include <string>

class Prefab : public Resource {
public:
    Prefab();
    virtual ~Prefab();

    // Create a new GameObject instance from this prefab
    GameObject* Instantiate();

    // Load prefab data from JSON/YAML
    virtual bool Load(const std::string& path) override;

    // Save a GameObject as prefab
    static bool Create(GameObject* root, const std::string& path);

private:
    std::string data_; // Serialized data
};

#endif // DSE_PREFAB_H
