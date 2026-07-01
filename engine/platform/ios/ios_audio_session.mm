/**
 * @file ios_audio_session.mm
 * @brief iOS AVAudioSession 配置 — miniaudio CoreAudio 后端前置设置
 *
 * iOS 要求在使用音频前配置 AVAudioSession。
 * miniaudio 的 ma_context_init 会自动选择 CoreAudio 后端，
 * 但需要提前设置 session category 以确保音频正确路由。
 *
 * 调用时机：在 AudioSystem::Init() / ma_engine_init() 之前
 */

#ifdef DSE_ENABLE_APPLE_PLATFORM

#import <AVFoundation/AVAudioSession.h>
#import <Foundation/Foundation.h>
#include "engine/base/debug.h"

namespace dse::platform::ios {

void ConfigureAudioSession() {
    @autoreleasepool {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;

        // Ambient: 与其他应用混音，静音开关生效，适合游戏默认行为
        BOOL success = [session setCategory:AVAudioSessionCategoryAmbient
                                      error:&error];
        if (!success || error) {
            DEBUG_LOG_WARN("[iOS Audio] Failed to set category: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
        }

        error = nil;
        success = [session setActive:YES error:&error];
        if (!success || error) {
            DEBUG_LOG_WARN("[iOS Audio] Failed to activate session: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
        }

        DEBUG_LOG_INFO("[iOS Audio] AVAudioSession configured (Ambient, active)");
    }
}

void DeactivateAudioSession() {
    @autoreleasepool {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;

        [session setActive:NO
              withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                    error:&error];
        if (error) {
            DEBUG_LOG_WARN("[iOS Audio] Failed to deactivate session: {}",
                           [[error localizedDescription] UTF8String]);
        }
    }
}

} // namespace dse::platform::ios

#endif // DSE_ENABLE_APPLE_PLATFORM
