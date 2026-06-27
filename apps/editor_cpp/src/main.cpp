#include "editor_app.h"

int main(int argc, char* argv[]) {
    dse::editor::EditorApp app;
    if (!app.Init(argc, argv)) return -1;
    app.Run();
    app.Shutdown();
    return app.ExitCode();
}
