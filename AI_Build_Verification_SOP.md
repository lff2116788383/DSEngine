# DSEngine AI 编译与验证标准规范 (AI Build & Verification SOP)

## 1. 核心目标
为 AI 编程助手提供在修改 `DSEngine` 代码后，进行标准化构建、运行和验证的明确指令集。AI 在每次完成代码（C++、CMake 或 Lua）的修改后，**强烈建议/必须**遵循此规范进行自我闭环验证，确保交付的代码是“编译通过且可运行”的。

## 2. 环境与前置要求
- **操作系统**: Windows
- **Shell 环境**: PowerShell 5.x（在使用 `RunCommand` 时，务必使用 PowerShell 兼容语法，禁止使用纯 bash 特有指令）
- **构建系统**: CMake
- **编译器**: Visual Studio 17 2022 (MSVC x64)
- **工作目录**: 项目根目录（当前为 `c:\Users\wenbilin\Desktop\Engine\DSEngine`），执行命令前需确认 `cwd` 为该路径。

---

## 3. 标准编译流水线 (Build Pipeline)

### 3.1 步骤一：配置 CMake 工程 (Configure)
**适用场景**：新增/删除源文件、修改 `CMakeLists.txt` 或首次全新构建时。
**执行命令**：
```powershell
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64
```
> **注意**：如果遇到严重的 CMake 缓存异常或工程结构错乱，可先使用 PowerShell 指令清理目录后再生成：
> `If (Test-Path build_vs2022) { Remove-Item -Recurse -Force build_vs2022 } ; cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64`

### 3.2 步骤二：执行编译 (Build)
**适用场景**：日常修改 `.cpp` 或 `.h` 代码后的增量编译。
**执行命令**（推荐直接全量编译所有 Target）：
```powershell
cmake --build build_vs2022 --config Debug
```
> **按需编译特定目标 (可选)**：
> - 仅编译引擎核心库：`cmake --build build_vs2022 --config Debug --target dse_engine`
> - 仅编译 C++ 示例：`cmake --build build_vs2022 --config Debug --target DSEngine_example_cpp`
> - 仅编译 Lua 示例：`cmake --build build_vs2022 --config Debug --target dse_example_lua`

---

## 4. 验证与测试标准 (Verification Standards)

代码编译通过不代表逻辑正确，AI 应进一步执行运行验证。

### 4.1 核心可执行文件运行验证
**执行命令**：
使用 `RunCommand` 工具运行生成的测试宿主程序：
```powershell
# 运行 C++ 示例
& .\build_vs2022\examples\c++\Debug\DSEngine_example_cpp.exe

# 运行 Lua 示例
& .\build_vs2022\examples\lua\Debug\DSEngine_lua.exe
```

### 4.2 验证通过的验收标准 (Acceptance Criteria)
1. **进程退出码**：`Exit Code` 必须为 `0`。如果程序 Crash（段错误、空指针等），退出码通常不为 0。
2. **日志无致命异常**：检查命令行输出，确保没有未捕获的 `[Error]` 或 `[Fatal]` 级别引擎报错。
3. **资源无泄漏**：如果是带有回归模式的执行，需关注生命周期收尾阶段的内存与资源台账输出是否平衡。

---

## 5. AI 错误排查与修复 SOP (Troubleshooting Loop)

当 AI 在执行第 3 节（编译）或第 4 节（运行）遇到失败时，**必须**执行以下自闭环修复流程：

1. **提取精确报错**：若构建失败，使用 `CheckCommandStatus` 获取终端日志，寻找 `error CXXXX` (MSVC 编译错误) 或 `LNKXXXX` (链接错误) 对应的具体文件与行号。
2. **溯源与分析**：利用 `Read` / `Grep` 定位错误上下文，如果是接口变更导致，需排查头文件声明与源文件实现的匹配关系。
3. **实施修复**：使用 `SearchReplace` 或其他文件编辑工具修正代码。
4. **重新触发流水线**：修复后，必须从 `3.2 步骤二 (Build)` 重新开始验证。
5. **严禁半途而废**：**决不允许**在编译或运行依然报错的状态下直接交付结果给用户，直到验证全链路通过，或者遇到无法获取上下文的外部库缺失等阻断性问题时，才能向用户如实报告并寻求帮助。