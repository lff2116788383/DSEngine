# Lua 绑定审计工具

用于核对 `engine/scripting/lua/bindings/` 实际注册的 Lua 函数与 `docs/api/LUA_API.md` 的同步情况。

## 脚本

| 脚本 | 作用 |
|------|------|
| `lua_api_audit.py` | 提取所有已注册 Lua 函数名，对比文档，报告“已绑定但未文档化”的函数 |
| `gen_accessor_doc.py` | 从 `tools/codegen/binding_defs.json` 生成 §18 组件字段访问器文档表 |
| `sig_detail.py` / `sig_extract.py` | 解析 C 函数体，提取参数类型/默认值/返回值数量 |

## 用法

```bash
cd docs/api
python ../../tools/audit/lua_api_audit.py        # 期望 "BOUND but NOT in doc ( 0 )"
```

Windows 上若中文输出乱码，先设 `PYTHONUTF8=1 PYTHONIOENCODING=utf-8`。

中间产物（`*.txt` / `*.json` / `accessor_doc.md`）由 `.gitignore` 忽略，不纳入版本控制。
