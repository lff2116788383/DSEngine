#include "engine/render/pipeline/render_pipeline_profile.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/atmosphere_sky_pass.h"
#include "engine/render/meshlet/meshlet_render_pass.h"
#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY
#include "engine/render/virtual_geometry/virtual_geometry_pass.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unordered_set>

#ifdef DSE_ENABLE_LUA
extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}
#endif

namespace dse {
namespace render {
namespace {

std::string NormalizeName(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        } else if (ch == '-' || ch == ' ' || ch == '.') {
            ch = '_';
        }
    }
    return value;
}

template <typename PassT>
void RegisterBuiltin(RenderPipelineRegistry& registry, RenderPassMetadata metadata) {
    const std::string name = metadata.name;
    registry.Register(std::move(metadata), [name](RenderPassContext& ctx) -> std::unique_ptr<IRenderPass> {
        (void)name;
        return std::make_unique<PassT>(ctx);
    });
}

RenderPipelinePassConfig Pass(std::string name, bool enabled = true) {
    RenderPipelinePassConfig config;
    config.name = std::move(name);
    config.enabled = enabled;
    return config;
}

std::vector<RenderPipelinePassConfig> DefaultPassList() {
    return {
        Pass("pre_z"),
        Pass("hiz_build"),
        Pass("hiz_cull"),
        Pass("csm_shadow"),
        Pass("spot_shadow"),
        Pass("point_shadow"),
        Pass("gpu_cull"),
        Pass("meshlet_cull"),
#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY
        Pass("vg_cull"),
        Pass("vg_sw_raster"),
#endif
        Pass("rsm"),
        Pass("ddgi_update"),
        Pass("forward_scene"),
        Pass("meshlet_draw"),
#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY
        Pass("vg_resolve"),
#endif
        Pass("atmosphere_sky"),
        Pass("wboit"),
        Pass("water"),
        Pass("bloom"),
        Pass("ssao"),
        Pass("contact_shadow"),
        Pass("auto_exposure"),
        Pass("motion_vector"),
        Pass("ssr"),
        Pass("outline"),
        Pass("light_shaft"),
        Pass("volumetric_fog"),
        Pass("decal"),
        Pass("ui"),
        Pass("composite"),
        Pass("dof"),
        Pass("motion_blur"),
        Pass("fxaa"),
        Pass("taa"),
        Pass("present"),
    };
}

void SetPassEnabled(RenderPipelineProfile& profile, const std::string& name, bool enabled) {
    for (auto& pass : profile.passes) {
        if (NormalizeName(pass.name) == NormalizeName(name)) {
            pass.enabled = enabled;
            return;
        }
    }
    profile.passes.push_back(Pass(name, enabled));
}

std::filesystem::path FindProfilePath(const std::string& selector, const std::string& data_root) {
    if (selector.empty()) return {};
    std::filesystem::path direct(selector);
    std::error_code ec;
    if (std::filesystem::exists(direct, ec)) return direct;

    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("samples") / "lua" / "pipelines" / (selector + ".lua"),
        std::filesystem::path(data_root) / "pipelines" / (selector + ".lua"),
        std::filesystem::path(data_root) / "render" / "pipelines" / (selector + ".lua"),
        std::filesystem::path("data") / "pipelines" / (selector + ".lua"),
        std::filesystem::path("pipelines") / (selector + ".lua"),
        std::filesystem::path("script") / "pipelines" / (selector + ".lua"),
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec)) return candidate;
    }
    return {};
}

