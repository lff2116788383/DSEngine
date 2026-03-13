#include "light_2d.h"
#include "light_manager.h"

Light2D::Light2D() 
    : color_(1.0f), intensity_(1.0f), range_(10.0f), type_(Type::Point) {
    LightManager::RegisterLight(this);
}

Light2D::~Light2D() {
    LightManager::UnregisterLight(this);
}

void Light2D::OnRender() {
    // Already managed by LightManager
}
