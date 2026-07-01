/**
 * @file harmony_audio.cpp
 * @brief OHOS 音频配置 — miniaudio 后端前置验证
 *
 * 仅在 __OHOS__ 下编译。
 * 在引擎初始化阶段调用，验证 OHAudio 音频子系统可用性。
 * miniaudio 在 OHOS 上使用 OpenSL ES 或 AAudio 后端，
 * 此处提前创建 AudioStreamBuilder 确认音频通路正常。
 */

#ifdef __OHOS__

#include <ohaudio/native_audiostreambuilder.h>
#include <hilog/log.h>

#define OHOS_LOG_TAG "DSEngine"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, OHOS_LOG_TAG, __VA_ARGS__)

namespace dse::platform::harmony {

void ConfigureAudioSession() {
    OH_AudioStreamBuilder* builder = nullptr;
    OH_AudioStream_Result result =
        OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);

    if (result != AUDIOSTREAM_SUCCESS || !builder) {
        LOGI("OHAudio: AudioStreamBuilder creation failed, audio may not be available");
        return;
    }

    OH_AudioStreamBuilder_SetSamplingRate(builder, 44100);
    OH_AudioStreamBuilder_SetChannelCount(builder, 2);
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);

    OH_AudioStreamBuilder_Destroy(builder);
    LOGI("OHAudio: audio session configured (44100Hz, stereo, S16LE)");
}

void DeactivateAudioSession() {
    // OHOS 音频资源由系统自动管理，显式释放为 no-op
}

} // namespace dse::platform::harmony

#endif // __OHOS__