#ifdef DSE_ENABLE_LUA
bool ReadBoolField(lua_State* L, int table_index, const char* field, bool fallback) {
    lua_getfield(L, table_index, field);
    bool value = fallback;
    if (lua_isboolean(L, -1)) value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

std::string ReadStringField(lua_State* L, int table_index, const char* field, const std::string& fallback) {
    lua_getfield(L, table_index, field);
    std::string value = fallback;
    if (lua_isstring(L, -1)) value = lua_tostring(L, -1);
    lua_pop(L, 1);
    return value;
}

PipelineValue ReadPipelineValue(lua_State* L, int index) {
    if (lua_isboolean(L, index)) return lua_toboolean(L, index) != 0;
    if (lua_isinteger(L, index)) return static_cast<int>(lua_tointeger(L, index));
    if (lua_isnumber(L, index)) return static_cast<double>(lua_tonumber(L, index));
    if (lua_isstring(L, index)) return std::string(lua_tostring(L, index));
    return std::string();
}

void ReadParams(lua_State* L, int table_index, RenderPipelinePassConfig& config) {
    table_index = lua_absindex(L, table_index);
    lua_pushnil(L);
    while (lua_next(L, table_index) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const std::string key = lua_tostring(L, -2);
            if (key != "name" && key != "enabled") {
                config.params[key] = ReadPipelineValue(L, -1);
            }
        }
        lua_pop(L, 1);
    }
}

bool LoadLuaProfileFile(const std::filesystem::path& path,
                        const std::string& backend_name,
                        RenderPipelineProfile& profile,
                        std::string& error_message) {
    lua_State* L = luaL_newstate();
    if (!L) {
        error_message = "failed to create Lua state";
        return false;
    }

    lua_pushstring(L, backend_name.c_str());
    lua_setglobal(L, "backend");

    const std::string path_string = path.generic_string();
    if (luaL_loadfile(L, path_string.c_str()) != LUA_OK) {
        error_message = lua_tostring(L, -1) ? lua_tostring(L, -1) : "failed to load Lua profile";
        lua_close(L);
        return false;
    }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        error_message = lua_tostring(L, -1) ? lua_tostring(L, -1) : "failed to execute Lua profile";
        lua_close(L);
        return false;
    }
    if (!lua_istable(L, -1)) {
        error_message = "Lua profile must return a table";
        lua_close(L);
        return false;
    }

    const int root = lua_absindex(L, -1);
    profile = RenderPipelineProfile{};
    profile.loaded_from_lua = true;
    profile.source_path = path_string;
    profile.name = ReadStringField(L, root, "name", path.stem().string());

    lua_getfield(L, root, "settings");
    if (lua_istable(L, -1)) {
        const int settings = lua_absindex(L, -1);
        profile.settings.gpu_driven = ReadBoolField(L, settings, "gpu_driven", profile.settings.gpu_driven);
        profile.settings.shadows = ReadBoolField(L, settings, "shadows", profile.settings.shadows);
        profile.settings.shadow_quality = ReadStringField(L, settings, "shadow_quality", profile.settings.shadow_quality);
        profile.settings.postprocess_quality = ReadStringField(L, settings, "postprocess_quality", profile.settings.postprocess_quality);
    }
    lua_pop(L, 1);

    lua_getfield(L, root, "passes");
    if (!lua_istable(L, -1)) {
        error_message = "Lua profile must define a passes table";
        lua_close(L);
        return false;
    }
    const int passes = lua_absindex(L, -1);
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, passes));
    for (lua_Integer i = 1; i <= count; ++i) {
        lua_rawgeti(L, passes, i);
        if (lua_istable(L, -1)) {
            const int pass_table = lua_absindex(L, -1);
            RenderPipelinePassConfig pass;
            pass.name = ReadStringField(L, pass_table, "name", "");
            pass.enabled = ReadBoolField(L, pass_table, "enabled", true);
            ReadParams(L, pass_table, pass);
            if (!pass.name.empty()) profile.passes.push_back(std::move(pass));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
    lua_close(L);
    return true;
}
#else
bool LoadLuaProfileFile(const std::filesystem::path&,
                        const std::string&,
                        RenderPipelineProfile&,
                        std::string& error_message) {
    error_message = "DSE_ENABLE_LUA is OFF";
    return false;
}
#endif

bool IsNumericValue(const PipelineValue& value) {
    return std::holds_alternative<int>(value) || std::holds_alternative<double>(value);
}

} // namespace

void RenderPipelineRegistry::Register(RenderPassMetadata metadata, PassFactory factory) {
    const std::string canonical = NormalizeName(metadata.name);
    metadata.name = canonical;
    if (metadata_.find(canonical) == metadata_.end()) {
        order_.push_back(canonical);
    }
    metadata_[canonical] = std::move(metadata);
    factories_[canonical] = std::move(factory);
}

