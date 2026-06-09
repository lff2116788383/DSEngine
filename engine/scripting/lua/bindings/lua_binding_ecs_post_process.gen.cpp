/**
 * @file lua_binding_ecs_post_process.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * PostProcessComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_post_process_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_enabled(e));
    return 1;
}
int L_Set_post_process_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_bloom_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_bloom_enabled(e));
    return 1;
}
int L_Set_post_process_bloom_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_bloom_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_bloom_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_bloom_threshold(e));
    return 1;
}
int L_Set_post_process_bloom_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_bloom_threshold(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_bloom_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_bloom_intensity(e));
    return 1;
}
int L_Set_post_process_bloom_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_bloom_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_bloom_knee(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_bloom_knee(e));
    return 1;
}
int L_Set_post_process_bloom_knee(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_bloom_knee(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_bloom_mip_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_bloom_mip_weight(e));
    return 1;
}
int L_Set_post_process_bloom_mip_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_bloom_mip_weight(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_color_grading_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_color_grading_enabled(e));
    return 1;
}
int L_Set_post_process_color_grading_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_color_grading_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_exposure(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_exposure(e));
    return 1;
}
int L_Set_post_process_exposure(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_exposure(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_gamma(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_gamma(e));
    return 1;
}
int L_Set_post_process_gamma(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_gamma(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssao_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_ssao_enabled(e));
    return 1;
}
int L_Set_post_process_ssao_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssao_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_ssao_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssao_radius(e));
    return 1;
}
int L_Set_post_process_ssao_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssao_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssao_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssao_bias(e));
    return 1;
}
int L_Set_post_process_ssao_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssao_bias(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssao_sample_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_post_process_get_ssao_sample_count(e));
    return 1;
}
int L_Set_post_process_ssao_sample_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssao_sample_count(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_post_process_ssao_power(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssao_power(e));
    return 1;
}
int L_Set_post_process_ssao_power(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssao_power(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssao_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssao_intensity(e));
    return 1;
}
int L_Set_post_process_ssao_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssao_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_auto_exposure_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_auto_exposure_enabled(e));
    return 1;
}
int L_Set_post_process_auto_exposure_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_auto_exposure_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_exposure_min(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_exposure_min(e));
    return 1;
}
int L_Set_post_process_exposure_min(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_exposure_min(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_exposure_max(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_exposure_max(e));
    return 1;
}
int L_Set_post_process_exposure_max(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_exposure_max(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_adaptation_speed_up(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_adaptation_speed_up(e));
    return 1;
}
int L_Set_post_process_adaptation_speed_up(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_adaptation_speed_up(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_adaptation_speed_down(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_adaptation_speed_down(e));
    return 1;
}
int L_Set_post_process_adaptation_speed_down(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_adaptation_speed_down(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_exposure_compensation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_exposure_compensation(e));
    return 1;
}
int L_Set_post_process_exposure_compensation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_exposure_compensation(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_color_lut_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_color_lut_intensity(e));
    return 1;
}
int L_Set_post_process_color_lut_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_color_lut_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_vignette_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_vignette_enabled(e));
    return 1;
}
int L_Set_post_process_vignette_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_vignette_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_vignette_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_vignette_intensity(e));
    return 1;
}
int L_Set_post_process_vignette_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_vignette_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_vignette_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_vignette_radius(e));
    return 1;
}
int L_Set_post_process_vignette_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_vignette_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_vignette_softness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_vignette_softness(e));
    return 1;
}
int L_Set_post_process_vignette_softness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_vignette_softness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_film_grain_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_film_grain_enabled(e));
    return 1;
}
int L_Set_post_process_film_grain_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_film_grain_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_film_grain_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_film_grain_intensity(e));
    return 1;
}
int L_Set_post_process_film_grain_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_film_grain_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_film_grain_time_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_film_grain_time_scale(e));
    return 1;
}
int L_Set_post_process_film_grain_time_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_film_grain_time_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_fxaa_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_fxaa_enabled(e));
    return 1;
}
int L_Set_post_process_fxaa_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fxaa_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_taa_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_taa_enabled(e));
    return 1;
}
int L_Set_post_process_taa_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_taa_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_taa_blend_factor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_taa_blend_factor(e));
    return 1;
}
int L_Set_post_process_taa_blend_factor(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_taa_blend_factor(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_contact_shadow_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_contact_shadow_enabled(e));
    return 1;
}
int L_Set_post_process_contact_shadow_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_contact_shadow_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_contact_shadow_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_contact_shadow_strength(e));
    return 1;
}
int L_Set_post_process_contact_shadow_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_contact_shadow_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_contact_shadow_steps(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_post_process_get_contact_shadow_steps(e));
    return 1;
}
int L_Set_post_process_contact_shadow_steps(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_contact_shadow_steps(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_post_process_contact_shadow_step_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_contact_shadow_step_size(e));
    return 1;
}
int L_Set_post_process_contact_shadow_step_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_contact_shadow_step_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_dof_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_dof_enabled(e));
    return 1;
}
int L_Set_post_process_dof_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_dof_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_dof_focus_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_dof_focus_distance(e));
    return 1;
}
int L_Set_post_process_dof_focus_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_dof_focus_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_dof_focus_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_dof_focus_range(e));
    return 1;
}
int L_Set_post_process_dof_focus_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_dof_focus_range(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_dof_bokeh_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_dof_bokeh_radius(e));
    return 1;
}
int L_Set_post_process_dof_bokeh_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_dof_bokeh_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_motion_blur_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_motion_blur_enabled(e));
    return 1;
}
int L_Set_post_process_motion_blur_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_motion_blur_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_motion_blur_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_motion_blur_intensity(e));
    return 1;
}
int L_Set_post_process_motion_blur_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_motion_blur_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_motion_blur_samples(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_post_process_get_motion_blur_samples(e));
    return 1;
}
int L_Set_post_process_motion_blur_samples(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_motion_blur_samples(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_post_process_ssr_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_ssr_enabled(e));
    return 1;
}
int L_Set_post_process_ssr_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_ssr_max_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssr_max_distance(e));
    return 1;
}
int L_Set_post_process_ssr_max_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_max_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssr_thickness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssr_thickness(e));
    return 1;
}
int L_Set_post_process_ssr_thickness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_thickness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssr_step_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssr_step_size(e));
    return 1;
}
int L_Set_post_process_ssr_step_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_step_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssr_max_steps(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_post_process_get_ssr_max_steps(e));
    return 1;
}
int L_Set_post_process_ssr_max_steps(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_max_steps(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_post_process_ssr_fade_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssr_fade_distance(e));
    return 1;
}
int L_Set_post_process_ssr_fade_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_fade_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_ssr_max_roughness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_ssr_max_roughness(e));
    return 1;
}
int L_Set_post_process_ssr_max_roughness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_ssr_max_roughness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_outline_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_outline_enabled(e));
    return 1;
}
int L_Set_post_process_outline_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_outline_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_outline_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_post_process_get_outline_color(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_post_process_outline_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_outline_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_post_process_outline_thickness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_outline_thickness(e));
    return 1;
}
int L_Set_post_process_outline_thickness(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_outline_thickness(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_outline_depth_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_outline_depth_threshold(e));
    return 1;
}
int L_Set_post_process_outline_depth_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_outline_depth_threshold(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_outline_normal_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_outline_normal_threshold(e));
    return 1;
}
int L_Set_post_process_outline_normal_threshold(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_outline_normal_threshold(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_light_shaft_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_light_shaft_enabled(e));
    return 1;
}
int L_Set_post_process_light_shaft_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_light_shaft_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_post_process_get_light_shaft_color(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_post_process_light_shaft_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_post_process_light_shaft_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_light_shaft_density(e));
    return 1;
}
int L_Set_post_process_light_shaft_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_density(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_light_shaft_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_light_shaft_weight(e));
    return 1;
}
int L_Set_post_process_light_shaft_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_weight(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_light_shaft_decay(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_light_shaft_decay(e));
    return 1;
}
int L_Set_post_process_light_shaft_decay(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_decay(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_light_shaft_exposure(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_light_shaft_exposure(e));
    return 1;
}
int L_Set_post_process_light_shaft_exposure(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_exposure(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_light_shaft_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_light_shaft_intensity(e));
    return 1;
}
int L_Set_post_process_light_shaft_intensity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_intensity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_light_shaft_samples(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_post_process_get_light_shaft_samples(e));
    return 1;
}
int L_Set_post_process_light_shaft_samples(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_light_shaft_samples(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_post_process_fog_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_post_process_get_fog_enabled(e));
    return 1;
}
int L_Set_post_process_fog_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_post_process_fog_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_post_process_get_fog_color(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_post_process_fog_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_color(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_post_process_fog_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_fog_density(e));
    return 1;
}
int L_Set_post_process_fog_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_density(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_fog_height_falloff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_fog_height_falloff(e));
    return 1;
}
int L_Set_post_process_fog_height_falloff(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_height_falloff(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_fog_height_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_fog_height_offset(e));
    return 1;
}
int L_Set_post_process_fog_height_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_height_offset(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_fog_start(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_fog_start(e));
    return 1;
}
int L_Set_post_process_fog_start(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_start(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_fog_end(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_fog_end(e));
    return 1;
}
int L_Set_post_process_fog_end(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_end(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_post_process_fog_steps(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_post_process_get_fog_steps(e));
    return 1;
}
int L_Set_post_process_fog_steps(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_steps(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_post_process_fog_sun_scatter(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_post_process_get_fog_sun_scatter(e));
    return 1;
}
int L_Set_post_process_fog_sun_scatter(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_set_fog_sun_scatter(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterPostProcessComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_post_process_enabled", L_Get_post_process_enabled},
        {"set_post_process_enabled", L_Set_post_process_enabled},
        {"get_post_process_bloom_enabled", L_Get_post_process_bloom_enabled},
        {"set_post_process_bloom_enabled", L_Set_post_process_bloom_enabled},
        {"get_post_process_bloom_threshold", L_Get_post_process_bloom_threshold},
        {"set_post_process_bloom_threshold", L_Set_post_process_bloom_threshold},
        {"get_post_process_bloom_intensity", L_Get_post_process_bloom_intensity},
        {"set_post_process_bloom_intensity", L_Set_post_process_bloom_intensity},
        {"get_post_process_bloom_knee", L_Get_post_process_bloom_knee},
        {"set_post_process_bloom_knee", L_Set_post_process_bloom_knee},
        {"get_post_process_bloom_mip_weight", L_Get_post_process_bloom_mip_weight},
        {"set_post_process_bloom_mip_weight", L_Set_post_process_bloom_mip_weight},
        {"get_post_process_color_grading_enabled", L_Get_post_process_color_grading_enabled},
        {"set_post_process_color_grading_enabled", L_Set_post_process_color_grading_enabled},
        {"get_post_process_exposure", L_Get_post_process_exposure},
        {"set_post_process_exposure", L_Set_post_process_exposure},
        {"get_post_process_gamma", L_Get_post_process_gamma},
        {"set_post_process_gamma", L_Set_post_process_gamma},
        {"get_post_process_ssao_enabled", L_Get_post_process_ssao_enabled},
        {"set_post_process_ssao_enabled", L_Set_post_process_ssao_enabled},
        {"get_post_process_ssao_radius", L_Get_post_process_ssao_radius},
        {"set_post_process_ssao_radius", L_Set_post_process_ssao_radius},
        {"get_post_process_ssao_bias", L_Get_post_process_ssao_bias},
        {"set_post_process_ssao_bias", L_Set_post_process_ssao_bias},
        {"get_post_process_ssao_sample_count", L_Get_post_process_ssao_sample_count},
        {"set_post_process_ssao_sample_count", L_Set_post_process_ssao_sample_count},
        {"get_post_process_ssao_power", L_Get_post_process_ssao_power},
        {"set_post_process_ssao_power", L_Set_post_process_ssao_power},
        {"get_post_process_ssao_intensity", L_Get_post_process_ssao_intensity},
        {"set_post_process_ssao_intensity", L_Set_post_process_ssao_intensity},
        {"get_post_process_auto_exposure_enabled", L_Get_post_process_auto_exposure_enabled},
        {"set_post_process_auto_exposure_enabled", L_Set_post_process_auto_exposure_enabled},
        {"get_post_process_exposure_min", L_Get_post_process_exposure_min},
        {"set_post_process_exposure_min", L_Set_post_process_exposure_min},
        {"get_post_process_exposure_max", L_Get_post_process_exposure_max},
        {"set_post_process_exposure_max", L_Set_post_process_exposure_max},
        {"get_post_process_adaptation_speed_up", L_Get_post_process_adaptation_speed_up},
        {"set_post_process_adaptation_speed_up", L_Set_post_process_adaptation_speed_up},
        {"get_post_process_adaptation_speed_down", L_Get_post_process_adaptation_speed_down},
        {"set_post_process_adaptation_speed_down", L_Set_post_process_adaptation_speed_down},
        {"get_post_process_exposure_compensation", L_Get_post_process_exposure_compensation},
        {"set_post_process_exposure_compensation", L_Set_post_process_exposure_compensation},
        {"get_post_process_color_lut_intensity", L_Get_post_process_color_lut_intensity},
        {"set_post_process_color_lut_intensity", L_Set_post_process_color_lut_intensity},
        {"get_post_process_vignette_enabled", L_Get_post_process_vignette_enabled},
        {"set_post_process_vignette_enabled", L_Set_post_process_vignette_enabled},
        {"get_post_process_vignette_intensity", L_Get_post_process_vignette_intensity},
        {"set_post_process_vignette_intensity", L_Set_post_process_vignette_intensity},
        {"get_post_process_vignette_radius", L_Get_post_process_vignette_radius},
        {"set_post_process_vignette_radius", L_Set_post_process_vignette_radius},
        {"get_post_process_vignette_softness", L_Get_post_process_vignette_softness},
        {"set_post_process_vignette_softness", L_Set_post_process_vignette_softness},
        {"get_post_process_film_grain_enabled", L_Get_post_process_film_grain_enabled},
        {"set_post_process_film_grain_enabled", L_Set_post_process_film_grain_enabled},
        {"get_post_process_film_grain_intensity", L_Get_post_process_film_grain_intensity},
        {"set_post_process_film_grain_intensity", L_Set_post_process_film_grain_intensity},
        {"get_post_process_film_grain_time_scale", L_Get_post_process_film_grain_time_scale},
        {"set_post_process_film_grain_time_scale", L_Set_post_process_film_grain_time_scale},
        {"get_post_process_fxaa_enabled", L_Get_post_process_fxaa_enabled},
        {"set_post_process_fxaa_enabled", L_Set_post_process_fxaa_enabled},
        {"get_post_process_taa_enabled", L_Get_post_process_taa_enabled},
        {"set_post_process_taa_enabled", L_Set_post_process_taa_enabled},
        {"get_post_process_taa_blend_factor", L_Get_post_process_taa_blend_factor},
        {"set_post_process_taa_blend_factor", L_Set_post_process_taa_blend_factor},
        {"get_post_process_contact_shadow_enabled", L_Get_post_process_contact_shadow_enabled},
        {"set_post_process_contact_shadow_enabled", L_Set_post_process_contact_shadow_enabled},
        {"get_post_process_contact_shadow_strength", L_Get_post_process_contact_shadow_strength},
        {"set_post_process_contact_shadow_strength", L_Set_post_process_contact_shadow_strength},
        {"get_post_process_contact_shadow_steps", L_Get_post_process_contact_shadow_steps},
        {"set_post_process_contact_shadow_steps", L_Set_post_process_contact_shadow_steps},
        {"get_post_process_contact_shadow_step_size", L_Get_post_process_contact_shadow_step_size},
        {"set_post_process_contact_shadow_step_size", L_Set_post_process_contact_shadow_step_size},
        {"get_post_process_dof_enabled", L_Get_post_process_dof_enabled},
        {"set_post_process_dof_enabled", L_Set_post_process_dof_enabled},
        {"get_post_process_dof_focus_distance", L_Get_post_process_dof_focus_distance},
        {"set_post_process_dof_focus_distance", L_Set_post_process_dof_focus_distance},
        {"get_post_process_dof_focus_range", L_Get_post_process_dof_focus_range},
        {"set_post_process_dof_focus_range", L_Set_post_process_dof_focus_range},
        {"get_post_process_dof_bokeh_radius", L_Get_post_process_dof_bokeh_radius},
        {"set_post_process_dof_bokeh_radius", L_Set_post_process_dof_bokeh_radius},
        {"get_post_process_motion_blur_enabled", L_Get_post_process_motion_blur_enabled},
        {"set_post_process_motion_blur_enabled", L_Set_post_process_motion_blur_enabled},
        {"get_post_process_motion_blur_intensity", L_Get_post_process_motion_blur_intensity},
        {"set_post_process_motion_blur_intensity", L_Set_post_process_motion_blur_intensity},
        {"get_post_process_motion_blur_samples", L_Get_post_process_motion_blur_samples},
        {"set_post_process_motion_blur_samples", L_Set_post_process_motion_blur_samples},
        {"get_post_process_ssr_enabled", L_Get_post_process_ssr_enabled},
        {"set_post_process_ssr_enabled", L_Set_post_process_ssr_enabled},
        {"get_post_process_ssr_max_distance", L_Get_post_process_ssr_max_distance},
        {"set_post_process_ssr_max_distance", L_Set_post_process_ssr_max_distance},
        {"get_post_process_ssr_thickness", L_Get_post_process_ssr_thickness},
        {"set_post_process_ssr_thickness", L_Set_post_process_ssr_thickness},
        {"get_post_process_ssr_step_size", L_Get_post_process_ssr_step_size},
        {"set_post_process_ssr_step_size", L_Set_post_process_ssr_step_size},
        {"get_post_process_ssr_max_steps", L_Get_post_process_ssr_max_steps},
        {"set_post_process_ssr_max_steps", L_Set_post_process_ssr_max_steps},
        {"get_post_process_ssr_fade_distance", L_Get_post_process_ssr_fade_distance},
        {"set_post_process_ssr_fade_distance", L_Set_post_process_ssr_fade_distance},
        {"get_post_process_ssr_max_roughness", L_Get_post_process_ssr_max_roughness},
        {"set_post_process_ssr_max_roughness", L_Set_post_process_ssr_max_roughness},
        {"get_post_process_outline_enabled", L_Get_post_process_outline_enabled},
        {"set_post_process_outline_enabled", L_Set_post_process_outline_enabled},
        {"get_post_process_outline_color", L_Get_post_process_outline_color},
        {"set_post_process_outline_color", L_Set_post_process_outline_color},
        {"get_post_process_outline_thickness", L_Get_post_process_outline_thickness},
        {"set_post_process_outline_thickness", L_Set_post_process_outline_thickness},
        {"get_post_process_outline_depth_threshold", L_Get_post_process_outline_depth_threshold},
        {"set_post_process_outline_depth_threshold", L_Set_post_process_outline_depth_threshold},
        {"get_post_process_outline_normal_threshold", L_Get_post_process_outline_normal_threshold},
        {"set_post_process_outline_normal_threshold", L_Set_post_process_outline_normal_threshold},
        {"get_post_process_light_shaft_enabled", L_Get_post_process_light_shaft_enabled},
        {"set_post_process_light_shaft_enabled", L_Set_post_process_light_shaft_enabled},
        {"get_post_process_light_shaft_color", L_Get_post_process_light_shaft_color},
        {"set_post_process_light_shaft_color", L_Set_post_process_light_shaft_color},
        {"get_post_process_light_shaft_density", L_Get_post_process_light_shaft_density},
        {"set_post_process_light_shaft_density", L_Set_post_process_light_shaft_density},
        {"get_post_process_light_shaft_weight", L_Get_post_process_light_shaft_weight},
        {"set_post_process_light_shaft_weight", L_Set_post_process_light_shaft_weight},
        {"get_post_process_light_shaft_decay", L_Get_post_process_light_shaft_decay},
        {"set_post_process_light_shaft_decay", L_Set_post_process_light_shaft_decay},
        {"get_post_process_light_shaft_exposure", L_Get_post_process_light_shaft_exposure},
        {"set_post_process_light_shaft_exposure", L_Set_post_process_light_shaft_exposure},
        {"get_post_process_light_shaft_intensity", L_Get_post_process_light_shaft_intensity},
        {"set_post_process_light_shaft_intensity", L_Set_post_process_light_shaft_intensity},
        {"get_post_process_light_shaft_samples", L_Get_post_process_light_shaft_samples},
        {"set_post_process_light_shaft_samples", L_Set_post_process_light_shaft_samples},
        {"get_post_process_fog_enabled", L_Get_post_process_fog_enabled},
        {"set_post_process_fog_enabled", L_Set_post_process_fog_enabled},
        {"get_post_process_fog_color", L_Get_post_process_fog_color},
        {"set_post_process_fog_color", L_Set_post_process_fog_color},
        {"get_post_process_fog_density", L_Get_post_process_fog_density},
        {"set_post_process_fog_density", L_Set_post_process_fog_density},
        {"get_post_process_fog_height_falloff", L_Get_post_process_fog_height_falloff},
        {"set_post_process_fog_height_falloff", L_Set_post_process_fog_height_falloff},
        {"get_post_process_fog_height_offset", L_Get_post_process_fog_height_offset},
        {"set_post_process_fog_height_offset", L_Set_post_process_fog_height_offset},
        {"get_post_process_fog_start", L_Get_post_process_fog_start},
        {"set_post_process_fog_start", L_Set_post_process_fog_start},
        {"get_post_process_fog_end", L_Get_post_process_fog_end},
        {"set_post_process_fog_end", L_Set_post_process_fog_end},
        {"get_post_process_fog_steps", L_Get_post_process_fog_steps},
        {"set_post_process_fog_steps", L_Set_post_process_fog_steps},
        {"get_post_process_fog_sun_scatter", L_Get_post_process_fog_sun_scatter},
        {"set_post_process_fog_sun_scatter", L_Set_post_process_fog_sun_scatter},
    });
}

} // namespace dse::runtime::lua_binding
