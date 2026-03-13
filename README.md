# Dark Soul Engine (DSEngine)

DSEngine 是一个基于 C++17 和 Lua 的轻量级游戏引擎，支持 2D 和 3D 渲染、物理模拟、音频系统和组件化架构。

## 开发环境支持

### CLion (推荐)
DSEngine 完全支持 JetBrains CLion。
1.  **打开项目**: 直接在 CLion 中打开 `DSEngine` 根目录。
2.  **CMake 加载**: 等待 CLion 加载 `CMakeLists.txt`。
3.  **运行配置**:
    *   CLion 会自动创建名为 `DSEngine` 的 Run/Debug Configuration。
    *   默认情况下即可直接运行。
    *   引擎内置了路径探测机制，会自动定位 `data` 和 `script` 目录，无需手动设置 Working Directory。

### Visual Studio Code
1.  安装 **C/C++** 和 **CMake Tools** 插件。
2.  打开 `DSEngine` 文件夹。
3.  CMake Tools 会自动检测并配置项目。
4.  选择构建目标 `DSEngine` 并运行。

## 目录结构

*   `src/`: 引擎核心 C++ 源码
*   `script/`: 引擎核心 Lua 脚本
*   `examples/`: 示例工程和 Lua 逻辑脚本
*   `data/`: 游戏资源（图片、模型、音频等）
*   `depends/`: 第三方依赖库

## 构建指南

1.  确保已安装 CMake (3.17+) 和 C++ 编译器 (支持 C++17)。
2.  在项目根目录下创建构建目录：
    ```bash
    mkdir build
    cd build
    ```
3.  生成构建文件：
    ```bash
    cmake ..
    ```
4.  编译项目：
    ```bash
    cmake --build .
    ```

## 运行说明

构建完成后，建议在 **项目根目录** 或 **构建目录** 下运行生成的可执行文件。引擎会自动探测 `script`, `examples` 和 `data` 目录。

## 演进规划

查看 [ROADMAP.md](ROADMAP.md) 了解 DSEngine 如何演进为成熟的商业化 2D 引擎。

## 特性

*   **ECS 架构**: 灵活的组件系统。
*   **Lua 脚本**: 完整的 Lua 绑定，支持热重载（部分）。
*   **混合渲染**: 支持 2D Sprite 和 3D Mesh 渲染。
*   **物理引擎**: 集成 PhysX。
*   **音频系统**: 集成 FMOD。
*   **编辑器**: 内置基于 ImGui 的编辑器。
