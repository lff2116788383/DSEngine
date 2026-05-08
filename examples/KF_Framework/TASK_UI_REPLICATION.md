# 任务: Title/Result/Loading 画面一比一复刻

## 目标
用 KF_Framework 原版纹理替换当前 bitmap font 文字，实现与原版 exe 画面一致的 Title、Result 和 Loading 过渡界面。

---

## 原版模式流程 (源码分析)

```
启动 → [Loading 动画] → Title → [FadeOut → Loading 动画 → FadeIn] → Battle
                                                                       ↓
Title ← [FadeOut → Loading 动画 → FadeIn] ← Result ← [FadeOut → Loading → FadeIn] ←
```

每次模式切换都经过完整的 Fade 流程:
1. **FadeOut** (1.0s): 黑幕 alpha 0→1
2. **FadeWaitOut** (0.5s): Loading 动画淡入 (alpha 0→1)
3. **FadeWait**: Loading 动画持续播放，等待资源加载完成
4. **FadeWaitIn** (0.5s): Loading 动画淡出 (alpha 1→0)
5. **FadeIn** (1.0s): 黑幕 alpha 1→0，新画面出现

---

## 一、Loading 动画 (fade_system.cpp) ⚠️ 目前 DSE 缺失

### 源码
```cpp
// fade_system.cpp Init()
// 黑色全屏遮罩
GameObjectSpawner::CreateBasicPolygon2d(
    Vector3(SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f),
    kUnableAutoDelete, L"fade", kDefault2dShader, k2dMask);

// Loading 精灵表动画
GameObjectSpawner::CreateScrollPolygon2d(
    Short2(2, 15),                    // 2列×15行 = 30帧
    2,                                 // 每帧持续 2 game-frames
    Vector3(412.0f, 64.0f, 0.0f),     // 尺寸: 412×64
    kUnableAutoDelete, L"loading",
    kDefault2dTextureShader, k2dMask,
    0.0f,
    Vector3(408.0f, 276.0f, 0.0f));   // 位置: 右下角 (x=408, y=276)
```

### Loading 动画行为 (scroll_2d_controller.cpp)
- Sprite sheet: `loading.png` (2 col × 15 row, 已在 `assets/textures/loading.png`)
- UV scroll: 每 2 帧切换 1 格，按 row-major 顺序遍历所有 30 帧
- 内容: 旋转的加载指示器 + "LOADING" → "LOADING." → "LOADING.." → "LOADING..."
- 位置: 屏幕中心偏右 408px，偏下 276px
- 尺寸: 412×64 px

### Fade 状态机时间参数 (fade_system.h)
```cpp
float fade_time_ = 1.0f;           // FadeIn/FadeOut 持续时间
float wait_fade_time_ = 0.5f;      // Loading 淡入/淡出时间
// 初始状态: kFadeWait (启动时先等待资源加载)
```

---

## 二、Title 画面 (mode_title.cpp)

### 源码
```cpp
void ModeTitle::Init(void) {
    // 全屏背景 title.jpg
    CreateBasicPolygon2d(Vector3(SCREEN_WIDTH, SCREEN_HEIGHT, 0), ..., L"title", ...);

    // "PLAY GAME" 按钮 — 左侧
    auto button = CreateButton2d(
        Vector3(400, 70, 0) * 0.8f,         // size: 320×56
        Vector3(-200, SCREEN_HEIGHT*0.25, 0), // pos: x=-200, y=+180 (下方)
        L"play_game.png", ...);
    button_controllers_[0] = ...;  // 初始 White

    // "DEMO PLAY" 按钮 — 右侧
    button = CreateButton2d(
        Vector3(400, 70, 0) * 0.8f,         // size: 320×56
        Vector3(200, SCREEN_HEIGHT*0.25, 0),  // pos: x=+200, y=+180 (下方)
        L"demo_play.png", ...);
    button_controllers_[1] = ...;
    button_controllers_[1]->ChangeColor(Color::kGray, 0.1f);  // 初始灰色
}

void ModeTitle::Update(void) {
    // 确认后等待 kWaitTime=0.25s 再 FadeTo
    if (time_counter_ > 0.0f) { time_counter_ -= dt; if <=0 FadeTo...; return; }

    // 左右切换
    if (Left || Right) {
        Play(kCursorSe);
        button_controllers_[current]->ChangeColor(kGray);
        next_mode_ ^= 1;  // toggle
        button_controllers_[new]->ChangeColor(kWhite);
    }
    // Submit 确认
    if (Submit) { Play(kSubmitSe); time_counter_ = 0.25f; }
}

void ModeTitle::OnCompleteLoading() { Play(kTitleBgm); }
```

### ButtonController 行为 (button_controller.cpp)
```cpp
// ChangeColor(target, time=0.1f) → 在 time 秒内 Lerp diffuse 到 target
void ButtonController::Update() {
    material_->diffuse_ = Lerp(current, target, time_counter / change_time);
}
```
- **选中**: diffuse = White (1,1,1,1)
- **未选中**: diffuse = Gray (0.5, 0.5, 0.5, 1) — 注意是 Lerp 过渡不是瞬间切换!

