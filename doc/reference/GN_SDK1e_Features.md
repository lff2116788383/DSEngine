# GN_SDK1e 游戏引擎 SDK 功能梳理

基于对 GN_SDK1e 的头文件类和结构体定义的分析，该引擎 SDK 提供了非常全面且成熟的 2D/3D 游戏开发功能。以下是按功能模块分类的详细梳理：

## 1. 核心基础模块 (Core & Base)
- **系统与初始化**: 引擎的初始化配置 (`GN_sINIT`)，线程结构 (`GN_sTHREAD`)，进程信息记录，系统底层基础结构封装。
- **内存与数据结构**:
  - 高效数组与链表 (`sARRAY`, `sLIST`, 循环列表 `sLIST_WHEEL` 等)。
  - 内存池与用量管理器 (`cMEM_POOL`, `cVOLUME_MAG` 等)，自动增长缓冲映射。
  - 高速处理临时缓冲 (`sTBUF`, `sTBUF_MT`, `sTBUF_MAG`)，专门用于局部临时大量空间分配。
  - 树形数据结构 (`sNODE`, `cTREE2`) 与线程安全的智能指针机制 (`P`, `P_`)。
- **并行处理 (Parallel)**: 提供高性能并行计算与高效的简短子任务运行调度机制 (`GN_cPARALLEL`, `GN_sTIME_EXEC_TASK`)。
- **文件与格式处理**:
  - 文件流加载与操作管理 (`sFILE`, `sFILE_INFO`)。
  - 音频与图像文件插件扩展接口 (`iFILE_AUDIO`, `iFILE_IMAGE`)，以及 ZIP 压缩文件处理 (`cZIP`)。
- **数学与辅助工具**:
  - 颜色类型支持（浮点型 `sCOLOR4F` 与 24位整型 `sCOLOR3`）。
  - 插值系统与贝塞尔曲线计算 (`GT_sVALUE_CHANGE`, `sBEZIER`)。
  - 动画时间轴控制 (`sAM_CTRL`) 与运行信息记录器 (`GT_cRUN_REC`)。

## 2. 2D 图形与场景模块 (2D Graphics)
- **2D 视口与场景**:
  - 2D 场景管理 (`cSCENE2D`)，2D 视口 (`cVIEW2D`) 支持多子视图、画面缩放比例和坐标映射转换。
  - 图形显示窗口抽象 (`sWINDOW`)。
  - 2D 对象与扩展功能 (`cOBJ2D`, `iOBJ2D_EXT`)，支持对象的层级关系管理 (`cOBJPTR`)。
- **2D 数学与图元**: 
  - 2D 向量 (`sVEC2`)，2D 变换结构 (`sTRANSF2D`)。
  - 丰富的 2D 基础图元：矩形 (`sRECT`, `sRECT2`)，圆形 (`sSPHERE2D`)，三角形及插值 (`sTRI2D`)，平行四边形 (`sQUADS2`) 等。
- **图像与渲染**:
  - 图像装载与底层管理 (`sIMAGE`, `sIMAGE_OBJ`)，支持图像组渲染。
- **文字与排版**:
  - 艺术文字输出器 (`cTEXT_ART`, `cTEXT_ARTEX`)，支持标签文本解析及复杂的绘制排列控制。

## 3. 3D 图形与场景模块 (3D Graphics)
- **3D 视口与场景**:
  - 3D 视口 (`cVIEW3D`) 与摄像机镜头控制 (`cCAMERA`)。
  - 3D 场景渲染与信息维护 (`cSCENE`, `sSCENE_INFO`)，支持高度图空间分割。
- **3D 对象与模型**:
  - 3D 对象基类与树形结构 (`cOBJ3D`)，提供骨骼、群组等功能扩展。
  - 3D 模型 (`cMODEL`)，轮廓线段 (`cSHAPE`)，网格渲染与蒙皮权重计算 (`GN3d_sMESH`, `sSKIN`, `sWEIGHT`)。
- **材质与贴图**:
  - 材质基本颜色与嵌套材质处理 (`sMATERIAL`, `sMATERIALEX`)。
  - 贴图采样、嵌套贴图机制及光照贴图 (`sTEXMAP`, `sTEXTURE`, `sLITMAP`)。
- **光照与特效**:
  - 3D 灯光与风场结构模拟 (`cLIGHT`, `cWIND`)。
  - 渲染特效处理（如屏幕散射、光照遮挡查询等）及渲染质量控制建议 (`sRENDER_HINT`)。
- **3D 动画系统**:
  - 各种关键帧动画集：变换、材质、贴图坐标、镜头、灯光等关键帧集 (`sAM_TRANSF_KEYS`, `sAM_MATL_KEYS` 等)。
  - 融合形状变形动画 (`sAM_BLENDSHAPE`, `GN3d_sBLENDSHAPE`) 及变形更新器。
