# 教程 ①：用 DSEngine 做一个「金币收集」2D 小游戏

> 接着 [快速上手](QUICKSTART.md) 往下做。我们在 `dse new 2d` 生成的模板基础上，一步步加出一个真正的小游戏：
> **控制方块四处移动 → 碰到金币就吃掉并加分 → 吃完全部金币就通关。**
>
> 全程只改一个文件：`scripts/main.lua`。本教程用到的每个 `dse.*` 接口都对照引擎源码核实过，复制即可运行。

预计耗时 20 分钟。前置：已按快速上手跑通 `dse new 2d` → `dse build` → 运行 exe。

---

## 0. 先建好项目

```powershell
dse new 2d CoinGame
```

下面每一步都是在 `CoinGame\scripts\main.lua` 里改动。每改完一步，用这两条命令看效果：

```powershell
dse build CoinGame
CoinGame\build\dist\CoinGame.exe
```

> 没独显 / 在虚拟机里跑不出窗口？先按快速上手第 3 步部署 llvmpipe 软件渲染，并设 `$env:GALLIUM_DRIVER="llvmpipe"`。

---

## 1. 复习模板：我们已有什么

模板生成的脚本里，已经有一个能用 WASD 移动的方块。两个核心函数：

- `Awake()` —— 启动时调用一次：建相机、建玩家方块
- `Update(dt)` —— 每帧调用一次：读键盘、移动方块

我们要做的就是在这两个函数里**加金币、加碰撞检测、加计分**。

DSEngine 的对象模型是 **ECS（实体-组件）**：

- `dse.ecs.create_entity()` 创建一个空实体，返回它的 id
- 给实体「加组件」让它具备能力：
  - `add_transform(e, x,y,z, sx,sy,sz)` —— 位置 + 缩放
  - `add_sprite(e, r,g,b,a, order)` —— 让它显示成一个纯色方块（RGBA 取值 0~1）
  - `add_camera(e, ortho_size)` —— 让它成为 2D 正交相机，`ortho_size` 是纵向可见范围的一半（世界单位）

---

## 2. 生成一排金币

金币就是「一堆黄色小方块」。我们在 `Awake()` 里批量创建，并把每个金币的实体 id 和坐标记在一张 Lua 表里，方便之后做碰撞和删除。

在脚本顶部、`Awake` 之前，加上金币相关的状态：

```lua
local coins = {}          -- 每项: { entity = id, x = ..., y = ..., alive = true }
local score = 0
local total_coins = 0
local coin_size = 0.5     -- 金币方块的缩放（半边长大约 0.25 世界单位）
local pickup_radius = 0.6 -- 玩家中心离金币多近算「吃到」
```

加一个生成金币的辅助函数：

```lua
local function spawn_coin(x, y)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, coin_size, coin_size, 1.0)
    dse.ecs.add_sprite(e, 1.0, 0.85, 0.10, 1.0, 0)   -- 金黄色
    coins[#coins + 1] = { entity = e, x = x, y = y, alive = true }
end
```

在 `Awake()` 里、创建完玩家之后，撒几个金币：

```lua
    -- 在地图上摆 5 个金币
    spawn_coin(-3.0,  2.0)
    spawn_coin( 3.0,  2.0)
    spawn_coin(-3.0, -2.0)
    spawn_coin( 3.0, -2.0)
    spawn_coin( 0.0,  3.0)
    total_coins = #coins
    print(string.format("[CoinGame] collect all %d coins!", total_coins))
```

构建运行，你会看到中间一个青色玩家方块，四周散着 5 个黄色金币。现在还吃不掉它们——下一步加碰撞。

---

## 3. 碰到金币就吃掉并加分

「碰到」用最简单的**圆形距离判定**：玩家中心和金币中心的距离小于 `pickup_radius` 就算吃到。吃到后：

- `dse.ecs.destroy_entity(coin.entity)` 把金币从世界里删掉（它就消失了）
- 把这枚金币标记为 `alive = false`，分数 +1

在 `Update(dt)` 里、移动玩家之后，加一段检测：

```lua
    -- 碰撞检测：玩家 vs 每个还活着的金币
    for _, coin in ipairs(coins) do
        if coin.alive then
            local ddx = pos.x - coin.x
            local ddy = pos.y - coin.y
            local dist2 = ddx * ddx + ddy * ddy
            if dist2 <= pickup_radius * pickup_radius then
                coin.alive = false
                dse.ecs.destroy_entity(coin.entity)
                score = score + 1
                print(string.format("Coin! score = %d / %d", score, total_coins))
            end
        end
    end
```

> 我们用 `dist2`（距离的平方）和 `pickup_radius` 的平方比较，省掉一次开方——这是游戏里很常见的小优化。

构建运行：把方块开过去碰金币，金币消失，控制台打印 `Coin! score = 1 / 5`……

---

## 4. 吃完全部金币就通关

加一个 `won` 标志，避免重复打印通关信息。在脚本顶部状态里加：

```lua
local won = false
```

在 `Update(dt)` 末尾，加胜利判定：

