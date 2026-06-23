/**
 * @file time.h
 * @brief 时间管理系统，提供高精度计时器、增量时间(Delta Time)计算
 */

//
// Created by captain on 2021/8/5.
//

#ifndef UNTITLED_TIME_H
#define UNTITLED_TIME_H


#include "engine/core/dse_export.h"
#include <string>
#include <chrono>

/**
 * @class Time
 * @brief 全局时间管理类，负责记录游戏运行时间、帧增量时间及固定更新时间。
 */
class DSE_EXPORT Time
{
public:
    Time();
    ~Time();

    /**
     * @brief 初始化时间系统，记录引擎启动时的初始时间点
     */
    static void Init();

    /**
     * @brief 每帧更新，计算距离上一帧经过的增量时间
     */
    static void Update();

    /**
     * @brief 获取自引擎启动以来的总运行时间
     * @return 运行时间（秒）
     */
    static float TimeSinceStartup();

    /**
     * @brief 获取上一帧到当前帧的增量时间（Delta Time）
     * @return 增量时间（秒）
     */
    static float delta_time();

    /**
     * @brief 获取缩放后的增量时间（delta_time * time_scale）
     *
     * 供 gameplay / 动画 / 粒子 / Tween / 物理累加器使用；UI / 输入 / 统计应使用
     * delta_time()（真实时间，不受 time-scale 影响）。
     * @return 缩放后增量时间（秒）
     */
    static float scaled_delta_time();

    /**
     * @brief 获取全局时间缩放（0=暂停, 1=正常, 0.5=半速, >1=快进）
     */
    static float time_scale();

    /**
     * @brief 设置全局时间缩放，负值钳制为 0
     */
    static void set_time_scale(float scale);

    /**
     * @brief 获取固定的物理更新步长
     * @return 固定更新时间（秒）
     */
    static float fixed_update_time();

    /**
     * @brief 设置固定的物理更新步长
     * @param time 设定的时间步长（秒）
     */
    static void set_fixed_update_time(float time);

    /**
     * @brief 重置时间状态为默认值（用于测试隔离）
     *
     * 将 delta_time、last_frame_time 恢复为 0，fixed_update_time 恢复为 1/60，
     * startup_time 重置为当前时刻。
     */
    static void Reset();

private:
    static std::chrono::steady_clock::time_point startup_time_;
    static float last_frame_time_;
    //~zh 上一帧花费的时间
    //~en The time spent on the last frame
    static float delta_time_;

    //~zh 固定更新时间，一般用于物理模拟
    //~en Fixed update time, usually used for physics simulation
    static float fixed_update_time_;

    //~zh 全局时间缩放（0=暂停, 1=正常）
    //~en Global time scale (0=paused, 1=normal)
    static float time_scale_;
};


#endif //UNTITLED_TIME_H
