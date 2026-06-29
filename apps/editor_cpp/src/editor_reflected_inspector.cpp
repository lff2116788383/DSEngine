#include "editor_reflected_inspector.h"

#include "editor_context.h"
#include "editor_shortcuts.h"   // GetUndoRedoManager
#include "editor_undo.h"        // LambdaCommand

#include "engine/reflect/reflect.h"

#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>

#include <climits>
#include <cstring>
#include <memory>
#include <string>
#include <variant>

namespace dse::editor {

namespace {

using dse::reflect::FieldInfo;
using dse::reflect::FieldType;
using dse::reflect::TypeInfo;

/// 字段值快照（用于撤销）。覆盖反射核心支持的全部 FieldType（枚举以 long long 存底层整数）。
using FieldValue = std::variant<bool, int, unsigned int, float, double,
                                glm::vec2, glm::vec3, glm::vec4, std::string, long long>;

FieldValue ReadField(const FieldInfo& f, void* inst) {
    void* p = f.get(inst);
    switch (f.type) {
        case FieldType::Bool:   return *static_cast<bool*>(p);
        case FieldType::Int:    return *static_cast<int*>(p);
        case FieldType::UInt:   return *static_cast<unsigned int*>(p);
        case FieldType::Float:  return *static_cast<float*>(p);
        case FieldType::Double: return *static_cast<double*>(p);
        case FieldType::Vec2:   return *static_cast<glm::vec2*>(p);
        case FieldType::Vec3:   return *static_cast<glm::vec3*>(p);
        case FieldType::Vec4:   return *static_cast<glm::vec4*>(p);
        case FieldType::String: return *static_cast<std::string*>(p);
        case FieldType::Enum:   return f.get_enum ? f.get_enum(inst) : 0LL;
        case FieldType::Struct: break;  // 嵌套结构按子字段单独快照，不在此整体处理
    }
    return FieldValue{};
}

void WriteField(const FieldInfo& f, void* inst, const FieldValue& v) {
    void* p = f.get(inst);
    switch (f.type) {
        case FieldType::Bool:   *static_cast<bool*>(p) = std::get<bool>(v); break;
        case FieldType::Int:    *static_cast<int*>(p) = std::get<int>(v); break;
        case FieldType::UInt:   *static_cast<unsigned int*>(p) = std::get<unsigned int>(v); break;
        case FieldType::Float:  *static_cast<float*>(p) = std::get<float>(v); break;
        case FieldType::Double: *static_cast<double*>(p) = std::get<double>(v); break;
        case FieldType::Vec2:   *static_cast<glm::vec2*>(p) = std::get<glm::vec2>(v); break;
        case FieldType::Vec3:   *static_cast<glm::vec3*>(p) = std::get<glm::vec3>(v); break;
        case FieldType::Vec4:   *static_cast<glm::vec4*>(p) = std::get<glm::vec4>(v); break;
        case FieldType::String: *static_cast<std::string*>(p) = std::get<std::string>(v); break;
        case FieldType::Enum:   if (f.set_enum) f.set_enum(inst, std::get<long long>(v)); break;
        case FieldType::Struct: break;
    }
}

// 撤销快照：ImGui 同一时刻只有一个 active item，故单组静态状态即可。
FieldValue       s_undo_old;
const TypeInfo*  s_undo_type = nullptr;
std::size_t      s_undo_field = static_cast<std::size_t>(-1);

bool DrawWidget(const char* id, const FieldInfo& f, void* inst) {
    void* p = f.get(inst);
    const FieldType t = f.type;
    const auto& a = f.attr;
    const float speed = a.step > 0.0 ? static_cast<float>(a.step) : 0.05f;
    bool changed = false;

    switch (t) {
        case FieldType::Bool:
            changed = ImGui::Checkbox(id, static_cast<bool*>(p));
            break;
        case FieldType::Int: {
            int* v = static_cast<int*>(p);
            if (a.has_range)
                changed = ImGui::SliderInt(id, v, static_cast<int>(a.min_value), static_cast<int>(a.max_value));
            else
                changed = ImGui::DragInt(id, v, a.step > 0.0 ? static_cast<float>(a.step) : 1.0f);
            break;
        }
        case FieldType::UInt: {
            unsigned int* v = static_cast<unsigned int*>(p);
            int tmp = static_cast<int>(*v);
            const int lo = a.has_range ? static_cast<int>(a.min_value) : 0;
            const int hi = a.has_range ? static_cast<int>(a.max_value) : INT_MAX;
            bool c = a.has_range ? ImGui::SliderInt(id, &tmp, lo, hi)
                                 : ImGui::DragInt(id, &tmp, 1.0f, 0, INT_MAX);
            if (c) { if (tmp < lo) tmp = lo; *v = static_cast<unsigned int>(tmp); }
            changed = c;
            break;
        }
        case FieldType::Float: {
            float* v = static_cast<float*>(p);
            if (a.has_range)
                changed = ImGui::DragFloat(id, v, speed, static_cast<float>(a.min_value), static_cast<float>(a.max_value));
            else
                changed = ImGui::DragFloat(id, v, speed);
            break;
        }
        case FieldType::Double: {
            double* v = static_cast<double*>(p);
            double mn = a.min_value, mx = a.max_value;
            changed = ImGui::DragScalar(id, ImGuiDataType_Double, v, speed,
                                        a.has_range ? &mn : nullptr,
                                        a.has_range ? &mx : nullptr);
            break;
        }
        case FieldType::Vec2:
            changed = ImGui::DragFloat2(id, glm::value_ptr(*static_cast<glm::vec2*>(p)), speed);
            break;
        case FieldType::Vec3: {
            float* v = glm::value_ptr(*static_cast<glm::vec3*>(p));
            changed = a.is_color ? ImGui::ColorEdit3(id, v) : ImGui::DragFloat3(id, v, speed);
            break;
        }
        case FieldType::Vec4: {
            float* v = glm::value_ptr(*static_cast<glm::vec4*>(p));
            changed = a.is_color ? ImGui::ColorEdit4(id, v) : ImGui::DragFloat4(id, v, speed);
            break;
        }
        case FieldType::String: {
            auto* s = static_cast<std::string*>(p);
            char buf[256];
            std::strncpy(buf, s->c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(id, buf, sizeof(buf))) { *s = buf; changed = true; }
            break;
        }
        case FieldType::Enum: {
            const long long cur = f.get_enum ? f.get_enum(inst) : 0LL;
            const char* preview = "<?>";
            if (f.enum_info) {
                for (const auto& e : f.enum_info->entries)
                    if (e.value == cur) { preview = e.name.c_str(); break; }
            }
            if (ImGui::BeginCombo(id, preview)) {
                if (f.enum_info) {
                    for (const auto& e : f.enum_info->entries) {
                        const bool sel = (e.value == cur);
                        if (ImGui::Selectable(e.name.c_str(), sel)) {
                            if (f.set_enum) f.set_enum(inst, e.value);
                            changed = true;
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            break;
        }
        case FieldType::Struct:
            break;  // 结构体由 DrawField 以子树递归绘制，不走单控件路径
    }
    return changed;
}

void DrawFields(EditorContext& context, const TypeInfo& ti, void* inst,
                const std::function<void*()>& resolve);

void DrawField(EditorContext& context, const TypeInfo& ti, std::size_t idx,
               void* inst, const std::function<void*()>& resolve) {
    const FieldInfo& f = ti.fields[idx];
    const char* label = f.attr.label ? f.attr.label : f.name.c_str();

    // 嵌套结构：渲染为可折叠子树，对其子字段递归（resolve 组合：先解析外层再偏移到该字段）。
    if (f.type == FieldType::Struct) {
        if (!f.struct_info) return;
        ImGui::AlignTextToFramePadding();
        const std::string node_id = label + std::string("##") + ti.name + "." + f.name;
        const bool open = ImGui::TreeNodeEx(node_id.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::NextColumn();
        ImGui::NextColumn();
        if (open) {
            const FieldInfo* fp = &f;
            std::function<void*()> nested = [resolve, fp]() -> void* {
                void* outer = resolve();
                return outer ? fp->get(outer) : nullptr;
            };
            if (void* nested_inst = nested()) {
                DrawFields(context, *f.struct_info, nested_inst, nested);
            }
            ImGui::TreePop();
        }
        return;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (f.attr.tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", f.attr.tooltip);
    }
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(-1);

    const bool disabled = f.attr.read_only || context.read_only;
    if (disabled) ImGui::BeginDisabled(true);

    // 控件 id 以"类型名.字段名"命名（"##<Type>.<field>"）：跨 section 全局唯一
    // （同一实体上多组件可能有同名字段），同时便于 UI 测试寻址。
    const std::string widget_id = "##" + ti.name + "." + f.name;
    DrawWidget(widget_id.c_str(), f, inst);

    // 通用撤销：item 激活时快照旧值，编辑结束后压入 LambdaCommand（重新解析实例）。
    if (!disabled) {
        if (ImGui::IsItemActivated()) {
            s_undo_old = ReadField(f, inst);
            s_undo_type = &ti;
            s_undo_field = idx;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && s_undo_type == &ti && s_undo_field == idx) {
            FieldValue old_v = s_undo_old;
            FieldValue new_v = ReadField(f, inst);
            s_undo_type = nullptr;
            s_undo_field = static_cast<std::size_t>(-1);
            const TypeInfo* tip = &ti;
            const std::size_t fi = idx;
            auto apply = [resolve, tip, fi](const FieldValue& val) {
                if (void* live = resolve()) WriteField(tip->fields[fi], live, val);
            };
            std::string desc = ti.name + "." + f.name;
            GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                desc,
                [apply, new_v]() { apply(new_v); },
                [apply, old_v]() { apply(old_v); }), false);
        }
    }

    if (disabled) ImGui::EndDisabled();
    ImGui::NextColumn();
}

void DrawFields(EditorContext& context, const TypeInfo& ti, void* inst,
                const std::function<void*()>& resolve) {
    for (std::size_t i = 0; i < ti.fields.size(); ++i) {
        if (ti.fields[i].attr.hidden) continue;
        DrawField(context, ti, i, inst, resolve);
    }
}

} // namespace

bool DrawReflectedSection(EditorContext& context,
                          const char* header_label,
                          const TypeInfo& type_info,
                          const std::function<void*()>& resolve_instance) {
    void* inst = resolve_instance();
    if (!inst) return false;
    if (!ImGui::CollapsingHeader(header_label, ImGuiTreeNodeFlags_DefaultOpen)) return false;

    ImGui::Columns(2, "refl_cols", false);
    ImGui::SetColumnWidth(0, 130.0f);
    DrawFields(context, type_info, inst, resolve_instance);
    ImGui::Columns(1);
    return true;
}

} // namespace dse::editor
