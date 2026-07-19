/**
 * @file time.cpp
 * @brief 时间管理系统，提供高精度计时器、增量时间(Delta Time)计算
 */

//
// Created by captain on 2021/8/5.
//

#include "time.h"
#include <cstdlib>

std::chrono::steady_clock::time_point Time::startup_time_;
float Time::delta_time_=0;
float Time::last_frame_time_=0;
float Time::fixed_update_time_=1.0/60;
float Time::time_scale_=1.0f;
bool Time::deterministic_=false;
bool Time::deterministic_checked_=false;
float Time::deterministic_dt_=1.0f/60.0f;
float Time::sim_time_=0.0f;

Time::Time() {
}

Time::~Time() {
}

float Time::delta_time() {
    return delta_time_;
}

float Time::scaled_delta_time() {
    return delta_time_ * time_scale_;
}

float Time::time_scale() {
    return time_scale_;
}

void Time::set_time_scale(float scale) {
    if (scale < 0.0f) scale = 0.0f;
    time_scale_ = scale;
}

float Time::fixed_update_time() {
    return fixed_update_time_;
}

void Time::set_fixed_update_time(float time) {
    fixed_update_time_ = time;
}

void Time::Reset() {
    startup_time_ = std::chrono::steady_clock::now();
    delta_time_ = 0.0f;
    last_frame_time_ = 0.0f;
    fixed_update_time_ = 1.0f / 60.0f;
    time_scale_ = 1.0f;
    sim_time_ = 0.0f;
}

void Time::Init() {
    startup_time_ = std::chrono::steady_clock::now();
    delta_time_ = 0.0f;
    last_frame_time_ = 0.0f;
    sim_time_ = 0.0f;
}

void Time::Update() {
    if (!deterministic_checked_) {
        deterministic_checked_ = true;
        if (const char* e = std::getenv("DSE_FIXED_DT")) {
            const float v = static_cast<float>(std::atof(e));
            if (v > 0.0f) { deterministic_ = true; deterministic_dt_ = v; }
        }
    }
    if (deterministic_) {
        delta_time_ = deterministic_dt_;
        sim_time_ += deterministic_dt_;
        last_frame_time_ = sim_time_;
        return;
    }
    const float now = TimeSinceStartup();
    delta_time_ = now - last_frame_time_;
    last_frame_time_ = now;
}

float Time::TimeSinceStartup() {
    if (deterministic_) return sim_time_;
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - startup_time_).count();
}