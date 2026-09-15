# DSE 台式机编译 / 联调文档

> 适用：笔记本（本机） 台式机（构建机）协作开发 DSEngine。
> 台式机为 Windows 10 + VS2022 Community + 现成 CMake/Ninja 构建目录，**无外网**；
> 依赖（`depends/` 子模块）已在台式机就绪，日常只需同步源码增量。

## 1. 环境与路径

| 项 | 值 |
|---|---|
| 台式机 | `Administrator@169.254.139.190`（网线直连/内网，SSH 免密已配好） |
| 仓库（Windows） | `C:\Users\70195005\Desktop\Engine\DSEngine`（属主 wenbilin） |
| 构建目录 | `out\build\windows-x64-relwithdebinfo`（Ninja + MSVC，已配置好）与 `windows-x64-debug` |
| VS / vcvars | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` |
| Ninja | VS 自带：`...\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`（记录在 CMakeCache） |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| 产物 | `bin\dsengine_lua_relwithdebinfo.exe`（Lua 宿主）、`bin\dse.exe`、`bin\AssetBuilder.exe` 等 |
| 截图/日志落盘 | `C:\ProgramData\dse_shot.png`、`C:\ProgramData\dse_run.log` |

一次性给 Git 加白名单（避免 `dubious ownership`）：

```powershell
git config --global --add safe.directory C:/Users/70195005/Desktop/Engine/DSEngine
```

## 2. 同步源码（笔记本  台式机，增量 git bundle）

台式机 `feature/engine-lib` 一般落后于笔记本，用 `--not <台式机当前 HEAD>` 生成**增量包**即可（几 MB）：

```powershell
# 本机
cd E:\Engine\DSEngine
git bundle create _dse_sync.bundle feature/engine-lib --not <台式机HEAD>
scp -o BatchMode=yes _dse_sync.bundle Administrator@169.254.139.190:C:/ProgramData/dse_sync.bundle
```

```bat
:: 台式机（SSH 执行）
cd /d C:\Users\70195005\Desktop\Engine\DSEngine
git fetch C:/ProgramData/dse_sync.bundle feature/engine-lib:refs/heads/_sync && git merge --ff-only _sync && git branch -D _sync
```

只改 Lua/素材时不必走 git：直接 `scp templates\hd2d_wuxia\scripts\*.lua Administrator@...:<repo>\templates\hd2d_wuxia\scripts\` 即可（改 C++ 才需要重新编译）。

## 3. 编译（增量，实测约 30s）

>  Ninja + MSVC 需要先 `vcvars64`；`cmake --build` 不带 `--config`（Ninja 单配置）。

```bat
:: 台式机上执行（cmd）
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\Users\70195005\Desktop\Engine\DSEngine
cmake --build out\build\windows-x64-relwithdebinfo --target dse_example_lua dse_cli --parallel
```

- 全量重建（首次/改 CMake）：`cmake -S . -B out\build\windows-x64-relwithdebinfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ...`（见 `CMakePresets.json` 的 `windows-x64-relwithdebinfo`）。
- 只要编辑器：`--target dse_editor_cpp`；只要测试：`ctest --test-dir out\build\windows-x64-relwithdebinfo`。

## 4. 运行 / 自动化截图

```bat
cd /d C:\Users\70195005\Desktop\Engine\DSEngine
set DSE_MAX_FRAMES=420
set DSE_SCREENSHOT_FRAME=380
set DSE_SCREENSHOT_PATH=C:\ProgramData\dse_shot.png
set DSE_HD2D_AUTOSTART=1
set DSE_HD2D_DEMO=1
bin\dsengine_lua_relwithdebinfo.exe --script=templates\hd2d_wuxia\scripts\main.lua
```

拉回本机分析：

```powershell
scp Administrator@169.254.139.190:C:/ProgramData/dse_shot.png C:/ProgramData/dse_shot.png
```

实测（HD-2D 模板）：截图 `1028x720`，均值 RGB `(95,116,111)`，主角红衣 3040px、HUD 文字 2324px、
技能栏 9050px、灯笼暖光 2249px、草地绿 58%，无 Lua 报错  渲染/UI/战斗表现均正常。

## 5. 踩坑清单（务必遵守）

1. **不要在 `ssh "..."` 里塞复杂 PowerShell/bash**：引号与 `$` 会被本机 PowerShell 吞掉。
   正确做法：把脚本写成 `.ps1`（或 `.sh`） `scp` 过去  `ssh ... "powershell -NoProfile -ExecutionPolicy Bypass -File C:\ProgramData\x.ps1"`。
2. **SSH 会话结束会杀掉后台子进程**：长时间编译/运行必须**前台同步**执行（配合 `-o ServerAliveInterval=30 -o ServerAliveCountMax=600`），不要指望 `nohup &` / `Start-Process`。
3. `scp` 远端路径用 Windows 形式 `C:/ProgramData/...`（不要用 `/c/...`）。
4. 仓库属主是 `wenbilin`、SSH 用户是 `Administrator`  git 需 `safe.directory`。
5. 台式机**无外网**：任何新依赖/工具必须先在笔记本下载再传过去（`depends/` 已齐）。
6. MSVC 中文告警在 SSH 管道里是乱码，判断成功要看 `exit=0` 与产物 mtime；引擎在 Lua `Awake` 报错时会 **segfault**（`[FATAL] Process received signal 11`），先看 `C:\ProgramData\dse_run.log` 里的 `Lua Awake failed` 行。
7. 运行 Lua 时的相对路径以**仓库根**为基准（本模板 `core.resolve()` 兼容仓库内与生成工程两种布局）。

## 6. WSL 备选（本机/台式机均可）

台式机 WSL（Store 版 WSL2，`C:\Program Files\WSL\wsl.exe`）内编译（无需 VS，GL 走 X11 + Xvfb）：

```bash
sudo apt-get install -y libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libgl1-mesa-dev xvfb
cmake -S /mnt/e/Engine/DSEngine -B ~/dse_build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DDSE_ENABLE_3D=OFF -DDSE_ENABLE_JOLT=OFF -DDSE_ENABLE_ASSIMP=OFF -DDSE_ENABLE_SPINE=OFF \
      -DDSE_ENABLE_NAVMESH=OFF -DDSE_ENABLE_NET=OFF -DDSE_ENABLE_HTTP=OFF -DDSE_ENABLE_CSHARP=OFF \
      -DDSE_ENABLE_VULKAN=OFF -DDSE_BUILD_GTESTS=OFF -DDSE_BUILD_EDITOR=OFF
xvfb-run -s "-screen 0 1280x720x24" env DSE_MAX_FRAMES=420 DSE_SCREENSHOT_FRAME=380 \
      DSE_SCREENSHOT_PATH=/tmp/hd2d.png DSE_HD2D_AUTOSTART=1 DSE_HD2D_DEMO=1 \
      ./bin/dsengine_lua --script=templates/hd2d_wuxia/scripts/main.lua
```

> 2D-only + GCC 组合当前需要一处可移植性修复：`engine/ecs/ui_serializer.cpp` 的
> `rapidjson::Value(long long)` 在 `int64_t=long` 的平台上重载歧义，已用 `static_cast<int64_t>` 修正。