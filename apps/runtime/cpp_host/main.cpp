#include "engine/runtime/engine_app.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "samples/cpp/phase1_demo_config.h"
#include "samples/cpp/phase1_demo_logic.h"
#include <cstdlib>

namespace {
void ConfigureDataPath() {
    if (std::getenv("DSE_DATA_ROOT") != nullptr) {
        return;
    }
#if defined(_WIN32)
    _putenv_s("DSE_DATA_ROOT", phase1::samples::cpp_demo::config::kDataPath);
#else
    setenv("DSE_DATA_ROOT", phase1::samples::cpp_demo::config::kDataPath, 1);
#endif
}
}

int main() {
    ConfigureDataPath();
    phase1::runtime::ConfigureCppBusinessHooks({
        phase1::samples::cpp_demo::Bootstrap,
        phase1::samples::cpp_demo::Tick,
        phase1::samples::cpp_demo::Shutdown
    });
    return phase1::runtime::RunEngine({
        800,
        600,
        phase1::samples::cpp_demo::config::kWindowTitle,
        Phase1BusinessMode::Cpp,
        ""
    });
}
