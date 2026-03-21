#ifndef DSE_AUDIO_SYSTEM_H
#define DSE_AUDIO_SYSTEM_H

#include "phase1/ecs/world.h"
#include <string>
#include <miniaudio.h>

namespace audio {

class AudioSystem {
public:
    void Init();
    void Update(Phase1World& world);
    void Shutdown();

    // Direct playback API for Lua/C++ usage
    void PlaySound(const std::string& filepath, bool loop = false, float volume = 1.0f);
    void StopAll();

private:
    ma_engine engine_;
    bool is_initialized_ = false;
};

} // namespace audio

#endif // DSE_AUDIO_SYSTEM_H