---

## 三、Result 画面 (mode_result.cpp)

### 源码
```cpp
void ModeResult::Init(void) {
    // 全屏背景 result.jpg "THANKS FOR PLAYING"
    CreateBasicPolygon2d(Vector3(SCREEN_WIDTH, SCREEN_HEIGHT, 0), ..., L"result", ...);

    // "PRESS ANY KEY" 闪烁按钮
    auto button = CreateFlashButton2d(
        1.0f,                                   // flash_speed 初始值
        Vector3(560, 73, 0),                    // size: 560×73
        ..., L"press_any_key", ...,
        Vector3(0, SCREEN_HEIGHT*0.25, 0));     // pos: x=0, y=+180 (居中下方)
    flash_button_controller_ = ...;
}

void ModeResult::Update(void) {
    // 确认后等待 kWaitTime=1.0s 再 FadeTo
    if (time_counter_ > 0.0f) { time_counter_ -= dt; if <=0 FadeTo(ModeTitle); return; }

    if (AnyKeyPressed) {
        Play(kSubmitSe);
        time_counter_ = 1.0f;
        flash_button_controller_->SetFlashSpeed(15.0f);  // 加速闪烁
    }
}

void ModeResult::OnCompleteLoading() { Play(kResultBgm); }
```

### FlashButtonController 行为 (flash_button_controller.cpp)
```cpp
// alpha 连续振荡: += flash_speed * dt, 到达 0 或 1 时反弹
void FlashButtonController::Update() {
    material_->diffuse_.a_ += flash_speed_ * dt;
    if (a >= 1.0) { a = 1.0; flash_speed_ *= -1; }
    if (a <= 0.0) { a = 0.0; flash_speed_ *= -1; }
}
```
- ⚠️ **不是 visibility 开关**，是 alpha 连续振荡!
- 初始 flash_speed = 1.0 → 1 秒完成一个 0→1 或 1→0 过程
- 按键后 flash_speed = 15.0 → 极快振荡

---

## 四、KF 坐标系
- 屏幕: 1280×720, 原点在中心
- Y+ = 向下 (注: KF 2D 坐标 y 正方向向下对应屏幕下方)
- `SCREEN_HEIGHT * 0.25 = 180` → 按钮在屏幕中心下方 180px
- Loading 动画位置 (408, 276) = 右下区域

---

## 纹理清单

| 文件 | 用途 | 原始尺寸 | 已复制 |
|------|------|----------|--------|
| `assets/textures/ui/title.jpg` | Title 全屏背景 | 1280×720 | ✅ |
| `assets/textures/ui/result.jpg` | Result 全屏背景 | 1280×720 | ✅ |
| `assets/textures/ui/play_game.png` | PLAY GAME 按钮 | ~400×70 | ✅ |
| `assets/textures/ui/demo_play.png` | DEMO PLAY 按钮 | ~400×70 | ✅ |
| `assets/textures/ui/press_any_key.png` | PRESS ANY KEY 闪烁 | ~560×73 | ✅ |
| `assets/textures/loading.png` | Loading 精灵表 (2×15=30帧) | ~824×960 | ✅ |

---

## 截图对比工具

### 工具路径
```
tools/capture_kf_original.py   — 启动原版 exe 自动截图
tools/verify_scene.py          — 运行 DSEngine 版本并截图分析
```

### 截取原版 KF exe 画面
```bash
# Title 画面 (默认, 启动后 5 秒截图)
python tools/capture_kf_original.py --wait 5

# Title + Result 连续截图 (需手动操作进入 Result)
python tools/capture_kf_original.py --result --wait 5

# 自定义输出文件名
python tools/capture_kf_original.py --wait 3 --out-name kf_loading.png
```

### 截取 DSEngine 版本画面
```bash
# 运行 60 帧后截图 (Title 画面)
python tools/verify_scene.py --frames 60

# 查看截图
copy screenshots\scene_capture.png tools\scene_temp.png
```

### 查看截图 (绕过 .gitignore)
```bash
copy screenshots\kf_original_title.png tools\kf_title_temp.png
copy screenshots\scene_capture.png tools\scene_temp.png
# 然后在 IDE 中打开 tools/ 下的文件查看
```

---

## 截图对比清单

### 对比 1: Title 画面 (静态)
| 项目 | 截图时机 | 验证要点 |
|------|----------|----------|
| 原版 | `--wait 5` (等 loading 结束) | 背景图 + 两按钮 + PLAY GAME 白色 + DEMO PLAY 灰色 |
| DSE | `--frames 60` | 同上布局 |

**对比要点**:
- 背景 title.jpg 铺满窗口，深灰色调
- "PLAY GAME" 左侧白色, "DEMO PLAY" 右侧灰色
- 按钮在背景标题文字下方约 180px
- 两按钮间距约 400px (各偏移中心 200px)

