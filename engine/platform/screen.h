/**
 * @file screen.h
 * @brief 屏幕与窗口管理，处理分辨率、全屏切换等平台相关的窗口操作
 */

//
// Created by captain on 2021/6/21.
//

#ifndef UNTITLED_SCREEN_H
#define UNTITLED_SCREEN_H

#include "engine/core/dse_export.h"

/**
 * @class Screen
 * @brief 屏幕状态管理类，提供全局的屏幕尺寸和宽高比访问
 */
class DSE_EXPORT Screen {
public:
    /**
     * @brief 获取当前窗口宽度
     * @return 宽度(像素)
     */
    static int width(){return width_;}
    
    /**
     * @brief 获取当前窗口高度
     * @return 高度(像素)
     */
    static int height(){return height_;}

    static int render_width()  { return static_cast<int>(width_  * render_scale_); }
    static int render_height() { return static_cast<int>(height_ * render_scale_); }
    static float render_scale() { return render_scale_; }
    static void set_render_scale(float s) { render_scale_ = (s < 0.25f) ? 0.25f : (s > 1.0f ? 1.0f : s); }

    /**
     * @brief 获取屏幕宽高比 (width / height)
     * @return 宽高比例的浮点数值
     */
    static float aspect_ratio(){return aspect_ratio_;}

    /**
     * @brief 设置屏幕宽度并自动更新宽高比
     * @param width 新的宽度
     */
    static void set_width(int width){width_=width;calculate_aspect_ratio();}
    
    /**
     * @brief 设置屏幕高度并自动更新宽高比
     * @param height 新的高度
     */
    static void set_height(int height){height_=height;calculate_aspect_ratio();}
    
    /**
     * @brief 同时设置屏幕的宽度和高度并更新宽高比
     * @param width 新的宽度
     * @param height 新的高度
     */
    static void set_width_height(int width,int height){width_=width;height_=height;calculate_aspect_ratio();}

    /**
     * @brief 重置屏幕状态为默认值（用于测试隔离）
     *
     * 将宽高和宽高比恢复为 0，等价于进程刚启动的初始状态。
     */
    static void Reset() { width_ = 0; height_ = 0; aspect_ratio_ = 0.0f; render_scale_ = 1.0f; }

private:
    /**
     * @brief 内部方法，根据当前宽高重新计算并缓存 aspect_ratio_
     */
    static void calculate_aspect_ratio(){aspect_ratio_=width_*1.0f/height_;}
private:
    static int width_;
    static int height_;
    static float aspect_ratio_;//宽高比
    static float render_scale_;
};


#endif //UNTITLED_SCREEN_H
