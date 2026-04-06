/**
 * @file debug.h
 * @brief 调试与日志系统，提供日志记录、断言和基础的调试工具
 */

//
// Created by captainchen on 2021/8/23.
//

#ifndef UNTITLED_DEBUG_H
#define UNTITLED_DEBUG_H

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "spdlog/spdlog.h"

/// 输出文件名
#define DEBUG_LOG_INFO(...) do { if(Debug::CanLog()) { SPDLOG_INFO(__VA_ARGS__); } } while(0)
#define DEBUG_LOG_WARN(...) do { if(Debug::CanLog()) { SPDLOG_WARN(__VA_ARGS__); } } while(0)
#define DEBUG_LOG_ERROR(...) do { if(Debug::CanLog()) { SPDLOG_ERROR(__VA_ARGS__); } } while(0)

#define __CHECK_GL_ERROR__ { \
        auto gl_error_code=glGetError();\
        if(gl_error_code!=GL_NO_ERROR){\
            DEBUG_LOG_ERROR("gl_error_code: {}",gl_error_code);\
        }\
    }

/**
 * @class Debug
 * @brief 调试类，提供日志系统的初始化和资源释放管理
 */
class Debug {
public:
    /**
     * @brief 初始化日志系统，配置控制台和文件多路输出
     */
    static void Init();

    static bool CanLog();

    /**
     * @brief 关闭并释放日志系统资源
     */
    static void ShutDown();

public:
    static bool bInited_;
};


#endif //UNTITLED_DEBUG_H