### 对比 2: Title 画面 (切换后)
| 项目 | 截图时机 | 验证要点 |
|------|----------|----------|
| 原版 | 手动按右键后截图 | PLAY GAME 变灰 + DEMO PLAY 变白 |
| DSE | 模拟按键后截图 | 同上颜色互换 |

### 对比 3: Loading 动画 ⚠️ 新增功能
| 项目 | 截图时机 | 验证要点 |
|------|----------|----------|
| 原版 | 启动后 ~1.5s (loading 中) | 黑色背景 + 右下 LOADING 动画 |
| DSE | 需实现后测试 | 精灵表动画 + 淡入淡出 + 位置正确 |

**对比要点**:
- 全屏黑色背景
- Loading 动画在右下区域 (x=408, y=276 from center)
- 尺寸 412×64
- 动画帧切换流畅 (旋转指示器 + 逐点出现的省略号)
- 淡入 0.5s → 播放 → 淡出 0.5s

### 对比 4: Result 画面
| 项目 | 截图时机 | 验证要点 |
|------|----------|----------|
| 原版 | 手动进入 Result (`--result`) | 背景图 + PRESS ANY KEY 闪烁 |
| DSE | 战斗结束后截图 | 同上 |

**对比要点**:
- 背景 result.jpg 铺满窗口
- "PRESS ANY KEY" 居中偏下 180px
- Alpha 振荡闪烁 (非 visibility 开关!)
- 按键后快速闪烁 (speed 15.0)

### 对比 5: Fade 过渡 (可选)
| 项目 | 截图时机 | 验证要点 |
|------|----------|----------|
| 原版 | 按下确认后 ~0.5s | 黑幕半透明 |
| DSE | 对应帧 | 黑幕渐变效果一致 |

---

## 实现优先级

### P-1 — UI 透明度修复 ✅ (2025-05-08)
- [x] **场景透过UI可见**: CompositePass 改为 DrawPostProcess("ui_overlay")，三后端加 alpha blend
- [x] **UI RT alpha 不正确**: OpenGL 改用 glBlendFuncSeparate，alpha 通道 (One, 1-SrcAlpha)
- [x] **UI颜色发白**: PBR shader 非光照路径用 light_params.w 标志跳过 tone mapping (三后端)
- [x] **管线状态不同步**: 移除 DrawPostProcess 中的直接 glEnable(GL_DEPTH_TEST) 调用
- [x] **UI 尺寸硬编码**: gameflow.lua 改为 app.get_screen_width()/height()
- **已验证**: OpenGL 后端 ✅ | DX11 后端 ❌ 待验证 | Vulkan 后端 ❌ 待验证

### P0 — 已完成 ✅
- [x] Title 背景 title.jpg 全屏
- [x] PLAY GAME / DEMO PLAY 按钮纹理 + 位置
- [x] 左右键切换 + 颜色切换 (白/灰)
- [x] Result 背景 result.jpg 全屏
- [x] PRESS ANY KEY 纹理 + 位置

### P1 — 需修复 (行为差异)
- [ ] FlashButton: 改为 alpha 连续振荡 (当前用 visibility 开关)
- [ ] ButtonController: 添加 0.1s color lerp 过渡 (当前瞬间切换)
- [ ] Title 确认后延迟 0.25s 再 FadeTo (当前立即触发)
- [ ] Result 确认后延迟 1.0s 再 FadeTo (当前立即触发)

### P2 — 需新增 (Loading 动画)
- [ ] 实现 sprite-sheet UV scroll 动画
- [ ] 在 fade.lua 中添加 Loading 实体 + 淡入/淡出
- [ ] Loading 位置: 屏幕右下 (408, 276), 尺寸 412×64
- [ ] 加载 loading.png 纹理
- [ ] FadeOut → Loading淡入(0.5s) → 动画播放 → Loading淡出(0.5s) → FadeIn

---

## DSE API 参考

```lua
-- ui.add_renderer(entity, tex_handle, r, g, b, a, order, width, height)
-- ui.set_anchor(entity, anchor_x, anchor_y)  -- 0.5,0.5=屏幕中心
-- ui.set_color(entity, r, g, b, a)           -- 修改颜色/alpha
-- ui.set_visible(entity, bool)               -- 显示/隐藏
-- ui.set_uv(entity, u, v, uw, vh)            -- UV offset+scale (需验证是否存在)
```

## 音效 key 名 (script/audio.lua)
- `cursor` — 按钮切换音效 (已注册 ✅)
- `submit` — 确认音效 (已注册 ✅)

## 注意事项
- 不要修改 `player.lua` / `enemy.lua` / `main.lua` 的核心逻辑
- Loading 动画需要 UV scroll 支持 — 需确认 DSE 是否有 `ui.set_uv()` API
- FlashButton 的 alpha 振荡在 speed=15 时非常快 (约 0.067s 一个周期)
- KF 原版 Result 画面没有 VICTORY/DEFEAT 文字区分，只有 "THANKS FOR PLAYING" 背景
- 启动时 FadeSystem 初始状态就是 kFadeWait → 即第一帧就在等 loading