- **3D 数学与空间计算**:
  - 3D 向量与齐次坐标 (`sVEC3`, `sVEC4`)，矩阵运算 (`sMAT22`, `sMAT33`, `sMAT44`)。
  - 3D 图元：射线 (`sRAY3D`)，平面 (`sPLANE3D`)，胶囊体 (`sCAPSULE`)。
  - 空间区域判定：透视区域、四边形区域等。

## 4. UI 界面系统 (User Interface)
- **基础 UI 组件**:
  - UI 组件基类 (`iUI_OBJECT`, `cUI_NEAR_OBJECT`, `cUI_GROUP`)，UI 场景管理 (`cUI_SCENE`)。
  - 静态图像 (`cUI_IMAGE`)，底层边框 (`cUI_BACK_FRAME`)，静态文本 (`cUI_TEXT_ART`)。
  - 排列与布局控制 (`sARRANGE_FMT`, `sDRAW_IN_AREA`)。
- **高级 UI 控件**:
  - 滚动列表与控制器 (`cSCROLL_RECT`, `cSCROLL_LIST`, `cSCROLL_LIST_GUI`)。
  - 树形滚动列表 (`cTREE_VIEW`)，支持动态增删重排及自定义树节点操作（如 `cTREE_NODE_OPTR`）。
  - 选择与交互控件：文本/图标选择列表 (`cSELECT_LIST_TEXT`)，按钮联与分页栏 (`cSELECT_BUTTON`, `cPAGE_BAR`)，滑动条 (`cSLIDER`)，颜色板 (`cPALETTE`)。
  - 文本输入框 (`cTEXT_INPUT`, `cUI_TEXT_INPUT`)。
  - 完整的文件浏览器 UI (`cFILE_BROWSER`)。
  - 弹出式简单对话框与菜单 (`cSIM_DIALOG`, `sSIM_MENU`)。
- **虚拟输入 UI**:
  - 触屏虚拟按钮 (`cBUTTON_TOUCH`)，虚拟操控杆 (`cJOYSTICK`)。
  - 漫画对话气泡效果 (`sBUBBLE`)。

## 5. 动画图框与特效模块 (Animation & Effects)
- **动画图框**:
  - 基础正规动画图框 (`cFRAME_RC`) 与支持复杂旋转效果的动画图框 (`cFRAME`)。
- **发射器系统 (粒子特效类)**:
  - 图框发射器 (`cFRAME_LAUNCHER`)，用于随机产生大量相同动画图框（实现类似粒子系统的画面丰富效果）。
  - 复合效果发射器 (`cFRAME_LAUNCHER2D`, `cFRAME_LAUNCHER3D`)。
  - 动画艺术字框发射器 (`cFRAME_LAUNCHER_FONT`)，带触摸检测，可用于炫酷的 UI 排列或数字跳动效果。
- **镜头光效**:
  - 镜头光环/光晕效果计算与渲染 (`cLENS_FLARE`)。

## 6. 网络与通信模块 (Network)
- **底层网络协议**:
  - 传统 Socket 封装，提供直接的 TCP 与 UDP 协议支持类 (`cSOC_TCP`, `cSOC_UDP`, `cSERVER_TCP`, `cSERVER_UDP`)。
- **HTTP 与 Web 支持**:
  - 完整的 HTTP 协议访问客户端 (`cCLIENT_HTTP`)，支持 HTTP POST (`sHTTP_POST`) 与 URL 资源分节下载 (`GT_sURL_LOAD`)。
- **数据传输与文件**:
  - 专用的传输器用于发送大数据包 (`cDTP`)，以及数据包收发任务调度系统 (`sPACKET_TASK`)。
  - 网络文件操作客户端 (`cFTM_CLIENT`)，支持登录用户与目录索引。

## 7. 音频与媒体模块 (Audio & Media)
- **声音系统**:
  - 声音基本对象与空间发声源点封装 (`sSOUND`, `cSOUND_SRC`)。
  - 音频流播放器 (`GT_cSOUND_STREAM`)，底层使用双缓冲实现平滑流播放。
  - 专用的背景 BGM 音乐播放器 (`cBGM_MUSIC`)。
- **多媒体支持**:
  - 视频流播放器 (`cVIDEO`)。

## 8. 输入动作系统 (Input)
- **输入动作捕获**:
  - 输入动作基本对象定义 (`sOBJ`)。
  - 高级特定动作识别：如鼠标长按不动 (`sMOUSE_DOWN_SKIP`)、鼠标按住摇晃动作 (`sMOUSE_DOWN_SHAKE`)。

## 9. 编辑器与工具模块 (Tools & Editor)
- **场景外部处理**:
  - 场景外部设置与资源处理方法集 (`GP_sSCD_EXT`)，支持在编辑器中对引擎场景的加载和处理 (`sSCENE_LOAD_CONFIG`)。
- **编辑器插件支持**:
  - 参数设置菜单插件接口 (`iSETTING_MENU`) 及其图像、文本样式、排列等各类初始化参数结构。
