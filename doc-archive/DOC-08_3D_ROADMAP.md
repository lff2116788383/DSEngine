# DOC-08 3D 说明（降级为参考文档）

本文档不再作为主线路线图文档维护，而是作为当前 3D 状态的参考说明。

当前 3D 的主线结论、后续路线和验收口径，统一以 `doc-archive/DOC-07_ROADMAP.md` 为准。

## 1. 当前定位

DSEngine 的 3D 能力当前应准确描述为：

- 已接入部分组件、模块、样例和测试入口
- 已具备继续推进 MVP 的基础
- 不是当前默认稳定主线
- 不应对外表述为完整商用品质 3D 引擎能力

## 2. 当前可确认的代码事实

当前仓库中可确认的 3D 相关事实包括：

- 存在 `engine/ecs/components_3d.h` 等 3D 组件定义
- 存在 `modules/gameplay_3d/` 目录下的 3D 模块代码
- 存在部分 3D 渲染、相机、动画、转向、地形、剔除等系统代码
- Mesh / Animator / Particle 等高价值系统已补充 `AssetManager` 注入边界，资源访问方式较前一阶段更统一
- 存在部分 3D 测试文件与样例入口
- 编辑器已接入一部分 3D 组件检视与 Gizmo 基础操作
- 顶层构建默认 `DSE_ENABLE_3D=OFF`

因此，当前 3D 更适合归类为：

- **已接入能力**
- **待收口能力**
- **MVP 规划方向**

而不是：

- 默认稳定主线
- 完整 3D 制作流
- 完整商业级 3D 工具链

## 3. 文档维护策略

从当前版本开始：

- 不再在本文件中维护独立的 3D 主路线图
- 3D 的优先级、验收条件、边界与阶段目标统一收敛到 [`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md)
- 3D 的阶段版本规划统一收敛到 [`doc-archive/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc-archive/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md)
- 如果未来 3D 真正进入稳定主线，再视情况恢复独立 3D 文档

## 4. 当前阅读建议

如果需要了解项目后续方向，优先阅读：

1. [`doc-archive/DOC-01_ARCHITECTURE.md`](doc-archive/DOC-01_ARCHITECTURE.md)
2. [`doc-archive/DOC-07_ROADMAP.md`](doc-archive/DOC-07_ROADMAP.md)
3. [`doc-archive/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md`](doc-archive/DOC-10_2D_AND_3D_MVP_VERSION_PLAN.md)

如果只想确认 3D 的当前口径，请记住一句话：

**3D 当前是已接入、资源链路正在收口，但仍未成为默认稳定主线的能力方向。**
