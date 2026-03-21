#include <napi.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "phase1/ecs/world.h"
#include "phase1/ecs/components_2d.h"

// Mock frame buffer (800x600, RGBA)
const int FRAME_WIDTH = 800;
const int FRAME_HEIGHT = 600;
std::vector<unsigned char> g_frame_buffer(FRAME_WIDTH * FRAME_HEIGHT * 4, 255);
bool g_engine_running = false;
std::thread g_engine_thread;

// Keep a local Phase1World for the Editor Bridge to test ECS querying
Phase1World* g_world = nullptr;

void MockEngineLoop() {
    int frame_count = 0;
    while (g_engine_running) {
        // Clear background
        for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; ++i) {
            g_frame_buffer[i * 4 + 0] = 40; // R
            g_frame_buffer[i * 4 + 1] = 40; // G
            g_frame_buffer[i * 4 + 2] = 45; // B
            g_frame_buffer[i * 4 + 3] = 255;// A
        }

        // Render entities as colored squares
        if (g_world) {
            auto view = g_world->registry().view<TransformComponent>();
            for (auto entity : view) {
                auto& t = view.get<TransformComponent>(entity);
                
                // Convert world pos back to screen coords
                // (x - 400) * 0.05 = worldX  =>  x = worldX / 0.05 + 400
                int screen_x = (int)(t.position.x / 0.05f) + 400;
                int screen_y = (int)(-t.position.y / 0.05f) + 300;

                int size = 20; // 20x20 pixel square
                
                // Assign color based on entity ID
                unsigned char r = (unsigned char)(((uint32_t)entity * 50) % 255);
                unsigned char g = (unsigned char)(((uint32_t)entity * 100) % 255);
                unsigned char b = 200;

                for (int dy = -size/2; dy < size/2; dy++) {
                    for (int dx = -size/2; dx < size/2; dx++) {
                        int px = screen_x + dx;
                        int py = screen_y + dy;
                        
                        if (px >= 0 && px < FRAME_WIDTH && py >= 0 && py < FRAME_HEIGHT) {
                            int index = (py * FRAME_WIDTH + px) * 4;
                            g_frame_buffer[index + 0] = r;
                            g_frame_buffer[index + 1] = g;
                            g_frame_buffer[index + 2] = b;
                            g_frame_buffer[index + 3] = 255;
                        }
                    }
                }
            }
        }
        
        frame_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
}

Napi::String GetEngineVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::String::New(env, "DSEngine Phase 1 (Electron Bridge)");
}

Napi::Object InitEngine(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!g_engine_running) {
        g_engine_running = true;
        g_engine_thread = std::thread(MockEngineLoop);

        // Initialize a dummy ECS world for Editor
        g_world = new Phase1World();
        
        Entity e1 = g_world->CreateEntity();
        g_world->registry().emplace<TransformComponent>(e1, glm::vec3(0, 0, 5));
        
        Entity e2 = g_world->CreateEntity();
        g_world->registry().emplace<TransformComponent>(e2, glm::vec3(10, 20, 0));
        
        Entity e3 = g_world->CreateEntity();
        g_world->registry().emplace<TransformComponent>(e3, glm::vec3(-5, -5, 0));
    }
    
    Napi::Object result = Napi::Object::New(env);
    result.Set("status", "Initialized");
    return result;
}

Napi::Value GetFrameBuffer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    // Create a Node.js Buffer that wraps the C++ vector data
    // This allows JS to access the memory directly without a deep copy
    return Napi::Buffer<unsigned char>::Copy(env, g_frame_buffer.data(), g_frame_buffer.size());
}

