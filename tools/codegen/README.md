# DSEngine Binding Codegen

统一代码生成系统，从单一数据源 (`binding_defs.json`) 驱动 C ABI / Lua / C# 三端绑定代码同步生成。

## 架构

```
binding_defs.json (唯一数据源)
       │
       ▼
   codegen.py (Jinja2 模板引擎)
       │
       ├── dse_api.gen.h              — C ABI 头文件声明
       ├── dse_api_<prefix>.gen.cpp   — C ABI 实现（每组件一个 TU）
       ├── lua_binding_ecs_<prefix>.gen.cpp — Lua 薄包装绑定
       ├── Native.gen.cs              — C# P/Invoke 声明
       ├── Components.gen.cs          — C# 高级封装类
       ├── repl_codec.gen.h           — 网络复制编解码
       ├── component_reflection.gen.cpp — 反射注册
       ├── scene_json_codec.gen.h     — 场景 JSON 序列化
       ├── NativeManual.gen.cs        — 手写 C ABI 的 C# P/Invoke
       └── ApiManual.gen.cs           — 手写 C ABI 的 C# 门面类
```

## 用法

```bash
# 重新生成所有绑定文件
python tools/codegen/codegen.py

# 指定输出目录
python tools/codegen/codegen.py --out /path/to/repo

# 干运行（只打印，不写文件）
python tools/codegen/codegen.py --dry-run

# 通过 CMake target 运行
cmake --build <build_dir> --target dse_codegen
```

依赖：`pip install jinja2`

## 添加新组件

### 步骤 1：在 `binding_defs.json` 中添加组件定义

```json
{
    "name": "MyNewComponent",
    "prefix": "my_new",
    "lua_table": "ecs",
    "include": "engine/ecs/my_component.h",
    "namespace": "dse",
    "fields": [
        {
            "name": "enabled",
            "type": "bool",
            "default": "true",
            "lua_getter": "get_my_new_enabled",
            "lua_setter": "set_my_new_enabled"
        },
        {
            "name": "speed",
            "type": "float",
            "default": "1.0f",
            "range": [0.0, 100.0],
            "lua_getter": "get_my_new_speed",
            "lua_setter": "set_my_new_speed"
        },
        {
            "name": "color",
            "type": "vec4",
            "color": true,
            "lua_getter": "get_my_new_color",
            "lua_setter": "set_my_new_color"
        },
        {
            "name": "label",
            "type": "string",
            "buffer_size": 256,
            "lua_getter": "get_my_new_label",
            "lua_setter": "set_my_new_label"
        }
    ]
}
```

### 步骤 2：运行 codegen

```bash
python tools/codegen/codegen.py
```

### 步骤 3：注册 Lua 绑定

在 `engine/scripting/lua/bindings/lua_binding_modules.h` 中添加声明：

```cpp
void RegisterMyNewComponentGenBindings(lua_State* L);
```

在 `engine/scripting/lua/bindings/lua_binding_ecs.cpp` 的 `RegisterEcsBindings()` 中添加调用：

```cpp
RegisterMyNewComponentGenBindings(L);
```

### 完成

CMake 的 `file(GLOB)` 会自动发现新生成的 `.gen.cpp` 文件，无需手动修改 `CMakeLists.txt`。

## 支持的字段类型

| 类型 | C ABI 签名 | Lua 行为 |
|------|-----------|---------|
| `float` | `float get(uint32_t)` / `void set(uint32_t, float)` | `lua_pushnumber` / `luaL_checknumber` |
| `int` | `int get(uint32_t)` / `void set(uint32_t, int)` | `lua_pushinteger` / `luaL_checkinteger` |
| `bool` | `int get(uint32_t)` / `void set(uint32_t, int)` | `lua_pushboolean` / `lua_toboolean` |
| `vec3` | `void get(uint32_t, float*, float*, float*)` / `void set(uint32_t, float, float, float)` | 返回 3 个 number |
| `vec4` | `void get(uint32_t, float*, float*, float*, float*)` / `void set(uint32_t, float, float, float, float)` | 返回 4 个 number |
| `euler_quat` | 同 `vec3`，但内部做欧拉角↔四元数转换 | 返回 3 个 number（角度制） |
| `string` | `int get(uint32_t, char*, int)` / `void set(uint32_t, const char*)` | `lua_pushstring` / `luaL_checkstring` |
| `enum_int` | 同 `int`，但 setter 做枚举类型转换 | 同 `int` |

## 字段属性

| 属性 | 说明 |
|------|------|
| `default` | 组件不存在或字段未设置时的默认返回值 |
| `range` | `[min, max]` 范围限制（用于反射 UI） |
| `color` | `true` 表示该 vec4 字段是颜色（反射 UI 显示颜色选择器） |
| `readonly` | `true` 表示只生成 getter，不生成 setter |
| `dirty_flag` | setter 执行后设置的脏标记字段名 |
| `buffer_size` | string 类型的缓冲区大小（默认 512） |
| `script` | `false` 表示该字段仅用于反射，不生成脚本绑定 |
| `lua_getter` / `lua_setter` | Lua 中注册的函数名 |

## 组件属性

| 属性 | 说明 |
|------|------|
| `name` | C++ 组件类名 |
| `prefix` | C ABI 函数前缀（如 `transform` → `dse_transform_get_*`） |
| `include` | 组件头文件路径 |
| `namespace` | C++ 命名空间 |
| `conditional` | 条件编译宏（如 `DSE_ENABLE_PHYSX \|\| DSE_ENABLE_JOLT`） |
| `lua_table` | Lua 注册表名（默认 `ecs`） |

## 手写 C ABI 函数

非组件字段的 C ABI 函数（如 `dse_input_get_key`、`dse_core_get_time`）仍手写在 `dse_api.h` / `dse_api_*.cpp` 中。它们的 C# P/Invoke 声明由 `gen_csharp_manual.py` 自动生成到 `NativeManual.gen.cs` 和 `ApiManual.gen.cs`。

## 扩展 codegen

### 添加新模板

1. 在 `templates/` 目录下创建 `.j2` 文件
2. 在 `codegen.py` 的 `main()` 中添加 `render()` 调用

### 修改现有模板

模板使用 Jinja2 语法，支持 `trim_blocks` 和 `lstrip_blocks`。修改模板后运行 `python codegen.py` 重新生成所有文件。

## 文件说明

| 文件 | 说明 |
|------|------|
| `codegen.py` | 主代码生成器，读取 `binding_defs.json` 并渲染所有模板 |
| `binding_defs.json` | 组件字段定义（唯一数据源） |
| `expand_defs.py` | 扩展工具：为现有组件添加反射元数据 + 批量添加新组件 |
| `gen_csharp_manual.py` | 从 `dse_api.h` 解析手写 C ABI 函数，生成 C# P/Invoke |
| `templates/` | Jinja2 模板目录 |
