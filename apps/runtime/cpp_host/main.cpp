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
    _putenv_s("DSE_DATA_ROOT", dse::samples::cpp_demo::config::kDataPath);
#else
    setenv("DSE_DATA_ROOT", dse::samples::cpp_demo::config::kDataPath, 1);
#endif
}
}

int main() {
    ConfigureDataPath();
    dse::runtime::ConfigureCppBusinessHooks({
        dse::samples::cpp_demo::Bootstrap,
        dse::samples::cpp_demo::Tick,
        dse::samples::cpp_demo::Shutdown
    });
    return dse::runtime::RunEngine({
        800,
        600,
        dse::samples::cpp_demo::config::kWindowTitle,
        BusinessMode::Cpp,
        ""
    });
}
