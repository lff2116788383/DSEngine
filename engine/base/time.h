/**
 * @file time.h
 * @brief 时间管理系统，提供高精度计时器、增量时间(Delta Time)计算
 */

//
// Created by captain on 2021/8/5.
//

#ifndef UNTITLED_TIME_H
#define UNTITLED_TIME_H


#include <string>
#include <chrono>

/**
 * @class Time
 * @brief 全局时间管理类，负责记录游戏运行时间、帧增量时间及固定更新时间。
 */
class Time
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
     * @brief 获取固定的物理更新步长
     * @return 固定更新时间（秒）
     */
    static float fixed_update_time();

    /**
     * @brief 设置固定的物理更新步长
     * @param time 设定的时间步长（秒）
     */
    static void set_fixed_update_time(float time);

private:
    static std::chrono::system_clock::time_point startup_time_;
    static float last_frame_time_;
    //~zh 上一帧花费的时间
    //~en The time spent on the last frame
    static float delta_time_;

    //~zh 固定更新时间，一般用于物理模拟
    //~en Fixed update time, usually used for physics simulation
    static float fixed_update_time_;
};


#endif //UNTITLED_TIME_H
