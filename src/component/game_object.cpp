//
// Created by captain on 2021/6/9.
//

#include "game_object.h"
#include <rttr/registration>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include "app/application.h"
#include "component.h"
#include "component/transform.h"
#include "renderer/sprite_renderer.h"
#include "renderer/tilemap.h"
#include "renderer/tilemap_renderer.h"
#include "renderer/grid.h"
#include "physics/rigid_dynamic.h"
#include "physics/rigid_static.h"
#include "physics/box_collider.h"
#include "physics/sphere_collider.h"
#include "utils/debug.h"

using namespace rttr;

Tree GameObject::game_object_tree_;//用树存储所有的GameObject。
std::list<GameObject*> GameObject::game_object_list_;

GameObject::GameObject(const char *name): Tree::Node(), layer_(0x01) {
    set_name(name);
    game_object_tree_.root_node()->AddChild(this);
}

GameObject::~GameObject() {
    DEBUG_LOG_INFO("GameObject::~GameObject");
}


bool GameObject::SetParent(GameObject* parent){
    if(parent== nullptr){
        DEBUG_LOG_ERROR("parent null");
        return false;
    }
    parent->AddChild(this);
    return true;
}

GameObject* GameObject::Find(const char *name) {
    GameObject* game_object_find= nullptr;
    game_object_tree_.Find(game_object_tree_.root_node(), [&name](Tree::Node* node){
        GameObject* game_object=dynamic_cast<GameObject*>(node);
        if(game_object->name()==name){
            return true;
        }
        return false;
    }, reinterpret_cast<Node **>(&game_object_find));
    return game_object_find;
}

