#ifndef DSE_REFLECT_COMPONENT_REFLECTION_H
#define DSE_REFLECT_COMPONENT_REFLECTION_H

// 核心组件的反射注册（A 阶段后端：手写运行时注册）。
// 未来切到 C（codegen）时，本文件由生成代码取代，消费方不变。

namespace dse::reflect {

/// 幂等地注册所有已支持反射的核心组件。线程不安全，应在初始化早期调用一次。
/// 序列化/Inspector 等消费方在使用前应确保已调用（内部已做一次性保护）。
void EnsureCoreReflectionRegistered();

}  // namespace dse::reflect

#endif  // DSE_REFLECT_COMPONENT_REFLECTION_H
