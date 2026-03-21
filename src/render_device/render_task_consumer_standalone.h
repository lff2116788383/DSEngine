//
// Created by captainchen on 2022/2/7.
//

#ifndef UNTITLED_RENDER_TASK_CONSUMER_STANDALONE_H
#define UNTITLED_RENDER_TASK_CONSUMER_STANDALONE_H

#include "render_task_consumer_base.h"

struct GLFWwindow;
struct ImGuiContext;

/// 渲染任务消费端(PC系统游戏程序)
class RenderTaskConsumerStandalone : public RenderTaskConsumerBase {
public:
    RenderTaskConsumerStandalone(GLFWwindow* window);
    ~RenderTaskConsumerStandalone();

    void InitGraphicsLibraryFramework() override;
    void Exit() override;

    void GetFramebufferSize(int& width, int& height) override;
    
    void SetImGuiContext(ImGuiContext* ctx) { imgui_ctx_ = ctx; }

protected:
    void SwapBuffer() override;

private:
    GLFWwindow* window_;
    ImGuiContext* imgui_ctx_ = nullptr;
};

#endif //UNTITLED_RENDER_TASK_CONSUMER_STANDALONE_H
