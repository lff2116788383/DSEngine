# Lua 绑定 Codegen 维护指南

本引擎的 Lua 自由函数绑定由 codegen 生成，**不要手写** `*.gen.cpp`。
所有改动都在 JSON 定义 + Jinja2 模板中完成，再运行 codegen 重新生成。

## 1. 目录与产物

| 路径 | 作用 |
| --- | --- |
| `tools/codegen/function_defs.json` | 自由函数定义（`function_groups` + `free_functions`）。 |
| `tools/codegen/binding_defs.json` | 组件字段的 get/set 属性访问器定义。 |
| `tools/codegen/templates/lua_binding_free.cpp.j2` | 自由函数生成模板。 |
| `tools/codegen/codegen.py` | 生成器入口。 |
| `engine/scripting/lua/bindings/lua_binding_free_<group>.gen.cpp` | 每组生成的绑定（勿手改）。 |
| `engine/scripting/lua/bindings/lua_binding_ecs.cpp` | 聚合各组 `RegisterEcs*Bindings`。 |
| `engine/scripting/native_api/dse_api.h` + `dse_api_*.h` + `dse_api_*.cpp` | C ABI 声明与实现（含 `dse_compat_*` 薄包装）。`dse_api.h` 为聚合头，按模块拆分为 `dse_api_core.h` / `dse_api_render.h` / `dse_api_physics.h` / `dse_api_world.h` / `dse_api_services.h` / `dse_api_gameplay.h`。 |

运行：
```bat
cd tools\codegen && python codegen.py
```

## 2. 组与注册

每个组含 `lua_path`（如 `["dse","ecs"]`）、`register_func`（如 `RegisterEcsRenderingMeshBindings`）、`functions`。
生成的 `L_<c_name>` 函数在**匿名命名空间**内（TU 内部链接），因此：
- **跨组同名 `L_` 安全**（不同 .gen.cpp）。
- **同组内 `c_name` 必须唯一**，否则同一 TU 内符号重复导致编译失败。

Lua 里最终函数名 = `lua_name`。多个注册表向同一个 Lua 表（如 `dse.ecs`）写入时，**后注册的覆盖先注册的**——避免让两处定义同一个 `lua_name`。

## 3. 添加一个函数

在对应组的 `functions` 里加一条：
```json
{
  "c_name": "dse_foo_bar",
  "lua_name": "foo_bar",
  "params": [
    { "name": "e", "type": "uint32" },
    { "name": "value", "type": "float" }
  ]
}
```
`c_name` 必须在 `dse_api.h` 中有匹配签名的声明。签名不符时见第 6 节。

## 4. 参数类型（params[].type）

| type | 生成 | 备注 |
| --- | --- | --- |
| `uint32` | `luaL_checkinteger` → uint32_t | 实体 id 常用。 |
| `int` | `luaL_checkinteger` → int | |
| `float` / `double` | `luaL_checknumber` | |
| `bool` | `helper::CheckBool(L,i)?1:0` | **传 `true/false` 必须用 bool**；写成 int 会拒绝布尔值报错。 |
| `string` | `luaL_checkstring` | |

可选参数加 `"default": <字面量>`：
```json
{ "name": "gravity_scale", "type": "float", "default": "1.0f" },
{ "name": "path", "type": "string", "default": "\"\"" }
```
生成 `luaL_opt*`，Lua 调用可省略该尾参。

### nil-skip 哨兵：`"keep_on_nil": true`
Lua 传 `nil` → 给 C ABI 传哨兵，由 C 侧 `Keep()` 判定保留原值：
- float → `std::nanf("")`（C 侧 `std::isnan` 判定）
- int → `-1`

```json
{ "name": "intensity", "type": "float", "keep_on_nil": true }
```
用于 `set_weather` / `set_cloud_layer` 等「部分字段可省略、保留当前值」的 setter。
模板顶部已 `#include <cmath>`。

## 5. 返回与数组参数

| 特性 | 键 | 说明 |
| --- | --- | --- |
| 多返回值 | `out_params: [{name,type}]` | type 支持 `float/int/uint32/double/bool/float2/float3/float4/float6`。指针传入，末尾 push 出去。`bool` → `lua_pushboolean`。 |
| bool 返回 | `returns: {type:"bool"}` | 单 bool 返回。 |
| 0→nil 返回 | `returns: {..., "nil_if_zero": true}` | uint32 返回 0 时 push nil（如 `load_material` 缺失文件）。 |
| int 数组入参 | `int_array_param: {name, stack_index}` | Lua table → `std::vector<uint32_t>` → `data()`+`size()`（如 `cloth_pin_vertices`）。 |
| float 数组入参 | `float_array_param: {name, stack_index}` | Lua table → `std::vector<float>` → `data()`+`size()`（如 heightmap/morph）。 |
| string 数组入参 | `string_array_param: {name, stack_index}` | Lua table → `const char*[]`。 |
| 缓冲输出 | `buffer_output` | C ABI 填充定长数组 → Lua table（如 `overlap_sphere`）。 |
| 多次调用 | `calls: [{c_name,args}]` | 一个 lua_name 依次调 N 个 C ABI（如 `add_camera_3d` = add + set_priority）。 |

> **栈索引注意**：`vec2`/`vec3` 各吃 2/3 个栈位。数组类参数需显式给 `stack_index`。若某函数在 vec 之后还有参数，优先拆成逐个 `float` 参数，避免栈错位。

## 6. C 签名不符 → `dse_compat_*` 薄包装

当 Lua 期望的签名与现有 C ABI 不一致（参数个数/顺序/类型不同，或需复合 nil-skip 语义）时，**不要改动既有 C ABI**，而是在 `dse_api_*.cpp` 加一个 `dse_compat_*` 薄包装，只调用既有 C ABI，零行为变更；在 `dse_api.h` 声明；`function_defs.json` 的 `c_name` 指向它。示例（3D 灯光复合 setter、`world_to_screen` 调整返回顺序、`add_weather` 字符串枚举 → int）见 `dse_api_gameplay3d.cpp` 尾部 `S1.9 3D compat` 段落。

## 7. 组件属性（binding_defs.json）

组件字段的 `get_/set_` 访问器在此定义：
```json
{ "name": "collision_layer", "type": "int",
  "lua_getter": "get_ragdoll_collision_layer",
  "lua_setter": "set_ragdoll_collision_layer" }
```
若某 `lua_name` 需要更复杂的多参语义（如 `set_ragdoll_collision_layer(e,layer,mask)`），把属性 setter 改名让出该名字，再在 `function_defs.json` 用自由函数（或 compat 包装）占用该 `lua_name`。

## 8. 验证流程

```bat
cd tools\codegen && python codegen.py
cmake --build build --config Release -- /m:6 /v:m /nodeReuse:false
bin\dse_gtest_integration_tests.exe
ctest -C Release            :: unit + integration + smoke 全绿
```
> MSBuild 并行节点死锁时用 `/m:6 /v:m /nodeReuse:false`，重编前清理残留 `MSBuild/cl/link/mspdbsrv` 进程。

## 9. 铁律

- 不改测试；测试是权威签名来源。
- 用 codegen，不写手写绑定文件。
- C 签名不符加 `dse_compat_*` 薄包装，不改既有 C ABI。
- 同组内 `c_name` 保持唯一；同一 Lua 表内 `lua_name` 不要重复定义。
