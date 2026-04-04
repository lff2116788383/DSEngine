# 国际化系统使用指南

## 概述

国际化（Localization）系统提供了完整的多语言支持，包括：
- 多语言文本管理
- 运行时语言切换
- 参数化文本
- RTL（从右到左）文本支持
- 字体管理和回退机制

---

## 快速开始

### 1. 初始化国际化系统

```cpp
#include "modules/gameplay_2d/localization/localization_system.h"
#include "modules/gameplay_2d/localization/font_manager.h"

using namespace dse::gameplay2d;

// 获取单例
LocalizationSystem& loc = LocalizationSystem::GetInstance();
FontManager& fm = FontManager::GetInstance();

// 加载语言配置
loc.LoadLanguage("en", "data/localization/en.json");
loc.LoadLanguage("zh", "data/localization/zh.json");
loc.LoadLanguage("ar", "data/localization/ar.json");

// 设置当前语言
loc.SetCurrentLanguage("en");
```

### 2. 配置文件格式（JSON）

**data/localization/en.json**
```json
{
  "ui": {
    "button": {
      "ok": "OK",
      "cancel": "Cancel",
      "submit": "Submit"
    },
    "label": {
      "welcome": "Welcome to DSEngine",
      "greeting": "Hello {name}"
    }
  },
  "game": {
    "level": "Level {level}",
    "score": "Score: {score}"
  }
}
```

**data/localization/zh.json**
```json
{
  "ui": {
    "button": {
      "ok": "确定",
      "cancel": "取消",
      "submit": "提交"
    },
    "label": {
      "welcome": "欢迎来到 DSEngine",
      "greeting": "你好 {name}"
    }
  },
  "game": {
    "level": "第 {level} 关",
    "score": "分数：{score}"
  }
}
```

### 3. 获取本地化文本

```cpp
// 获取简单文本
std::string ok_text = loc.GetText("ui.button.ok", "OK");

// 获取带参数的文本
std::unordered_map<std::string, std::string> params = {{"name", "Alice"}};
std::string greeting = loc.GetTextWithParams("ui.label.greeting", params, "Hello {name}");
// 结果："Hello Alice" 或 "你好 Alice"（取决于当前语言）
```

### 4. 运行时语言切换

```cpp
// 切换到中文
loc.SetCurrentLanguage("zh");

// 现在获取的文本将是中文
std::string ok_text = loc.GetText("ui.button.ok");
// 结果："确定"

// 切换回英文
loc.SetCurrentLanguage("en");
```

### 5. 监听语言变更

```cpp
// 注册语言变更回调
int callback_id = loc.OnLanguageChanged([](const std::string& new_language) {
    std::cout << "Language changed to: " << new_language << std::endl;
    // 更新 UI 文本
    UpdateUITexts();
});

// 注销回调
loc.UnregisterLanguageChangeCallback(callback_id);
```

---

## 字体管理

### 1. 注册字体

```cpp
// 注册默认字体
fm.RegisterFont("default", "data/fonts/Arial.ttf", 32);

// 注册中文字体
fm.RegisterFont("chinese", "data/fonts/SimHei.ttf", 32);

// 注册阿拉伯字体
fm.RegisterFont("arabic", "data/fonts/Arabic.ttf", 32);

// 设置默认字体
fm.SetDefaultFont("default");
```

### 2. 为语言设置字体

```cpp
// 为中文设置中文字体
fm.SetFontForLanguage("zh", "chinese");

// 为阿拉伯语设置阿拉伯字体
fm.SetFontForLanguage("ar", "arabic");

// 获取语言对应的字体
std::string font_id = fm.GetFontForLanguage("zh");
// 结果："chinese"
```

### 3. 字体回退机制

```cpp
// 添加字体回退链
fm.AddFontFallback("chinese", "default");

// 如果中文字体缺少某个字符，将使用默认字体
// 这对于处理多语言混合文本很有用
```

---

## RTL（从右到左）文本支持

### 1. 检测文本方向

```cpp
// 检测英文文本
TextDirection dir = loc.DetectTextDirection("Hello World");
// 结果：TextDirection::LTR

// 检测阿拉伯文本
dir = loc.DetectTextDirection("مرحبا");
// 结果：TextDirection::RTL
```

### 2. 检查语言是否为 RTL

```cpp
bool is_rtl = loc.IsRTLLanguage("ar");  // true
is_rtl = loc.IsRTLLanguage("he");      // true (Hebrew)
is_rtl = loc.IsRTLLanguage("en");      // false
```

### 3. 在 UI 中使用 RTL

```cpp
// 获取当前语言
std::string current_lang = loc.GetCurrentLanguage();

// 检查是否为 RTL 语言
if (loc.IsRTLLanguage(current_lang)) {
    // 调整 UI 布局为 RTL
    ui_element.SetTextDirection(TextDirection::RTL);
    ui_element.SetAlignment(UIAlignment::Right);
} else {
    // 调整 UI 布局为 LTR
    ui_element.SetTextDirection(TextDirection::LTR);
    ui_element.SetAlignment(UIAlignment::Left);
}
```

---

## 参数化文本

### 1. 基本用法