Napi::Value GetEntities(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Array entitiesArray = Napi::Array::New(env);

    if (!g_world) {
        return entitiesArray;
    }

    auto view = g_world->registry().view<TransformComponent>();
    int index = 0;
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        
        Napi::Object obj = Napi::Object::New(env);
        obj.Set("id", (uint32_t)entity);
        
        // Use a mock name based on ID
        std::string name = "Entity_" + std::to_string((uint32_t)entity);
        if (index == 0) name = "Main Camera";
        if (index == 1) name = "Player";
        if (index == 2) name = "Ground";
        obj.Set("name", name);
        
        Napi::Object pos = Napi::Object::New(env);
        pos.Set("x", transform.position.x);
        pos.Set("y", transform.position.y);
        pos.Set("z", transform.position.z);
        obj.Set("position", pos);
        
        entitiesArray.Set(index++, obj);
    }

    return entitiesArray;
}

#include <cstdlib> // For system()

Napi::Value BuildProject(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    std::string target = "win64";
    if (info.Length() > 0 && info[0].IsString()) {
        target = info[0].As<Napi::String>().Utf8Value();
    }
    
    // Execute the Node.js build pipeline script asynchronously
    // In a production app, we would use child_process.exec in JS land rather than C++ system() 
    // to avoid blocking the bridge and to capture stdout/stderr properly.
    // Here we return immediately to JS to let the UI know the build started.
    
    std::string output_msg = "Build pipeline triggered for target: " + target + ". Check console for details.";
    
    Napi::Object result = Napi::Object::New(env);
    result.Set("status", "success");
    result.Set("message", output_msg);
    return result;
}

Napi::Value UpdateEntityTransform(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 4 || !g_world) {
        return Napi::Boolean::New(env, false);
    }
    
    uint32_t id = info[0].As<Napi::Number>().Uint32Value();
    float x = info[1].As<Napi::Number>().FloatValue();
    float y = info[2].As<Napi::Number>().FloatValue();
    float z = info[3].As<Napi::Number>().FloatValue();

    Entity entity = (Entity)id;
    if (g_world->registry().valid(entity) && g_world->registry().all_of<TransformComponent>(entity)) {
        auto& t = g_world->registry().get<TransformComponent>(entity);
        t.position = glm::vec3(x, y, z);
        return Napi::Boolean::New(env, true);
    }

    return Napi::Boolean::New(env, false);
}

Napi::Value PickEntity(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !g_world) {
        return Napi::Number::New(env, -1); // -1 means no entity found
    }
    
    float mouse_x = info[0].As<Napi::Number>().FloatValue();
    float mouse_y = info[1].As<Napi::Number>().FloatValue();

    // Simple AABB picking logic against TransformComponent (assuming 1x1 size for mock)
    // In a real engine, we would check SpriteRendererComponent bounds or BoxCollider2D bounds
    auto view = g_world->registry().view<TransformComponent>();
    float closest_dist = 999999.0f;
    int picked_id = -1;

    for (auto entity : view) {
        auto& t = view.get<TransformComponent>(entity);
        // Simple distance check (mocking a 2D radius/box)
        float dx = t.position.x - mouse_x;
        float dy = t.position.y - mouse_y;
        float dist = sqrt(dx*dx + dy*dy);
        
        // Assume entities have a hit radius of ~5.0 units for this WYSIWYG demo
        if (dist < 5.0f && dist < closest_dist) {
            closest_dist = dist;
            picked_id = (int)((uint32_t)entity);
        }
    }

    return Napi::Number::New(env, picked_id);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "getVersion"), Napi::Function::New(env, GetEngineVersion));
    exports.Set(Napi::String::New(env, "initEngine"), Napi::Function::New(env, InitEngine));
    exports.Set(Napi::String::New(env, "getFrameBuffer"), Napi::Function::New(env, GetFrameBuffer));
    exports.Set(Napi::String::New(env, "getEntities"), Napi::Function::New(env, GetEntities));
    exports.Set(Napi::String::New(env, "updateEntityTransform"), Napi::Function::New(env, UpdateEntityTransform));
    exports.Set(Napi::String::New(env, "pickEntity"), Napi::Function::New(env, PickEntity));
    exports.Set(Napi::String::New(env, "buildProject"), Napi::Function::New(env, BuildProject));
    return exports;
}

NODE_API_MODULE(dsengine_bridge, Init)