void RenderPipelineRegistry::RegisterAlias(std::string alias, std::string canonical_name) {
    aliases_[NormalizeName(std::move(alias))] = NormalizeName(std::move(canonical_name));
}

std::string RenderPipelineRegistry::ResolveName(const std::string& name) const {
    const std::string normalized = NormalizeName(name);
    auto alias_it = aliases_.find(normalized);
    if (alias_it != aliases_.end()) return alias_it->second;
    return normalized;
}

const RenderPassMetadata* RenderPipelineRegistry::FindMetadata(const std::string& name) const {
    const std::string canonical = ResolveName(name);
    auto it = metadata_.find(canonical);
    return it != metadata_.end() ? &it->second : nullptr;
}

bool RenderPipelineRegistry::Contains(const std::string& name) const {
    return FindMetadata(name) != nullptr;
}

std::unique_ptr<IRenderPass> RenderPipelineRegistry::Create(const std::string& name, RenderPassContext& context) const {
    const std::string canonical = ResolveName(name);
    auto it = factories_.find(canonical);
    return it != factories_.end() ? it->second(context) : nullptr;
}

const RenderPipelineRegistry& BuiltinRenderPipelineRegistry() {
    static const RenderPipelineRegistry registry = [] {
        RenderPipelineRegistry r;
        RegisterBuiltin<PreZPass>(r, {"pre_z", true});
        RegisterBuiltin<HiZBuildPass>(r, {"hiz_build", false, false, true, false, true});
        RegisterBuiltin<HiZCullPass>(r, {"hiz_cull", false, false, true, false, true, true});
        RegisterBuiltin<CSMShadowPass>(r, {"csm_shadow"});
        RegisterBuiltin<SpotShadowPass>(r, {"spot_shadow"});
        RegisterBuiltin<PointShadowPass>(r, {"point_shadow"});
        RegisterBuiltin<GPUCullPass>(r, {"gpu_cull", false, false, false, true, true, true});
        RegisterBuiltin<MeshletCullRenderPass>(r, {"meshlet_cull", false, false, true, false, true, true});
        RegisterBuiltin<MeshletDrawRenderPass>(r, {"meshlet_draw"});
#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY
        RegisterBuiltin<vg::VGCullPass>(r, {"vg_cull", false, false, true, false, true, true});
        RegisterBuiltin<vg::VGRasterPass>(r, {"vg_sw_raster", false, false, false, false, true});
        RegisterBuiltin<vg::VGResolvePass>(r, {"vg_resolve"});
#endif
        RegisterBuiltin<RSMRenderPass>(r, {"rsm"});
        RegisterBuiltin<DDGIUpdatePass>(r, {"ddgi_update"});
        RegisterBuiltin<AtmosphereSkyPass>(r, {"atmosphere_sky"});
        RegisterBuiltin<ForwardScenePass>(r, {"forward_scene", true});
        RegisterBuiltin<WBOITPass>(r, {"wboit"});
        RegisterBuiltin<WaterPass>(r, {"water"});
        RegisterBuiltin<BloomPass>(r, {"bloom"});
        RegisterBuiltin<SSAOPass>(r, {"ssao"});
        RegisterBuiltin<SSSBlurPass>(r, {"sss_blur"});
        RegisterBuiltin<ContactShadowPass>(r, {"contact_shadow"});
        RegisterBuiltin<AutoExposurePass>(r, {"auto_exposure"});
        RegisterBuiltin<MotionVectorPass>(r, {"motion_vector"});
        RegisterBuiltin<SSRPass>(r, {"ssr"});
        RegisterBuiltin<OutlinePass>(r, {"outline"});
        RegisterBuiltin<LightShaftPass>(r, {"light_shaft"});
        RegisterBuiltin<VolumetricFogPass>(r, {"volumetric_fog"});
        RegisterBuiltin<VolumetricCloudPass>(r, {"volumetric_cloud"});
        RegisterBuiltin<DecalPass>(r, {"decal"});
        RegisterBuiltin<WeatherPass>(r, {"weather_particle"});
        RegisterBuiltin<UIPass>(r, {"ui", true});
        RegisterBuiltin<CompositePass>(r, {"composite", true});
        RegisterBuiltin<DOFPass>(r, {"dof"});
        RegisterBuiltin<MotionBlurPass>(r, {"motion_blur"});
        RegisterBuiltin<FXAAPass>(r, {"fxaa"});
        RegisterBuiltin<TAAPass>(r, {"taa"});
        RegisterBuiltin<PresentPass>(r, {"present", false, true});

        r.RegisterAlias("prez", "pre_z");
        r.RegisterAlias("prez_pass", "pre_z");
        r.RegisterAlias("shadow_pass", "csm_shadow");
        r.RegisterAlias("spot_shadow_pass", "spot_shadow");
        r.RegisterAlias("point_shadow_pass", "point_shadow");
        r.RegisterAlias("scene", "forward_scene");
        r.RegisterAlias("scene_pass", "forward_scene");
        r.RegisterAlias("post_process_pass", "bloom");
        r.RegisterAlias("ssao_pass", "ssao");
        r.RegisterAlias("contact_shadow_pass", "contact_shadow");
        r.RegisterAlias("fxaa_pass", "fxaa");
        r.RegisterAlias("taa_pass", "taa");
        r.RegisterAlias("ui_pass", "ui");
        r.RegisterAlias("composite_pass", "composite");
        r.RegisterAlias("present_pass", "present");
        r.RegisterAlias("gpu_cull_pass", "gpu_cull");
        r.RegisterAlias("hiz_build_pass", "hiz_build");
        r.RegisterAlias("hiz_cull_pass", "hiz_cull");
        return r;
    }();
    return registry;
}

