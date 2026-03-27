/**
 * @file input.cpp
 * @brief 输入系统，处理键盘、鼠标和控制器的输入事件映射
 */

//
// Created by captain on 2021/6/20.
//

#include "input.h"
#include <iostream>
#include <cmath>
#include "key_code.h"
#include "engine/base/time.h"

std::unordered_map<unsigned short,unsigned short> Input::key_event_map_;
std::unordered_map<unsigned short,unsigned short> Input::key_event_map_current_frame_;
std::unordered_map<unsigned short, float> Input::key_down_timestamp_;
std::unordered_map<unsigned short, float> Input::key_last_click_timestamp_;
std::unordered_map<unsigned short, bool> Input::key_double_click_frame_;
glm::vec2 Input::mouse_position_={0,0};
glm::vec2 Input::previous_mouse_position_={0,0};
glm::vec2 Input::swipe_delta_={0,0};
glm::vec2 Input::previous_swipe_delta_={0,0};
float Input::mouse_scroll_=0.0f;
bool Input::device_shaking_=false;

bool Input::GetKey(unsigned short key_code) {
    return key_event_map_.count(key_code)>0;
}

bool Input::GetKeyDown(unsigned short key_code) {
    if(key_event_map_.count(key_code)==0){
        return false;
    }
    return key_event_map_[key_code]!=KEY_ACTION_UP;
}

bool Input::GetKeyUp(unsigned short key_code) {
    if(key_event_map_.count(key_code)==0){
        return false;
    }
    return key_event_map_[key_code]==KEY_ACTION_UP;
}

bool Input::GetMouseButton(unsigned short mouse_button_index) {
    return GetKey(mouse_button_index);
}

bool Input::GetMouseButtonDown(unsigned short mouse_button_index) {
    return GetKeyDown(mouse_button_index);
}

bool Input::GetMouseButtonUp(unsigned short mouse_button_index) {
    return GetKeyUp(mouse_button_index);
}

void Input::RecordMousePosition(float x, float y) {
    previous_mouse_position_ = mouse_position_;
    mouse_position_.x = x;
    mouse_position_.y = y;
    swipe_delta_ = mouse_position_ - previous_mouse_position_;
    const glm::vec2 swipe_accel = swipe_delta_ - previous_swipe_delta_;
    previous_swipe_delta_ = swipe_delta_;
    device_shaking_ = std::sqrt(swipe_accel.x * swipe_accel.x + swipe_accel.y * swipe_accel.y) > 80.0f;
}

void Input::RecordKey(unsigned short key_code, unsigned short key_action) {
    const float now = static_cast<float>(Time::TimeSinceStartup());
    const unsigned short previous_action = key_event_map_.count(key_code) > 0 ? key_event_map_[key_code] : KEY_ACTION_UP;
    if (key_action == KEY_ACTION_DOWN && previous_action == KEY_ACTION_UP) {
        key_down_timestamp_[key_code] = now;
        const auto click_it = key_last_click_timestamp_.find(key_code);
        const bool is_double_click = (click_it != key_last_click_timestamp_.end()) && ((now - click_it->second) <= 0.25f);
        key_double_click_frame_[key_code] = is_double_click;
        key_last_click_timestamp_[key_code] = now;
    } else if (key_action == KEY_ACTION_UP) {
        key_down_timestamp_.erase(key_code);
    }
    key_event_map_[key_code]=key_action;
}

void Input::Update() {
    for(auto iterator=key_event_map_.begin(); iterator != key_event_map_.end();) {
        if(iterator->second == KEY_ACTION_UP) {
            iterator = key_event_map_.erase(iterator);    //删除元素，返回值指向已删除元素的下一个位置
        } else {
            ++iterator;    //指向下一个位置
        }
    }

    key_double_click_frame_.clear();
    swipe_delta_ = glm::vec2(0.0f);
    device_shaking_ = false;
    mouse_scroll_ = 0;
}

bool Input::GetDoubleClick(unsigned short key_code) {
    const auto it = key_double_click_frame_.find(key_code);
    if (it == key_double_click_frame_.end()) {
        return false;
    }
    return it->second;
}

bool Input::GetLongPress(unsigned short key_code, float duration_seconds) {
    if (duration_seconds <= 0.0f) {
        duration_seconds = 0.0f;
    }
    if (!GetKey(key_code) || GetKeyUp(key_code)) {
        return false;
    }
    const auto it = key_down_timestamp_.find(key_code);
    if (it == key_down_timestamp_.end()) {
        return false;
    }
    const float now = static_cast<float>(Time::TimeSinceStartup());
    return (now - it->second) >= duration_seconds;
}

glm::vec2 Input::GetSwipeDelta() {
    return swipe_delta_;
}

bool Input::IsDeviceShaking() {
    return device_shaking_;
}
