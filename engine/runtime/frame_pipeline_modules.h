/**
 * @file frame_pipeline_modules.h
 * @brief FramePipeline 内部 Pimpl 结构体定义（仅 .cpp 文件使用）
 *
 * 将 modules/ 依赖隔离在实现文件中，frame_pipeline.h 只保留前向声明，
 * 确保 engine/ 头文件不再传递依赖 modules/ 头文件。
 */

#ifndef DSE_FRAME_PIPELINE_MODULES_H
#define DSE_FRAME_PIPELINE_MODULES_H

#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "modules/gameplay_2d/camera/camera_system.h"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
#include "modules/gameplay_2d/spine/spine_system.h"
#include "modules/gameplay_2d/gameplay_2d_module.h"
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#ifdef DSE_ENABLE_3D
#include "modules/gameplay_3d/gameplay_3d_module.h"
#else
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "modules/gameplay_3d/ai/steering_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#endif

struct FramePipelineModules {
    dse::gameplay2d::Gameplay2DModule gameplay2d_module;
    dse::gameplay3d::MeshRenderSystem mesh_render_system;
#ifdef DSE_ENABLE_3D
    dse::gameplay3d::Gameplay3DModule gameplay3d_module;
#else
    dse::gameplay3d::Particle3DSystem particle3d_system;
    dse::gameplay3d::SteeringSystem steering_system;
    dse::gameplay3d::AnimatorSystem animator3d_system;
#endif
};

#endif