RenderPipelineProfile MakeForwardPlusDefaultProfile() {
    RenderPipelineProfile profile;
    profile.name = "ForwardPlusDefault";
    profile.passes = DefaultPassList();
    return profile;
}

RenderPipelineProfile MakeForwardPlusLiteProfile() {
    RenderPipelineProfile profile = MakeForwardPlusDefaultProfile();
    profile.name = "ForwardPlusLite";
    profile.settings.shadow_quality = "medium";
    profile.settings.postprocess_quality = "lite";
    SetPassEnabled(profile, "spot_shadow", false);
    SetPassEnabled(profile, "point_shadow", false);
    SetPassEnabled(profile, "ssao", false);
    SetPassEnabled(profile, "contact_shadow", false);
    SetPassEnabled(profile, "taa", false);
    SetPassEnabled(profile, "dof", false);
    SetPassEnabled(profile, "motion_blur", false);
    SetPassEnabled(profile, "ssr", false);
    SetPassEnabled(profile, "light_shaft", false);
    SetPassEnabled(profile, "volumetric_fog", false);
    return profile;
}

RenderPipelineProfile MakeForward2DProfile() {
    // 面向 2D-first 平台（如 Web/WebGL2）的最小前向管线：不跑延迟着色、WBOIT、
    // HDR 后处理链，仅 清屏深度 → 前向场景（精灵批次）→ UI → 合成 → 呈现。
    RenderPipelineProfile profile;
    profile.name = "Forward2D";
    profile.settings.gpu_driven = false;
    profile.settings.shadows = false;
    profile.settings.shadow_quality = "off";
    profile.settings.postprocess_quality = "none";
    profile.passes = {
        Pass("pre_z"),
        Pass("forward_scene"),
        Pass("ui"),
        Pass("composite"),
        Pass("present"),
    };
    return profile;
}

