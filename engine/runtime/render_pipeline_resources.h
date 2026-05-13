#ifndef DSE_RENDER_PIPELINE_RESOURCES_H
#define DSE_RENDER_PIPELINE_RESOURCES_H

#include <vector>
#include "engine/render/rhi/rhi_device.h"

namespace dse::runtime {

struct RenderPipelineResources {
    unsigned int main_render_target = 0;
    unsigned int scene_render_target = 0;
    unsigned int ui_render_target = 0;
    unsigned int prez_render_target = 0;

    unsigned int pp_bloom_extract_rt = 0;
    std::vector<unsigned int> pp_bloom_mip_rts;

    unsigned int pp_ssao_rt = 0;       // 半分辨率 AO
    unsigned int pp_ssao_blur_rt = 0;  // 模糊后 AO
    unsigned int pp_contact_shadow_rt = 0;  // 接触阴影
    unsigned int pp_fxaa_rt = 0;       // FXAA 输出
    unsigned int pp_taa_rt = 0;        // TAA resolve 输出

    unsigned int pp_lum_temp_rt = 0;     // 64x64 log luminance
    unsigned int pp_lum_adapted_rt[2] = {0, 0}; // 1x1 ping-pong (EMA adapted exposure)

    unsigned int sprite_pipeline_state = 0;
    unsigned int mesh_pipeline_state = 0;
    unsigned int prez_pipeline_state = 0;
    unsigned int composite_pipeline_state = 0;
    unsigned int shadow_render_target[CSM_CASCADES] = {0, 0, 0};
    unsigned int spot_shadow_render_target[4] = {0, 0, 0, 0};
    unsigned int point_shadow_render_target[4] = {0, 0, 0, 0};
    unsigned int shadow_pipeline_state = 0;

    void Reset() {
        main_render_target = 0;
        scene_render_target = 0;
        ui_render_target = 0;
        prez_render_target = 0;
        for (int i = 0; i < CSM_CASCADES; ++i) {
            shadow_render_target[i] = 0;
        }
        for (int i = 0; i < 4; ++i) {
            spot_shadow_render_target[i] = 0;
            point_shadow_render_target[i] = 0;
        }
        pp_bloom_extract_rt = 0;
        pp_bloom_mip_rts.clear();
        pp_ssao_rt = 0;
        pp_ssao_blur_rt = 0;
        pp_contact_shadow_rt = 0;
        pp_fxaa_rt = 0;
        pp_taa_rt = 0;
        pp_lum_temp_rt = 0;
        pp_lum_adapted_rt[0] = 0;
        pp_lum_adapted_rt[1] = 0;
        sprite_pipeline_state = 0;
        mesh_pipeline_state = 0;
        prez_pipeline_state = 0;
        composite_pipeline_state = 0;
        shadow_pipeline_state = 0;
    }
};

} // namespace dse::runtime

#endif
