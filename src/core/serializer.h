#ifndef DSE_SERIALIZER_H
#define DSE_SERIALIZER_H

#include <string>
#include <rttr/type>
#include "component/game_object.h"

// Simple JSON-based serializer using RTTR
class Serializer {
public:
    // Serialize a GameObject hierarchy to JSON string
    static std::string Serialize(GameObject* game_object);
    
    // Deserialize a JSON string to GameObject hierarchy
    static GameObject* Deserialize(const std::string& json_data);

    // Serialize a generic object (component, resource)
    static std::string SerializeObject(rttr::instance obj);
    
    // Deserialize into an existing object
    static void DeserializeObject(rttr::instance obj, const std::string& json_data);
};

#endif // DSE_SERIALIZER_H