RenderPipelineProfile MakeForward3DProfile() {
    // M5 best-effort 3D forward for capability-limited platforms (Web/WebGL2):
    // depth pre-pass → 阴影（CSM 方向光 / 聚光 / 点光逐面立方体）→ forward scene
    //（UBO PBR 着色 + 阴影采样）→ SSAO/bloom/auto-exposure（全屏片元）→ UI →
    // composite（tonemap + bloom/ssao/曝光合成）→ FXAA → present。
    // 阴影与后处理链均走 2D 纹理 + 全屏片元 + ESSL300 变体（WebGL2 原生支持，
    // bloom 在无 compute 时自动回退到全屏 quad 下/上采样），不需 Compute/SSBO/
    // MultiDrawIndirect。仍不跑 GPU-driven 剔除与延迟着色。
    RenderPipelineProfile profile;
    profile.name = "Forward3D";
    profile.settings.gpu_driven = false;
    profile.settings.shadows = true;
    profile.settings.shadow_quality = "low";
    profile.settings.postprocess_quality = "low";
    profile.passes = {
        Pass("pre_z"),
        Pass("csm_shadow"),
        Pass("spot_shadow"),
        Pass("point_shadow"),
        Pass("forward_scene"),
        Pass("ssao"),
        Pass("bloom"),
        Pass("auto_exposure"),
        Pass("ui"),
        Pass("composite"),
        Pass("fxaa"),
        Pass("present"),
    };
    return profile;
}

RenderPipelineProfile MakeDebugDepthProfile() {
    RenderPipelineProfile profile;
    profile.name = "DebugDepth";
    profile.settings.postprocess_quality = "debug";
    profile.passes = {
        Pass("pre_z"),
        Pass("forward_scene"),
        Pass("ui"),
        Pass("composite"),
        Pass("present"),
    };
    return profile;
}

RenderPipelineLoadResult ResolveRenderPipelineProfileFromEnvironment(const std::string& backend_name,
                                                                    const std::string& data_root) {
    RenderPipelineLoadResult result;
    result.profile = MakeForwardPlusDefaultProfile();

    const char* path_env = std::getenv("DSE_RENDER_PIPELINE_PROFILE_PATH");
    const char* selector_env = std::getenv("DSE_RENDER_PIPELINE_PROFILE");
    const std::string selector = selector_env ? selector_env : "";
    const std::string explicit_path = path_env ? path_env : "";

    if (explicit_path.empty() && selector.empty()) {
        result.message = "using built-in ForwardPlusDefault";
        return result;
    }

    const std::string normalized_selector = NormalizeName(selector);
    if (explicit_path.empty()) {
        if (normalized_selector == "default" || normalized_selector == "forward_plus" ||
            normalized_selector == "forward_plus_default") {
            result.message = "using built-in ForwardPlusDefault";
            return result;
        }
        if (normalized_selector == "lite" || normalized_selector == "forward_plus_lite") {
            result.profile = MakeForwardPlusLiteProfile();
            result.message = "using built-in ForwardPlusLite";
            return result;
        }
        if (normalized_selector == "forward_2d" || normalized_selector == "2d" ||
            normalized_selector == "web2d") {
            result.profile = MakeForward2DProfile();
            result.message = "using built-in Forward2D";
            return result;
        }
        if (normalized_selector == "forward_3d" || normalized_selector == "3d" ||
            normalized_selector == "web3d") {
            result.profile = MakeForward3DProfile();
            result.message = "using built-in Forward3D";
            return result;
        }
        if (normalized_selector == "debug_depth") {
            result.profile = MakeDebugDepthProfile();
            result.message = "using built-in DebugDepth";
            return result;
        }
    }

    const std::filesystem::path profile_path = !explicit_path.empty()
        ? std::filesystem::path(explicit_path)
        : FindProfilePath(selector, data_root);
    if (!profile_path.empty()) {
        std::string error;
        RenderPipelineProfile lua_profile;
        if (LoadLuaProfileFile(profile_path, backend_name, lua_profile, error)) {
            result.profile = std::move(lua_profile);
            result.message = "loaded Lua profile: " + profile_path.generic_string();
            return result;
        }
        result.used_fallback = true;
        result.message = "failed to load Lua profile '" + profile_path.generic_string() + "': " + error + "; using ForwardPlusDefault";
        return result;
    }

    result.used_fallback = true;
    result.message = "unknown render pipeline profile '" + selector + "'; using ForwardPlusDefault";
    return result;
}