```lua
    if not won and score >= total_coins then
        won = true
        print("YOU WIN! All coins collected.")
    end
```

构建运行，吃光 5 个金币后控制台会打印 `YOU WIN!`。恭喜——你已经做出了一个有目标、有反馈、有胜负的完整小游戏。

---

## 5. 完整脚本

下面是最终的 `scripts/main.lua`，可直接整体替换：

```lua
-- CoinGame — 金币收集 2D 小游戏
local app = dse.app
local KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP = 263, 262, 264, 265
local KEY_A, KEY_D, KEY_S, KEY_W = 65, 68, 83, 87

local player
local pos = { x = 0.0, y = 0.0 }
local speed = 5.0

local coins = {}
local score = 0
local total_coins = 0
local coin_size = 0.5
local pickup_radius = 0.6
local won = false

local function spawn_coin(x, y)
    local e = dse.ecs.create_entity()
    dse.ecs.add_transform(e, x, y, 0.0, coin_size, coin_size, 1.0)
    dse.ecs.add_sprite(e, 1.0, 0.85, 0.10, 1.0, 0)
    coins[#coins + 1] = { entity = e, x = x, y = y, alive = true }
end

function Awake()
    local camera = dse.ecs.create_entity()
    dse.ecs.add_transform(camera, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_camera(camera, 5.0)

    player = dse.ecs.create_entity()
    dse.ecs.add_transform(player, pos.x, pos.y, 0.0, 1.0, 1.0, 1.0)
    dse.ecs.add_sprite(player, 0.30, 0.80, 1.0, 1.0, 0)

    spawn_coin(-3.0,  2.0)
    spawn_coin( 3.0,  2.0)
    spawn_coin(-3.0, -2.0)
    spawn_coin( 3.0, -2.0)
    spawn_coin( 0.0,  3.0)
    total_coins = #coins
    print(string.format("[CoinGame] collect all %d coins!", total_coins))
end

function Update(dt)
    dt = dt or 0.0

    -- 移动
    local dx, dy = 0.0, 0.0
    if app.get_key(KEY_A) or app.get_key(KEY_LEFT)  then dx = dx - 1.0 end
    if app.get_key(KEY_D) or app.get_key(KEY_RIGHT) then dx = dx + 1.0 end
    if app.get_key(KEY_W) or app.get_key(KEY_UP)    then dy = dy + 1.0 end
    if app.get_key(KEY_S) or app.get_key(KEY_DOWN)  then dy = dy - 1.0 end
    pos.x = pos.x + dx * speed * dt
    pos.y = pos.y + dy * speed * dt
    dse.ecs.set_transform_position(player, pos.x, pos.y, 0.0)

    -- 收集金币
    for _, coin in ipairs(coins) do
        if coin.alive then
            local ddx = pos.x - coin.x
            local ddy = pos.y - coin.y
            if ddx * ddx + ddy * ddy <= pickup_radius * pickup_radius then
                coin.alive = false
                dse.ecs.destroy_entity(coin.entity)
                score = score + 1
                print(string.format("Coin! score = %d / %d", score, total_coins))
            end
        end
    end

    -- 通关判定
    if not won and score >= total_coins then
        won = true
        print("YOU WIN! All coins collected.")
    end
end
```

---

## 6. 自己动手改改看

- **更多金币 / 随机位置**：用 `math.random` 在 `Awake` 里随机撒金币（`spawn_coin(math.random(-4,4), math.random(-4,4))`）。
- **加速带**：吃到某个特殊颜色的金币时把 `speed` 调大。
- **限时挑战**：在 `Update` 里累加 `dt` 到一个 `timer`，超过 N 秒还没吃完就 `print("Time up!")`。
- **会动的金币**：给金币也存一个速度，在 `Update` 里更新它们的 `coin.x/coin.y` 并 `set_transform_position`。

---

## 本教程用到的 Lua 接口（均对照源码核实）

| 接口 | 作用 |
|------|------|
| `dse.ecs.create_entity()` → id | 创建实体 |
| `dse.ecs.destroy_entity(e)` | 销毁实体（金币消失） |
| `dse.ecs.add_transform(e, x,y,z, sx,sy,sz)` | 加位置 + 缩放组件 |
| `dse.ecs.set_transform_position(e, x,y,z)` | 设置实体位置 |
| `dse.ecs.add_sprite(e, r,g,b,a, order?, tex?)` | 加纯色/贴图精灵（RGBA 0~1；`tex=0` 时为纯色方块） |
| `dse.ecs.add_camera(e, ortho_size?, priority?)` | 加 2D 正交相机 |
| `dse.app.get_key(code)` → bool | 查询某个键是否按下（GLFW 键码） |
| `print(...)` | 输出到控制台 |

更多接口见 [Lua API 参考](../api/LUA_API.md)。

---

## 下一步

- 想把游戏发到网上？回到 [快速上手第 5 步](QUICKSTART.md#第-5-步可选导出到-web)，用 `dse build --target web` 导出到浏览器。
- 想看更完整的 2D 示例（动画、粒子、物理）？参考仓库 `samples/lua/` 下的演示脚本。
