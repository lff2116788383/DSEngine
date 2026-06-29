#pragma once

#include <functional>

namespace dse::reflect { struct TypeInfo; }

namespace dse::editor {

struct EditorContext;

/// 通用反射 Inspector 驱动（A->C 迁移的消费方）。
///
/// 给定一个已注册的 TypeInfo 与"取得实例指针"的回调，按 FieldInfo 遍历渲染
/// ImGui 控件（依据 FieldType + FieldAttributes：range/step/tooltip/read_only/
/// is_color/label），并接入撤销栈。不直接依赖任何具体组件类型，因此后端从
/// 手写注册（A）切换到 codegen（C）时本驱动一行不改。
///
/// @param header_label   CollapsingHeader 标题（含图标）。
/// @param type_info      组件的反射类型信息。
/// @param resolve_instance 返回组件实例的可写指针；组件已被移除时返回 nullptr。
///                         撤销命令会在执行/回退时重新解析，避免缓存悬垂指针。
/// @return 是否绘制了 section 主体（header 折叠或实例不存在时返回 false）。
bool DrawReflectedSection(EditorContext& context,
                          const char* header_label,
                          const dse::reflect::TypeInfo& type_info,
                          const std::function<void*()>& resolve_instance);

} // namespace dse::editor
