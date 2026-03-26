---
alwaysApply: true
---
# DSEngine AI 注释与文档规范

## Description
DSEngine 项目的注释与文档标准，统一 C++(Doxygen)、Lua(LDoc) 和 TS(JSDoc) 的代码注释风格。

## Globs
*.h, *.cpp, *.lua, *.ts, *.tsx, *.md

## Rule Governance
- **规则集版本**: `v1.0.0`
- **生效日期**: `2026-03-26`
- **冲突优先级声明**: 本文件仅约束“注释与文档表达”；若与 `AI_DEVELOPMENT_RULES.md` 或 `AI_TESTING_RULES.md` 冲突，以高优先级规则为准。

## Content
### 1. 基本原则
- **拒绝机器水文**: 严禁生成诸如“执行 XXX 操作”、“参数说明”、“返回值说明”等毫无营养的机器废话注释。注释必须经过对上下文的真实语义理解后手工撰写。
- **全覆盖要求**: 引擎核心层（`engine/` 和 `modules/`）的**每一个接口和函数**（不论是 public 还是 protected/private 核心内部机制）都必须提供注释，绝不允许偷懒遗漏。
- **释“因”非“果”**: 重点解释复杂业务、特殊算法或设计妥协，对于函数实现内部的逻辑，不注释显而易见的代码。
- **格式化**: 所有函数、类、枚举等必须包含结构化注释。
  - **组件与字段释义**: 对于 ECS 的数据组件结构体（`struct`），必须使用行内注释语法（如 C++ 的 `///<`）对每一个成员字段的业务属性进行精确标注。
  - **代码示例**: 核心公共API（尤其是暴露给Lua的C++接口）必须在注释中提供简短的使用示例（如 `@example` 或 `@usage`）。
- **同步更新**: 修改代码时强制同步更新对应注释。
- **中文优先**: 代码注释与工程文档默认使用中文；若必须使用英文（如外部协议字段、第三方术语），需保证中文语义不缺失。
- **测试注释中文化**: 测试文件中的场景说明（如 `TEST_CASE` 上方说明文本）必须使用中文，明确测试意图、前置条件与预期行为。

### 2. 各语言规范
**C++ (engine/) - Doxygen风格**
- 文件头: `/** @file name.h @brief 功能简述 */`
- 类: 声明上方使用块注释，描述用途和注意事项。
- 函数: 包含参数`@param`、返回值`@return`及警告`@warning`。
- 行内: 双斜杠`//`，与代码留一空格。

**Lua (script/) - EmmyLua/LDoc风格**
- 模块: `--- @module Name`
- 函数:
  ```lua
  --- @param dt number 增量时间
  --- @return boolean
  ```
- 类/字段: `--- @class Name`, `--- @field id number`

**TypeScript/React (apps/) - JSDoc风格**
- 组件/Hook:
  ```tsx
  /**
   * 组件/Hook功能描述
   * @param props.xx 参数说明
   * @returns 返回值说明
   */
  ```

### 3. 文档与维护
- **Markdown维护**: 架构文档(如`Architecture.md`)为代码库真理源(SSOT)。架构变动时强制主动更新Markdown。
- **TODO标记规范**: 遇到阻碍或留下技术债时，必须使用标准化标记：`// TODO: [AI-YYYY-MM-DD] 描述` 或 `// FIXME: 待人工复核 - 描述`，以便全局检索。
- **排版**: 清晰层级(`#`, `##`)，合理使用加粗和代码块，使用Markdown链接`[文本](./路径)`跳转。
- **Git提交**: 采用 Conventional Commits (`feat:`, `fix:`, `docs:`, `refactor:` 等)。

## Version History
- `v1.0.0` (`2026-03-26`): 增加规则治理段，明确文档规范在冲突场景下的从属关系。