```cpp
// 配置文件中定义参数化文本
// "greeting": "Hello {name}, you have {count} messages"

std::unordered_map<std::string, std::string> params = {
    {"name", "Alice"},
    {"count", "5"}
};

std::string text = loc.GetTextWithParams("greeting", params);
// 结果："Hello Alice, you have 5 messages"
```

### 2. 复杂参数

```cpp
// 支持任意参数名称
std::unordered_map<std::string, std::string> params = {
    {"player_name", "Hero"},
    {"level", "10"},
    {"experience", "5000"},
    {"gold", "1000"}
};

std::string text = loc.GetTextWithParams(
    "level_up",
    params,
    "{player_name} reached level {level}! Gained {experience} exp and {gold} gold."
);
```

---

## 完整示例

### 多语言 UI 系统

```cpp
class LocalizedUILabel {
private:
    std::string text_key_;
    std::unordered_map<std::string, std::string> params_;
    
public:
    LocalizedUILabel(const std::string& key) : text_key_(key) {}
    
    void SetParameter(const std::string& param_name, const std::string& param_value) {
        params_[param_name] = param_value;
    }
    
    std::string GetDisplayText() {
        LocalizationSystem& loc = LocalizationSystem::GetInstance();
        return loc.GetTextWithParams(text_key_, params_);
    }
    
    void OnLanguageChanged() {
        // 重新获取文本
        std::string new_text = GetDisplayText();
        // 更新 UI 显示
        UpdateDisplay(new_text);
    }
};

// 使用示例
int main() {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    
    // 加载语言
    loc.LoadLanguage("en", "data/localization/en.json");
    loc.LoadLanguage("zh", "data/localization/zh.json");
    
    // 创建本地化标签
    LocalizedUILabel label("ui.label.greeting");
    label.SetParameter("name", "Alice");
    
    // 获取英文文本
    loc.SetCurrentLanguage("en");
    std::cout << label.GetDisplayText() << std::endl;  // "Hello Alice"
    
    // 获取中文文本
    loc.SetCurrentLanguage("zh");
    std::cout << label.GetDisplayText() << std::endl;  // "你好 Alice"
    
    return 0;
}
```

---

## 最佳实践

### 1. 组织文本键

使用分层的键结构，便于管理和查找：
```
ui.button.ok
ui.button.cancel
ui.label.welcome
game.level
game.score
error.network_error
error.file_not_found
```

### 2. 提供默认文本

始终为 `GetText()` 和 `GetTextWithParams()` 提供默认文本：
```cpp
std::string text = loc.GetText("unknown.key", "Default Text");
```

### 3. 避免硬编码文本

所有用户可见的文本都应该通过国际化系统获取：
```cpp
// 不好
ui_label.SetText("OK");

// 好
ui_label.SetText(loc.GetText("ui.button.ok", "OK"));
```

### 4. 测试多语言

在开发过程中测试所有支持的语言：
```cpp
for (const auto& lang : loc.GetAvailableLanguages()) {
    loc.SetCurrentLanguage(lang);
    // 测试 UI 和文本显示
}
```

### 5. 处理缺失的翻译

使用默认文本作为后备：
```cpp
std::string text = loc.GetText("ui.button.ok");
if (text.empty()) {
    text = "OK";  // 使用英文作为后备
}
```

---

## 常见问题

### Q: 如何支持新的语言？
A: 创建新的 JSON 配置文件，然后使用 `LoadLanguage()` 加载即可。

### Q: RTL 文本如何正确显示？
A: 使用 `DetectTextDirection()` 或 `IsRTLLanguage()` 检测文本方向，然后调整 UI 布局。

### Q: 如何处理缺失的翻译？
A: 始终提供默认文本，系统会在翻译缺失时使用默认文本。

### Q: 性能如何？
A: 文本查询是 O(1) 操作（哈希表查找），性能很好。

---

## API 参考

### LocalizationSystem

| 方法 | 说明 |
|------|------|
| `LoadLanguage()` | 加载语言配置文件 |
| `SetCurrentLanguage()` | 设置当前语言 |
| `GetCurrentLanguage()` | 获取当前语言 |
| `GetAvailableLanguages()` | 获取所有可用语言 |
| `GetText()` | 获取本地化文本 |
| `GetTextWithParams()` | 获取带参数的本地化文本 |
| `DetectTextDirection()` | 检测文本方向 |
| `IsRTLLanguage()` | 检查语言是否为 RTL |
| `OnLanguageChanged()` | 注册语言变更回调 |
| `UnregisterLanguageChangeCallback()` | 注销语言变更回调 |
| `Clear()` | 清空所有数据 |

### FontManager

| 方法 | 说明 |
|------|------|
| `RegisterFont()` | 注册字体 |
| `LoadFont()` | 加载字体 |
| `UnloadFont()` | 卸载字体 |
| `GetFont()` | 获取字体资产 |
| `SetDefaultFont()` | 设置默认字体 |
| `GetDefaultFont()` | 获取默认字体 |
| `SetFontForLanguage()` | 为语言设置字体 |
| `GetFontForLanguage()` | 获取语言对应的字体 |
| `AddFontFallback()` | 添加字体回退 |
| `GetFontFallbacks()` | 获取字体回退链 |
| `GetAllFontIds()` | 获取所有字体 ID |
| `Clear()` | 清空所有字体 |
