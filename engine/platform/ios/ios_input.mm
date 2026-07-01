/**
 * @file ios_input.mm
 * @brief iOS 触屏输入 — DSETouchView 实现
 *
 * UIView 子类，接收 UIKit 触屏事件并转发到引擎的 PlatformApp::TouchCallback。
 * TouchPhase 映射与 Android 后端一致：
 *   1 = Began,  2 = Moved,  3 = Stationary,  4 = Ended,  5 = Cancelled
 */

#ifdef DSE_ENABLE_APPLE_PLATFORM

#import <UIKit/UIKit.h>
#include "engine/platform/platform_app.h"

// =============================================================================
// DSETouchView — 触屏事件接收视图
// =============================================================================

@interface DSETouchView : UIView

/// 引擎触屏回调（C 函数指针）
@property (nonatomic, assign) dse::platform::PlatformApp::TouchCallback touchCallback;

/// 视图缩放因子（用于坐标转换到像素坐标）
@property (nonatomic, assign) CGFloat contentScaleFactor;

@end

@implementation DSETouchView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.multipleTouchEnabled = YES;
        self.userInteractionEnabled = YES;
        self.backgroundColor = [UIColor clearColor];
        _touchCallback = nullptr;
        _contentScaleFactor = [UIScreen mainScreen].scale;
    }
    return self;
}

#pragma mark - Touch Event Handling

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesBegan:touches withEvent:event];
    [self reportTouches:touches phase:1]; // Began
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesMoved:touches withEvent:event];
    [self reportTouches:touches phase:2]; // Moved
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesEnded:touches withEvent:event];
    [self reportTouches:touches phase:4]; // Ended
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [super touchesCancelled:touches withEvent:event];
    [self reportTouches:touches phase:5]; // Cancelled
}

#pragma mark - Internal

/// 将 UITouch 事件转发到引擎回调
- (void)reportTouches:(NSSet<UITouch*>*)touches phase:(int)phase {
    if (!self.touchCallback) return;

    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self];

        // 将点坐标转换为像素坐标（乘以 content scale factor）
        float px = static_cast<float>(location.x * self.contentScaleFactor);
        float py = static_cast<float>(location.y * self.contentScaleFactor);

        // 使用 UITouch 指针地址作为 finger_id（唯一标识同一触点的生命周期）
        int finger_id = static_cast<int>(reinterpret_cast<uintptr_t>(
            (__bridge void*)touch) & 0x7FFFFFFF);

        self.touchCallback(finger_id, px, py, phase);
    }
}

@end

// =============================================================================
// C 桥接函数 — 供 ios_app.mm 调用
// =============================================================================

void DSETouchView_SetCallback(UIView* view,
                               dse::platform::PlatformApp::TouchCallback cb) {
    if ([view isKindOfClass:[DSETouchView class]]) {
        ((DSETouchView*)view).touchCallback = cb;
    }
}

// =============================================================================
// 触屏阶段常量（与 PlatformApp::TouchCallback 文档对齐）
// =============================================================================
//
// UITouchPhaseBegan       → 1 (Began)
// UITouchPhaseMoved       → 2 (Moved)
// UITouchPhaseStationary  → 3 (Stationary) — 引擎通常不需要，但保留以完整映射
// UITouchPhaseEnded       → 4 (Ended)
// UITouchPhaseCancelled   → 5 (Cancelled)
//
// 引擎侧 (engine/input) 通过 finger_id 跟踪多点触控状态，
// 无需关心 UITouch 内部的 phase 枚举值。

#endif // DSE_ENABLE_APPLE_PLATFORM
