### DSEngine

### 推荐入口

当前仓库默认推荐的 Lua 范例是 `samples/lua/phase1_2d_physics_showcase.lua`。

它覆盖以下链路：
- **2D 刚体下落**：动态箱体落到静态地面
- **触发器回调**：箱体穿过触发区域时输出 enter / exit 日志
- **射线检测**：水平射线持续探测并显示命中实体
- **命中高亮**：射线命中的实体会被高亮显示
- **射线可视化**：橙色点列显示当前射线路径

### 运行方式

- **快速构建 Lua 宿主**
  - 运行 `build_fast_lua.bat`
- **直接构建目标**
  - `cmake --build build_vs2022 --config Debug --target dse_example_lua`
- **启动示例**
  - 运行 `bin/DSEngine_lua_debug.exe`
  - 或运行 `bin/DSEngine_lua.exe`

### 样例目录说明

`samples/lua/` 保留多个 Lua 示例，当前默认推荐入口为 2D 物理 showcase：
- `main.lua`：Lua 宿主入口，会根据 `config.lua` 的 `game_entry` 选择示例
- `config.lua`：运行配置，默认指向 `phase1_2d_physics_showcase`
- `phase1_2d_physics_showcase.lua`：2D 物理推荐范例