const char* RenderPassCapabilityPruneReason(const RenderPassMetadata& metadata,
                                            const RenderPipelineValidationContext& context) {
    if (metadata.runtime_only && context.editor_mode) return "editor";
    if (metadata.requires_hiz && !context.hiz_available) return "no_hiz";
    if (metadata.requires_gpu_driven && !context.gpu_driven_supported) return "no_gpu_driven";
    if (metadata.requires_compute && !context.compute_supported) return "no_compute";
    if (metadata.requires_ssbo && !context.ssbo_supported) return "no_ssbo";
    if (metadata.requires_mrt && context.max_color_attachments < 4) return "no_mrt";
    return nullptr;
}

bool ValidateRenderPipelineProfile(const RenderPipelineProfile& profile,
                                   const RenderPipelineRegistry& registry,
                                   const RenderPipelineValidationContext& context,
                                   std::string& error_message) {
    std::unordered_set<std::string> seen;
    std::unordered_set<std::string> enabled;
    for (const auto& pass : profile.passes) {
        const std::string canonical = registry.ResolveName(pass.name);
        if (!registry.Contains(canonical)) {
            error_message = "unknown pass: " + pass.name;
            return false;
        }
        if (!seen.insert(canonical).second) {
            error_message = "duplicate pass: " + pass.name;
            return false;
        }
        if (pass.enabled) enabled.insert(canonical);
        for (const auto& param : pass.params) {
            if (canonical == "bloom" &&
                (param.first == "intensity" || param.first == "threshold") &&
                !IsNumericValue(param.second)) {
                error_message = "bloom." + param.first + " must be numeric";
                return false;
            }
        }
    }

    for (const std::string& name : {"pre_z", "forward_scene", "composite"}) {
        if (enabled.find(name) == enabled.end()) {
            error_message = "required pass is disabled or missing: " + name;
            return false;
        }
    }
    if (!context.editor_mode && enabled.find("present") == enabled.end()) {
        error_message = "runtime profile must enable present pass";
        return false;
    }
    return true;
}

bool IsRenderPipelinePassEnabled(const RenderPipelineProfile& profile,
                                 const RenderPipelineRegistry& registry,
                                 const std::string& pass_name) {
    const std::string canonical_target = registry.ResolveName(pass_name);
    for (const auto& pass : profile.passes) {
        if (registry.ResolveName(pass.name) == canonical_target) {
            return pass.enabled;
        }
    }
    return false;
}

const PipelineValue* FindRenderPipelinePassParam(const RenderPipelineProfile& profile,
                                                 const RenderPipelineRegistry& registry,
                                                 const std::string& pass_name,
                                                 const std::string& param_name) {
    const std::string canonical_target = registry.ResolveName(pass_name);
    for (const auto& pass : profile.passes) {
        if (registry.ResolveName(pass.name) != canonical_target) continue;
        auto it = pass.params.find(param_name);
        return it != pass.params.end() ? &it->second : nullptr;
    }
    return nullptr;
}

std::string DumpRenderPipelineProfile(const RenderPipelineProfile& profile,
                                      const RenderPipelineRegistry& registry,
                                      const RenderPipelineValidationContext& context) {
    std::ostringstream oss;
    oss << "RenderPipelineProfile " << profile.name;
    if (!profile.source_path.empty()) oss << " source=" << profile.source_path;
    oss << " settings{gpu_driven=" << (profile.settings.gpu_driven ? "true" : "false")
        << ", shadows=" << (profile.settings.shadows ? "true" : "false")
        << ", shadow_quality=" << profile.settings.shadow_quality
        << ", postprocess_quality=" << profile.settings.postprocess_quality << "}";
    int index = 0;
    for (const auto& pass : profile.passes) {
        const std::string canonical = registry.ResolveName(pass.name);
        const RenderPassMetadata* metadata = registry.FindMetadata(canonical);
        std::string state = pass.enabled ? "enabled" : "disabled";
        if (pass.enabled && metadata) {
            if (const char* reason = RenderPassCapabilityPruneReason(*metadata, context))
                state = std::string("skipped(") + reason + ")";
        }
        oss << "\n  " << index++ << ": " << canonical << " [" << state << "]";
    }
    return oss.str();
}

} // namespace render
} // namespace dse
