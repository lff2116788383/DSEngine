# Editor Fonts

此目录存放编辑器所需的字体文件（不提交到 Git，需手动下载）。

## 所需文件

| 文件名 | 来源 | 用途 |
|--------|------|------|
| `Inter-Regular.ttf` | [rsms/inter](https://github.com/rsms/inter) (OFL) | 主 UI 字体 16px |
| `Inter-Bold.ttf` | 同上 | 标题/强调 |
| `NotoSansSC-Regular.ttf` | [Google Noto CJK](https://github.com/notofonts/noto-cjk) (OFL) | 中文 fallback |
| `MaterialDesignIcons-Webfont.ttf` | [Templarian/MDI](https://github.com/Templarian/MaterialDesign-Webfont) (Apache 2.0) | 图标字体 |

## 快速下载

```powershell
cd apps/editor_cpp/fonts
powershell -ExecutionPolicy Bypass -File download_fonts.ps1
```

## 手动下载

1. **Inter**: https://github.com/rsms/inter/releases → 解压取 `Inter-Regular.ttf` 和 `Inter-Bold.ttf`
2. **Noto Sans SC**: https://fonts.google.com/noto/specimen/Noto+Sans+SC → 下载 Regular 权重
3. **MDI Webfont**: https://cdn.jsdelivr.net/npm/@mdi/font@latest/fonts/materialdesignicons-webfont.ttf

将文件放入本目录即可，编辑器启动时自动加载。如缺少字体文件，编辑器仍可启动（使用默认字体）。
