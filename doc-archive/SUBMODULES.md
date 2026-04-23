## 子模块版本与升级策略

本文档说明 DSEngine 当前子模块的**固定版本来源**与**推荐升级方式**。

### 总体原则

- **默认使用主仓库锁定的子模块提交**，保证构建可复现。
- **`.gitmodules` 中的 `branch` 仅表达推荐更新来源**，不代表子模块会自动漂移到该分支最新。
- 对于存在稳定 **tag** 的依赖，优先按稳定 tag 选型并固定到对应 commit。
- 对于主要通过长期维护分支发布的依赖，可在 `.gitmodules` 中补充 `branch` 作为后续人工升级入口。

### 当前版本映射

| 子模块 | 当前固定版本来源 | 当前策略 |
| --- | --- | --- |
| `depends/assimp` | `v5.4.3` | 固定稳定 tag，对外更新来源标记为 `master` |
| `depends/box2d-2.4.1` | `v2.4.1` | 固定稳定 tag |
| `depends/entt-3.13.0` | `v3.13.0` | 固定稳定 tag |
| `depends/eventpp` | `v0.1.3` | 固定稳定 tag |
| `depends/glfw-3.3-3.4` | `3.3.4` | 固定稳定 tag |
| `depends/glm` | `1.0.1` | 固定稳定 tag |
| `depends/imgui` | `origin/docking` 上的固定提交，当前描述为 `v1.92.6-docking-94-g148bd34a7` | 固定稳定分支上的提交 |
| `depends/lua` | `v5.4.6` | 固定稳定 tag |
| `depends/luasocket-3.0.0` | `v3.1.0` | 固定稳定 tag |
| `depends/rapidjson` | `v1.1.0` | 固定稳定 tag |
| `depends/spdlog` | `v1.14.1` | 固定稳定 tag |
| `depends/spine-runtimes` | `origin/4.2` 上的固定提交，当前描述为 `spine-libgdx-4.2.1-676-g99ebb13ed` | 固定稳定分支上的提交 |
| `depends/stb` | `origin/master` 上的固定提交 | 无 tag，固定提交 |
| `depends/tiny-AES-c` | 接近 `v1.0.0` 的固定提交，当前描述为 `v1.0.0-34-g2385675` | 无精确 tag，固定提交 |
| `depends/tinygltf` | `v2.9.7`，上游默认分支为 `release` | 固定稳定 tag + 推荐按 `release` 升级 |
| `reference/VSEngine2.1` | `origin/main` 上的固定提交 | 参考仓库，固定提交 |

### 推荐升级流程

#### tag 型依赖

适用于 `assimp`、`glm`、`rapidjson`、`lua`、`spdlog` 等。

1. 在子模块仓库中确认目标稳定 tag
2. 进入子模块后切换到目标 tag
3. 回到主仓库提交子模块指针变更
4. 运行构建与测试验证

示例：

```bash
git -C depends/rapidjson fetch --tags
git -C depends/rapidjson checkout v1.1.0
git add depends/rapidjson
```

#### 分支型依赖

适用于 `imgui`、`spine-runtimes`，以及后续如果按 `release` 维护的 `tinygltf`。

1. 先确认对应稳定分支仍是项目认可的升级来源
2. 拉取远端并在子模块内更新到目标提交
3. 不要直接盲目追到分支最新，优先选择经过验证的具体提交
4. 回到主仓库提交子模块指针变更

示例：

```bash
git -C depends/imgui fetch origin
git -C depends/imgui checkout origin/docking
git add depends/imgui
```

### 不推荐的做法

- 对所有子模块统一执行 `git submodule update --remote --recursive`
- 让子模块长期自动追随上游最新提交
- 仅依赖 `.gitmodules` 判断当前固定版本

### 说明

`.gitmodules` 主要用于记录**路径、URL、推荐分支**；真正生效的版本锁定信息仍然是主仓库记录的子模块 commit。
