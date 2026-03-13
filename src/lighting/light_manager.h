#ifndef DSE_LIGHT_MANAGER_H
#define DSE_LIGHT_MANAGER_H

#include "light_2d.h"
#include <vector>

class LightManager {
public:
    static void RegisterLight(Light2D* light) {
        lights_.push_back(light);
    }

    static void UnregisterLight(Light2D* light) {
        for (auto it = lights_.begin(); it != lights_.end(); ++it) {
            if (*it == light) {
                lights_.erase(it);
                break;
            }
        }
    }

    static const std::vector<Light2D*>& GetLights() { return lights_; }
    
    // In a real engine, we would pass these lights to the renderer/shader
    static void SubmitToShader(unsigned int shader_program) {
        // for each light, set uniforms like u_Lights[i].Position, Color, etc.
    }

private:
    static std::vector<Light2D*> lights_;
};

#endif // DSE_LIGHT_MANAGER_H
