#include <spine/Extension.h>

namespace spine {
SpineExtension *getDefaultExtension() {
    static DefaultSpineExtension extension;
    return &extension;
}
}
