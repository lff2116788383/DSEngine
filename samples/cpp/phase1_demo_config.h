#ifndef DSE_EXAMPLES_CPP_PHASE1_DEMO_CONFIG_H
#define DSE_EXAMPLES_CPP_PHASE1_DEMO_CONFIG_H

namespace phase1::samples::cpp_demo::config {

struct StressSettings {
    int total_boxes;
    int spawn_per_frame;
    int columns;
    float spacing;
    float start_y;
    float box_scale;
    float camera_ortho_size;
};

inline constexpr const char* kWindowTitle = "DSEngine";
inline constexpr const char* kDataPath = "data/";
inline constexpr StressSettings kPhase1_2D = {
    1024,
    64,
    64,
    0.55f,
    2.0f,
    0.45f,
    12.0f
};

}

#endif
