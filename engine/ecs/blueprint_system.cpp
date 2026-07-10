/**
 * @file blueprint_system.cpp
 * @brief 运行时蓝图 Tick 系统 —— 遍历 BlueprintComponent 实体并驱动其字节码 VM。
 *
 * 设计：
 *  - 编译缓存按 .dbp 路径共享（同一蓝图的多个实例共用一份字节码）；
 *  - 每实体变量状态独立（BlueprintInstance），按 entity id 存储于系统内部；
 *  - LOD：tick_interval > 0 时按间隔累加驱动，复用 AI LOD 思路；
 *  - ECS 读写通过 IBlueprintEcsBridge 落到 TransformComponent（字段 0 = position）。
 *
 * 蓝图编译/编辑在编辑器侧（dse::editor::bp），运行时执行在此，二者共享 .dbp 格式。
 */

#include "engine/ecs/blueprint_system.h"

#include <string>
#include <unordered_map>
#include <cstdlib>

#include "engine/ecs/blueprint_component.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/core/service_locator.h"
#include "engine/base/debug.h"
#include "engine/scripting/blueprint/blueprint_compiler.h"
#include "engine/scripting/blueprint/blueprint_vm.h"

namespace dse {

int BlueprintSystem::s_last_tick_count = 0;

namespace {

// TransformComponent.position 落到 field 0；其余字段暂无映射（返回/写入 0）。
class WorldEcsBridge : public dse::bp::IBlueprintEcsBridge {
public:
    explicit WorldEcsBridge(entt::registry& reg) : reg_(reg) {}

    void GetVec3(uint32_t entity, int field, float out[3]) override {
        out[0] = out[1] = out[2] = 0.0f;
        auto e = static_cast<entt::entity>(entity);
        if (field == 0 && reg_.valid(e)) {
            if (auto* t = reg_.try_get<TransformComponent>(e)) {
                out[0] = t->position.x; out[1] = t->position.y; out[2] = t->position.z;
            }
        }
    }
    void SetVec3(uint32_t entity, int field, const float in[3]) override {
        auto e = static_cast<entt::entity>(entity);
        if (field == 0 && reg_.valid(e)) {
            auto& t = reg_.get_or_emplace<TransformComponent>(e);
            t.position.x = in[0]; t.position.y = in[1]; t.position.z = in[2];
            t.dirty = true;
        }
    }
    float GetFloat(uint32_t entity, int field) override {
        auto e = static_cast<entt::entity>(entity);
        if (reg_.valid(e)) {
            if (auto* t = reg_.try_get<TransformComponent>(e)) {
                switch (field) { case 0: return t->position.x; case 1: return t->position.y; case 2: return t->position.z; default: break; }
            }
        }
        return 0.0f;
    }
    void SetFloat(uint32_t entity, int field, float value) override {
        auto e = static_cast<entt::entity>(entity);
        if (reg_.valid(e)) {
            auto& t = reg_.get_or_emplace<TransformComponent>(e);
            switch (field) { case 0: t.position.x = value; break; case 1: t.position.y = value; break; case 2: t.position.z = value; break; default: return; }
            t.dirty = true;
        }
    }
private:
    entt::registry& reg_;
};

struct BlueprintRuntimeState {
    std::unordered_map<std::string, dse::bp::CompiledBlueprint> compiled_cache; // by .dbp path
    std::unordered_map<uint32_t, dse::bp::BlueprintInstance> instances;         // by entity id
    bool externs_registered = false;
};

BlueprintRuntimeState& State() {
    static BlueprintRuntimeState s;
    return s;
}

// 首次编译某节点名为 extern 的图前，externs 必须已注册（编译期按名解析索引）。
void RegisterDefaultExterns() {
    auto& vm = dse::bp::BlueprintVM::Get();
    vm.RegisterExtern("Log", [](const std::vector<dse::bp::BpValue>& args) {
        if (!args.empty()) {
            const auto& v = args[0];
            if (v.type == dse::bp::BpValue::Type::String) DEBUG_LOG_INFO("[Blueprint] %s", v.str.c_str());
            else DEBUG_LOG_INFO("[Blueprint] %f", v.AsFloat());
        }
        return dse::bp::BpValue();
    });
    vm.RegisterExtern("Random", [](const std::vector<dse::bp::BpValue>&) {
        return dse::bp::BpValue::Float(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
    });
}

const dse::bp::CompiledBlueprint* GetOrCompile(const std::string& path) {
    auto& st = State();
    auto it = st.compiled_cache.find(path);
    if (it != st.compiled_cache.end()) return &it->second;

    dse::bp::BlueprintAsset asset;
    if (!dse::bp::LoadBlueprintAsset(asset, path)) {
        DEBUG_LOG_WARN("[Blueprint] 加载失败，跳过: %s", path.c_str());
        // 缓存空蓝图以避免每帧重试解析
        auto& empty = st.compiled_cache[path];
        return &empty;
    }
    auto compiled = dse::bp::CompileToByteCode(asset);
    auto& stored = st.compiled_cache[path];
    stored = std::move(compiled);
    return &stored;
}

} // namespace

void BlueprintSystem::Init() {
    auto& st = State();
    if (!st.externs_registered) {
        RegisterDefaultExterns();
        st.externs_registered = true;
    }
    st.compiled_cache.clear();
    st.instances.clear();
    s_last_tick_count = 0;
}

void BlueprintSystem::Update(float dt) {
    auto& st = State();
    if (!st.externs_registered) {
        RegisterDefaultExterns();
        st.externs_registered = true;
    }

    auto* world = dse::core::ServiceLocator::Instance().Get<World>();
    if (!world) return;

    auto& reg = world->registry();
    WorldEcsBridge bridge(reg);
    auto& vm = dse::bp::BlueprintVM::Get();

    int ticked = 0;
    auto view = reg.view<BlueprintComponent>();
    for (auto entity : view) {
        auto& bc = view.get<BlueprintComponent>(entity);
        if (!bc.enabled || bc.blueprint_asset_path.empty()) continue;

        const dse::bp::CompiledBlueprint* compiled = GetOrCompile(bc.blueprint_asset_path);
        if (!compiled || compiled->functions.empty()) continue;

        uint32_t eid = static_cast<uint32_t>(entity);
        auto& inst = st.instances[eid];
        if (inst.blueprint != compiled) {
            inst.blueprint = compiled;
            inst.initialized = false;
        }

        if (!inst.initialized) {
            vm.RunInit(inst, eid, &bridge);
            bc.initialized = true;
        }

        // LOD: tick_interval > 0 时按固定间隔驱动
        float step = dt;
        if (bc.tick_interval > 0.0f) {
            bc.time_accumulator += dt;
            if (bc.time_accumulator < bc.tick_interval) continue;
            step = bc.time_accumulator;
            bc.time_accumulator = 0.0f;
        }

        vm.RunUpdate(inst, eid, step, &bridge);
        ++ticked;
    }

    s_last_tick_count = ticked;
}

int BlueprintSystem::GetLastTickCount() {
    return s_last_tick_count;
}

void BlueprintSystem::HotReload(const char* asset_path) {
    if (!asset_path) return;
    auto& st = State();
    std::string path = asset_path;

    // 丢弃旧编译缓存 —— 缓存对象随之析构，现存实例持有的旧指针失效。
    // 无需在此触碰实例：下帧 Update 中 GetOrCompile 返回新指针，
    // 与实例保存的旧指针不等，自动触发重新 on_init（不解引用旧指针）。
    st.compiled_cache.erase(path);
    GetOrCompile(path);
}

} // namespace dse