bool GameObject::SaveScene(const std::string& file_path) {
    std::ofstream output(Application::data_path() + file_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    std::unordered_map<GameObject*, int> ids;
    std::vector<GameObject*> objects;
    int next_id = 0;

    GameObject::Foreach([&](GameObject* game_object)->bool {
        if (!game_object) {
            return true;
        }
        ids[game_object] = next_id++;
        objects.push_back(game_object);
        return true;
    });

    output << "SCENE 1\n";

    for (auto* game_object : objects) {
        int id = ids[game_object];
        int parent_id = -1;
        GameObject* parent = dynamic_cast<GameObject*>(game_object->parent());
        if (parent && ids.find(parent) != ids.end()) {
            parent_id = ids[parent];
        }
        output << "GO " << id << " " << parent_id << " " << (int)game_object->layer() << " " << (game_object->active_self() ? 1 : 0) << " " << std::quoted(std::string(game_object->name())) << "\n";

        Transform* transform = game_object->GetComponent<Transform>();
        if (transform) {
            glm::vec3 pos = transform->local_position();
            glm::vec3 rot = transform->local_rotation();
            glm::vec3 scale = transform->local_scale();
            output << "C Transform " << pos.x << " " << pos.y << " " << pos.z << " "
                   << rot.x << " " << rot.y << " " << rot.z << " "
                   << scale.x << " " << scale.y << " " << scale.z << "\n";
        }

        SpriteRenderer* sprite_renderer = game_object->GetComponent<SpriteRenderer>();
        if (sprite_renderer) {
            auto color = sprite_renderer->color();
            Sprite* sprite = sprite_renderer->sprite();
            std::string texture_path = "-";
            float rect_x = 0.0f;
            float rect_y = 0.0f;
            float rect_w = 0.0f;
            float rect_h = 0.0f;
            float pivot_x = 0.5f;
            float pivot_y = 0.5f;
            float ppu = 100.0f;
            if (sprite && !sprite->texture_path().empty()) {
                texture_path = sprite->texture_path();
                auto rect = sprite->rect();
                auto pivot = sprite->pivot();
                rect_x = rect.x;
                rect_y = rect.y;
                rect_w = rect.width;
                rect_h = rect.height;
                pivot_x = pivot.x;
                pivot_y = pivot.y;
                ppu = sprite->ppu();
            }
            output << "C SpriteRenderer " << sprite_renderer->sorting_layer() << " " << sprite_renderer->order_in_layer() << " "
                   << color.r << " " << color.g << " " << color.b << " " << color.a << " "
                   << std::quoted(texture_path) << " " << rect_x << " " << rect_y << " " << rect_w << " " << rect_h << " "
                   << pivot_x << " " << pivot_y << " " << ppu << "\n";
        }

        Grid* grid = game_object->GetComponent<Grid>();
        if (grid) {
            glm::vec2 cell_size = grid->cell_size();
            glm::vec2 cell_gap = grid->cell_gap();
            output << "C Grid " << cell_size.x << " " << cell_size.y << " " << cell_gap.x << " " << cell_gap.y << "\n";
        }

        Tilemap* tilemap = game_object->GetComponent<Tilemap>();
        if (tilemap) {
            std::string tilemap_file = file_path + "_tilemap_" + std::to_string(id) + ".txt";
            tilemap->SaveToFile(tilemap_file);
            output << "C Tilemap " << std::quoted(tilemap_file) << "\n";
        }

        TilemapRenderer* tilemap_renderer = game_object->GetComponent<TilemapRenderer>();
        if (tilemap_renderer) {
            glm::vec4 color = tilemap_renderer->color();
            output << "C TilemapRenderer " << color.r << " " << color.g << " " << color.b << " " << color.a << " "
                   << tilemap_renderer->sorting_layer() << " " << tilemap_renderer->order_in_layer() << "\n";
        }

        RigidDynamic* rigid_dynamic = game_object->GetComponent<RigidDynamic>();
        if (rigid_dynamic) {
            output << "C RigidDynamic " << (rigid_dynamic->is_2d_mode() ? 1 : 0) << "\n";
        }

        RigidStatic* rigid_static = game_object->GetComponent<RigidStatic>();
        if (rigid_static) {
            output << "C RigidStatic\n";
        }

        BoxCollider* box_collider = game_object->GetComponent<BoxCollider>();
        if (box_collider) {
            glm::vec3 size = box_collider->size();
            output << "C BoxCollider " << size.x << " " << size.y << " " << size.z << " " << (box_collider->is_trigger() ? 1 : 0) << "\n";
        }

        SphereCollider* sphere_collider = game_object->GetComponent<SphereCollider>();
        if (sphere_collider) {
            output << "C SphereCollider " << sphere_collider->radius() << " " << (sphere_collider->is_trigger() ? 1 : 0) << "\n";
        }
    }

    return true;
}

bool GameObject::LoadScene(const std::string& file_path) {
    std::ifstream input(Application::data_path() + file_path);
    if (!input.is_open()) {
        return false;
    }

    std::unordered_map<int, GameObject*> id_to_object;
    std::unordered_map<int, int> id_to_parent;
    GameObject* current = nullptr;

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag == "SCENE") {
            continue;
        }
        if (tag == "GO") {
            int id = 0;
            int parent_id = -1;
            int layer = 0;
            int active = 1;
            std::string name;
            stream >> id >> parent_id >> layer >> active >> std::quoted(name);
            GameObject* game_object = new GameObject(name.c_str());
            game_object->set_layer((unsigned char)layer);
            game_object->set_active_self(active != 0);
            id_to_object[id] = game_object;
            id_to_parent[id] = parent_id;
            current = game_object;
            continue;
        }
        if (tag == "C" && current) {
            std::string type;
            stream >> type;
            if (type == "Transform") {
                float px = 0.0f, py = 0.0f, pz = 0.0f;
                float rx = 0.0f, ry = 0.0f, rz = 0.0f;
                float sx = 1.0f, sy = 1.0f, sz = 1.0f;
                stream >> px >> py >> pz >> rx >> ry >> rz >> sx >> sy >> sz;
                Transform* transform = current->GetComponent<Transform>();
                if (!transform) {
                    transform = current->AddComponent<Transform>();
                }
                transform->set_local_position(glm::vec3(px, py, pz));
                transform->set_local_rotation(glm::vec3(rx, ry, rz));
                transform->set_local_scale(glm::vec3(sx, sy, sz));
            } else if (type == "SpriteRenderer") {
                int sorting_layer = 0;
                int order_in_layer = 0;
                float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
                std::string texture_path;
                float rect_x = 0.0f, rect_y = 0.0f, rect_w = 0.0f, rect_h = 0.0f;
                float pivot_x = 0.5f, pivot_y = 0.5f, ppu = 100.0f;
                stream >> sorting_layer >> order_in_layer >> r >> g >> b >> a >> std::quoted(texture_path)
                       >> rect_x >> rect_y >> rect_w >> rect_h >> pivot_x >> pivot_y >> ppu;
                SpriteRenderer* sprite_renderer = current->GetComponent<SpriteRenderer>();
                if (!sprite_renderer) {
                    sprite_renderer = current->AddComponent<SpriteRenderer>();
                }
                sprite_renderer->set_sorting_layer(sorting_layer);
                sprite_renderer->set_order_in_layer(order_in_layer);
                sprite_renderer->set_color(r, g, b, a);
                if (texture_path != "-") {
                    Sprite* sprite = Sprite::CreateFromAtlas(texture_path, rect_x, rect_y, rect_w, rect_h, pivot_x, pivot_y, ppu);
                    if (sprite) {
                        sprite_renderer->set_sprite(sprite);
                    }
                }
            } else if (type == "Grid") {
                float size_x = 1.0f, size_y = 1.0f, gap_x = 0.0f, gap_y = 0.0f;
                stream >> size_x >> size_y >> gap_x >> gap_y;
                Grid* grid = current->GetComponent<Grid>();
                if (!grid) {
                    grid = current->AddComponent<Grid>();
                }
                grid->set_cell_size(glm::vec2(size_x, size_y));
                grid->set_cell_gap(glm::vec2(gap_x, gap_y));
            } else if (type == "Tilemap") {
                std::string tilemap_file;
                stream >> std::quoted(tilemap_file);
                Tilemap* tilemap = current->GetComponent<Tilemap>();
                if (!tilemap) {
                    tilemap = current->AddComponent<Tilemap>();
                }
                tilemap->LoadFromFile(tilemap_file);
            } else if (type == "TilemapRenderer") {
                float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
                int sorting_layer = 0;
                int order_in_layer = 0;
                stream >> r >> g >> b >> a >> sorting_layer >> order_in_layer;
                TilemapRenderer* tilemap_renderer = current->GetComponent<TilemapRenderer>();
                if (!tilemap_renderer) {
                    tilemap_renderer = current->AddComponent<TilemapRenderer>();
                }
                tilemap_renderer->set_color(r, g, b, a);
                tilemap_renderer->set_sorting_layer(sorting_layer);
                tilemap_renderer->set_order_in_layer(order_in_layer);
            } else if (type == "RigidDynamic") {
                int is_2d = 0;
                stream >> is_2d;
                RigidDynamic* rigid_dynamic = current->GetComponent<RigidDynamic>();
                if (!rigid_dynamic) {
                    rigid_dynamic = current->AddComponent<RigidDynamic>();
                }
                rigid_dynamic->set_2d_mode(is_2d != 0);
            } else if (type == "RigidStatic") {
                if (!current->GetComponent<RigidStatic>()) {
                    current->AddComponent<RigidStatic>();
                }
            } else if (type == "BoxCollider") {
                float size_x = 1.0f, size_y = 1.0f, size_z = 1.0f;
                int is_trigger = 0;
                stream >> size_x >> size_y >> size_z >> is_trigger;
                BoxCollider* box = current->GetComponent<BoxCollider>();
                if (!box) {
                    box = current->AddComponent<BoxCollider>();
                }
                box->set_size(glm::vec3(size_x, size_y, size_z));
                box->set_is_trigger(is_trigger != 0);
            } else if (type == "SphereCollider") {
                float radius = 0.5f;
                int is_trigger = 0;
                stream >> radius >> is_trigger;
                SphereCollider* sphere = current->GetComponent<SphereCollider>();
                if (!sphere) {
                    sphere = current->AddComponent<SphereCollider>();
                }
                sphere->set_radius(radius);
                sphere->set_is_trigger(is_trigger != 0);
            }
        }
    }

    for (auto& pair : id_to_object) {
        int id = pair.first;
        GameObject* game_object = pair.second;
        int parent_id = id_to_parent[id];
        if (parent_id >= 0 && id_to_object.find(parent_id) != id_to_object.end()) {
            game_object->SetParent(id_to_object[parent_id]);
        }
    }

    return true;
}

/// 附加组件实例
/// \param component_instance_table
void GameObject::AttachComponent(Component* component){
    component->set_game_object(this);
    //获取类名
    type t=type::get(*component);
    std::string component_type_name=t.get_name().to_string();

    if(components_map_.find(component_type_name)==components_map_.end()){
        std::vector<Component*> component_vec;
        component_vec.push_back(component);
        components_map_[component_type_name]=component_vec;
    }else{
        components_map_[component_type_name].push_back(component);
    }
}

/// 遍历组件
/// \param func
void GameObject::ForeachComponent(std::function<void(Component*)> func) {
    for (auto& v : components_map_){
        for (auto& iter : v.second){
            Component* component=iter;
            func(component);
        }
    }
}

/// 遍历GameObject
/// \param func
void GameObject::Foreach(std::function<bool(GameObject* game_object)> func) {
    game_object_tree_.PreOrder(game_object_tree_.root_node(),[&func](Tree::Node* node)->bool {
        auto n=node;
        GameObject* game_object= dynamic_cast<GameObject *>(n);
        return func(game_object);
    });
}
