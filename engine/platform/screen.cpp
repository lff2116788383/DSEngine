/**
 * @file screen.cpp
 * @brief 屏幕与窗口管理，处理分辨率、全屏切换等平台相关的窗口操作
 */

//
// Created by captain on 2021/6/21.
//

#include "screen.h"

int Screen::width_=0.f;
int Screen::height_=0.f;
float Screen::aspect_ratio_=0.f;
float Screen::render_scale_=1.0f;