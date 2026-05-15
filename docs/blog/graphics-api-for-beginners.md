# 图形 API——OpenGL、Vulkan、Direct3D、Metal…… 都是啥？

> **灵魂拷问：你的显卡是 RTX 4090，但游戏是怎么"告诉"显卡该画什么的？**
>
> 显卡自己听不懂"给我画一棵树"这种话。游戏需要通过一种**标准化的"语言"**来和显卡沟通——这种语言就是**图形 API（Graphics API）**。但市面上有好几种这样的"语言"——OpenGL、Vulkan、Direct3D 11/12、Metal、WebGPU……它们之间到底有什么区别？

---

## 目录

1. [什么是图形 API？—— 游戏和显卡之间的"翻译官"](#一什么是图形-api--游戏和显卡之间的翻译官)
2. [OpenGL + OpenGL ES——跨平台的"老大哥"](#二opengl--opengl-es跨平台的老大哥)
3. [Direct3D 11——Windows 上的"老将"](#三direct3d-11windows-上的老将)
4. [Direct3D 12——D3D11 的"激进升级版"](#四direct3d-12d3d11-的激进升级版)
5. [Vulkan——跨平台的"效率狂魔"](#五vulkan跨平台的效率狂魔)
6. [Metal——苹果的"自家看门人"](#六metal苹果的自家看门人)
7. [WebGPU + WebGL——浏览器里的图形世界](#七webgpu--webgl浏览器里的图形世界)
8. [一张表看懂所有图形 API](#八一张表看懂所有图形-api)
9. [游戏引擎的 RHI 抽象层：一次编写，多端运行](#九游戏引擎的-rhi-抽象层一次编写多端运行)
10. [一句话总结](#十一🔞句话总结)

---

## 一、什么是图形 API？—— 游戏和显卡之间的"翻译官"

### 没有图形 API 的世界

想象一下，如果没有统一的图形 API：

```
游戏开发者要面对的恐怖场景：

  每张显卡的"语言"都不一样：
    NVIDIA RTX 4090：说"方言 A"
    AMD RX 7900 XTX：说"方言 B"
    Intel Arc A770：说"方言 C"
    Apple M3：说"方言 D"
    Qualcomm 手机 GPU：说"方言 E"
    ……
    
  游戏要为每种显卡写不同的代码？
  → 不可能，显卡型号成百上千种
```

### 图形 API 就是"普通话"

**图形 API（Application Programming Interface）** 就是统一的标准语言：

```
游戏 ──→ [图形 API] ──→ [显卡驱动] ──→ [显卡硬件]

游戏只用学一种"语言"
显卡驱动负责把 API 指令翻译成显卡能懂的"方言"
```

### 图形 API 负责什么

```
图形 API 的职责：

  ① 告诉显卡"画什么"（顶点数据、模型）
  ② 告诉显卡"怎么画"（着色器、渲染状态）
  ③ 告诉显卡"画到哪"（窗口、屏幕）
  ④ 管理显存（纹理、缓冲区的分配和释放）
  ⑤ 管理 GPU 的命令执行（什么时候开始画、什么时候画完）
```

### 为什么有这么多不同的 API？

```
各种图形 API 的"版图"：

  不同平台有自己的"方言习惯"
  不同年代有不同的设计理念
  不同厂商想要不同的控制权

  所以最终没有"统一天下"的 API
  而是形成了几个"诸侯国"各占一方
```

---

## 二、OpenGL + OpenGL ES——跨平台的"老大哥"

### OpenGL 的身世

**OpenGL（Open Graphics Library）** 诞生于 1992 年——比《Doom》还早一年。它由 Khronos 组织管理，是**最早被广泛采用的跨平台图形 API**。

### 覆盖范围

```
OpenGL 的版图：

  台式机/笔记本：Windows、Linux、macOS（已停止更新）
  移动端/嵌入式：OpenGL ES（ES = Embedded Systems）
    → Android 手机、iPhone（早期）、游戏机（Nintendo Switch）
    → 电视、车载系统、智能手表……
  
  OpenGL ES 是 OpenGL 的"精简版"
  去掉了桌面 GPU 上不需要的部分
  针对移动设备省电优化
```

### OpenGL 的特点

```
OpenGL 的核心特点：

  ✅ 跨平台 —— 几乎所有操作系统都支持
  ✅ 学习资源最多 —— 30 年积累的教程、书籍、社区
  ✅ 简单上手 —— "状态机"模式，几十行代码画三角形
  ✅ OpenGL ES 统治移动端 —— Android 上最主流的 API

  ❌ 设计陈旧 —— 1992 年的设计理念，有些地方过时了
  ❌ CPU 开销高 —— 驱动做了大量安全检查
  ❌ 多线程支持差 —— 状态机设计天生不适合多线程
  ❌ macOS 已淘汰 —— Apple 从 2018 年起不再支持新版本
```

### OpenGL 的工作方式：状态机

```cpp
// OpenGL 的"状态机"风格
glEnable(GL_DEPTH_TEST);        // 开启深度测试（一个"开关"）
glBindTexture(GL_TEXTURE_2D, tex); // 绑定纹理
glDrawArrays(GL_TRIANGLES, 0, 3); // 画三角形
// 状态一直被"记住"，直到你再次修改它
```

**优点：** 代码直观，容易理解。
**缺点：** 状态是"全局的"，多线程下互相干扰。

### 现状

```
OpenGL / OpenGL ES 在 2024 年的地位：

  ✅ 教学首选 —— 几乎所有图形学课程从 OpenGL 教起
  ✅ Android 开发 —— OpenGL ES 仍然是主流，正在被 Vulkan 取代
  ✅ 嵌入式设备 —— 轻量级图形渲染的首选
  ❌ AAA 游戏 —— 基本被 Vulkan 和 D3D12 取代
  ❌ macOS/iOS —— 已被 Apple 的 Metal 全面取代
```

---

## 三、Direct3D 11——Windows 上的"老将"

### Direct3D 的身世

**Direct3D（简称 D3D）** 是微软开发的图形 API，是 **DirectX** 套件的一部分。1995 年诞生，和 Windows 系统深度绑定。

### Direct3D 11 的特点

```
Direct3D 11 的核心特点：

  ✅ Windows 亲儿子 —— 和 Windows 系统配合最完美
  ✅ Xbox 也在用 —— 微软游戏主机用的也是 D3D
  ✅ 驱动质量好 —— 微软对驱动有严格的认证流程
  ✅ 开发工具强 —— Visual Studio + PIX 调试工具
  ✅ 比 OpenGL 更高效 —— 对象接口设计更现代

  ❌ 仅限 Windows/Xbox —— 不跨平台
  ❌ 版本分裂 —— D3D11 / D3D12 是两套完全不同的 API
  ❌ 微软主导 —— 不开放，由微软一家说了算
```

### D3D11 的设计理念

> **D3D11 走的是"中间路线"——不像 OpenGL 那样驱动包办一切，也不像 Vulkan/D3D12 那样让你自己管一切。**

```
OpenGL          D3D11             Vulkan / D3D12
  │               │                   │
  ↓               ↓                   ↓
驱动管得多     你管一部分          你管全部
上手容易       性能不错            性能最强
上限不高       上限中等            学习成本极高
```

### 现状

```
Direct3D 11 在 2024 年的地位：

  ✅ Windows 游戏的"底线兼容"——几乎所有 Windows PC 都支持
  ✅ 跨代兼容 —— 从 2009 年到现在的显卡都能跑
  ✅ 大量现有游戏在用 —— Steam 上超半数的游戏仍用 D3D11
  ❌ 新一代游戏逐步转向 D3D12 —— 为了榨干硬件性能
```

---

## 四、Direct3D 12——D3D11 的"激进升级版"

### D3D12 是什么？

**Direct3D 12** 是微软在 2015 年推出的**底层图形 API**——它和 D3D11 虽然名字都叫"Direct3D"，但完全是两套不同的 API。

### D3D12 的核心变化

> **D3D12 做了和 Vulkan 一样的事：把控制权从驱动手里夺回来，交给开发者。**

```
D3D11（上层 API）：

  游戏 → "帮我画这个物体"
  驱动 → 内部管理内存分配
       → 内部管理命令排队
       → 内部管理同步
       → 内部管理资源状态
       
  ✅ 开发者省心
  ❌ 驱动做了很多"你可能不需要的工作"

D3D12（底层 API）：

  游戏 → "我分配好了内存，地址是 xxx"
       → "我录好了命令，队列是 yyy"
       → "我设好了同步信号，等 zzz"
  驱动 → ……好，照做
  
  ✅ 开发者可以针对游戏精确优化
  ❌ 开发者要自己管理大量底层细节
```

### D3D12 的关键特性

```
① 更少的 CPU 开销
  
  D3D11：每个 Draw Call 约 0.05ms CPU 时间
  D3D12：每个 Draw Call 约 0.005ms CPU 时间
  
  差 10 倍！
  这意味着用 D3D12 可以画出多 10 倍的物体

② 多线程录制
  
  D3D11：命令录制只能在主线程
  D3D12：可以在多个线程同时录制命令
  → 充分利用多核 CPU

③ 显存管理
  
  D3D11：驱动帮你管理显存分配和回收
  D3D12：你自己管理，精确控制什么时候上传、什么时候回收
```

### D3D12 的"捆绑包"（Bundle）技术

```
D3D12 有一个很有特色的技术叫"捆绑包"：

  场景：同一棵树在场景中出现了 1000 次
  
  传统方式：
    画 1000 次 → CPU 发出 1000 次 Draw Call
  
  D3D12 捆绑包：
    预录制一棵树的绘制命令 → 存成"捆绑包"
    画 1000 次 → 引用同一个"捆绑包"1000 次
    
  CPU 开销从 1000 次 → 几乎为 0
```

### 现状

```
Direct3D 12 在 2024 年的地位：

  ✅ Xbox Series X/S 原生使用 —— 主机游戏的标配
  ✅ AAA Windows 游戏 —— 越来越多新作使用
  ✅ 光线追踪支持 —— D3D12 是 Windows 上光线追踪的入口
  ❌ 仅限 Windows 10+ / Xbox —— 不跨平台，不兼容旧系统
```

---

## 五、Vulkan——跨平台的"效率狂魔"

### Vulkan 的身世

**Vulkan** 由 Khronos 组织管理——和 OpenGL 同一个"妈"。但它不是 OpenGL 的升级版，而是**完全重写的、全新的底层图形 API**。

Vulkan 的设计目标：**做到 D3D12 能做的事，但跨平台。**

### Vulkan 的特点

```
Vulkan 的核心特点：

  ✅ CPU 开销极低 —— 比 OpenGL 少 50%~90%
  ✅ 多线程友好 —— 天生支持多线程录制命令
  ✅ 跨平台 —— Windows、Linux、Android、Nintendo Switch
  ✅ 精细控制 —— 开发者自己管理显存、同步、命令提交
  ✅ 开源标准 —— 不是任何一家公司独有

  ❌ 学习曲线陡峭 —— 画个三角形要几百行代码
  ❌ 开发效率低 —— 什么都要自己管，容易出错
  ❌ 驱动差异大 —— 不同厂商的 Vulkan 驱动质量参差不齐
  ❌ 生态系统碎片化 —— 各平台支持程度不一
```

### Vulkan vs D3D12

```
两者都是底层 API，理念非常相似：

  Vulkan                          D3D12
  ──────                          ─────
  跨平台                          Windows/Xbox 独占
  Khronos 组织管理                微软管理
  任何厂商都可以参与              微软一家说了算
  桌面 + 移动 + 主机             PC + Xbox
  学习资源相对少                  微软官方文档详尽

性能上：
  两者基本持平
  在 Windows 上 D3D12 略优（微软自家优化）
  在 Android/Linux 上 Vulkan 是唯一选择
```

### 用生活场景理解

🍳 **做饭的三种方式：**

```
OpenGL = 去餐厅点菜
  你说"番茄炒蛋" → 厨师全包
  省心，但不能控制细节

D3D11 = 半成品料理包
  你负责加热和装盘
  比点菜更灵活，但大部分还是别人做好了

Vulkan / D3D12 = 自己下厨
  买菜、洗菜、切菜、炒菜、洗碗全自己来
  最累，但每一道工序都能精确控制
  想少放盐？想多放辣？随你
```

### 现状

```
Vulkan 在 2024 年的地位：

  ✅ Android 新设备标配 —— 高端 Android 手机都支持 Vulkan
  ✅ PC 跨平台引擎 —— Unity/Unreal 的默认高性能后端
  ✅ Nintendo Switch —— 主机的图形 API 就是 Vulkan
  ✅ Linux 游戏 —— Proton（Steam Deck）通过 Vulkan 运行 Windows 游戏
  ❌ 学习门槛高 —— 开发者稀缺，招聘难度大
```

---

## 六、Metal——苹果的"自家看门人"

### Metal 是什么？

**Metal** 是 Apple 在 2014 年推出的图形 API——**只用于苹果生态**：iPhone、iPad、Mac、Apple TV。

### 为什么 Apple 要自己做一套？

```
Apple 的逻辑：

  2014 年以前：
    iPhone/iPad → OpenGL ES
    Mac → OpenGL
    
  但 OpenGL 不是 Apple 自己控制的：
    标准由 Khronos 组织决定
    Apple 想加新功能？得等 Khronos 投票
    进度太慢了
    
  Apple 的决定：
    与其等别人，不如自己来
    → 2014 年推出 Metal
    → 2018 年正式宣布废弃 OpenGL
```

### Metal 的特点

```
Metal 的核心特点：

  ✅ 深度集成 —— 和 Apple 的芯片（A 系列 / M 系列）绑定的最优实现
  ✅ 低开销 —— 底层 API，和 Vulkan/D3D12 一个级别
  ✅ 开发工具一流 —— Xcode 的 GPU Debugger 是业界最好的
  ✅ 统一架构 —— iOS / macOS / tvOS 用同一套 API
  ✅ CPU → GPU 无缝 —— Apple 芯片的统一内存设计

  ❌ 仅限苹果生态 —— 出不了 Apple 的"围墙花园"
  ❌ 闭源标准 —— 完全由 Apple 控制
  ❌ 市场份额小 —— 全球游戏市场占比不高
  ❌ 不支持 Windows —— PC 玩家用不了
```

### Metal 的统一内存

```
Apple Silicon（M1/M2/M3/M4）有一个独特优势：

传统 PC：
  CPU 内存（DDR5）←→ PCIe 总线 ←→ 显存（GDDR6）
  
  GPU 要读取 CPU 内存中的数据 → 需要先从 CPU 内存拷贝到显存
  → 额外开销，额外延迟

Apple 统一内存：
  CPU 和 GPU 共享同一块内存
  不需要拷贝！
  → 省电、快速、开发简单
  
  这也是为什么 Mac 不需要"独立显存"
  M3 Max 可以"共享"最高 128GB 内存给 GPU
```

### 现状

```
Metal 在 2024 年的地位：

  ✅ iPhone/iPad 图形 —— 唯一的图形 API
  ✅ Mac 游戏 —— 逐渐增多（《生化危机》《死亡搁浅》移植）
  ✅ Apple 生态开发 —— 必须学 Metal
  ❌ 游戏数量少 —— 相比 Windows，Mac 游戏市场小很多
  ❌ 引擎支持 —— Unity/Unreal 支持，但不如 D3D/Vulkan 完善
```

---

## 七、WebGPU + WebGL——浏览器里的图形世界

### 浏览器也能跑 3D？

你可能在浏览器里玩过 3D 游戏、看过 3D 产品展示——这些不是用 OpenGL 或 Vulkan 画的，而是用 **Web 图形 API**。

### WebGL：浏览器里的 OpenGL

**WebGL（Web Graphics Library）** 诞生于 2011 年，它基于 OpenGL ES，让 JavaScript 可以在浏览器里画 3D 图形。

```
WebGL 的工作原理：

  浏览器 ←→ WebGL API ←→ 底层图形 API（OpenGL/Vulkan/D3D）
  
  你的 JavaScript 代码调用 WebGL
  浏览器把 WebGL 指令翻译成底层的 OpenGL 或 Vulkan
  然后传给显卡

  相当于"API 之上的 API"
```

```
WebGL 的特点：

  ✅ 所有浏览器都支持 —— Chrome、Firefox、Safari、Edge
  ✅ 大量 3D 网页应用 —— Three.js、Babylon.js 等库
  ✅ 不需要安装 —— 打开网页就能跑

  ❌ 基于 OpenGL ES —— 继承了 OpenGL 的效率问题
  ❌ 单线程 —— JavaScript 是单线程的
  ❌ 功能有限 —— 比原生 API 少了很多高级特性
```

### WebGPU：下一代 Web 图形标准

**WebGPU** 是 WebGL 的继任者，2023 年正式发布。它的设计理念不是基于 OpenGL，而是**吸收了 Vulkan / D3D12 / Metal 的设计精华**。

```
WebGPU vs WebGL：

  WebGL：
    "让浏览器也能跑 OpenGL"
    设计于 2011 年 → 底层是 OpenGL ES
    
  WebGPU：
    "让浏览器也能用底层 API"
    设计于 2023 年 → 底层可以是 Vulkan/D3D12/Metal
```

```
WebGPU 的特点：

  ✅ 更低的 CPU 开销 —— 比 WebGL 减少 50% 以上的开销
  ✅ 多线程支持 —— 可以在 Web Worker 中并行计算
  ✅ 现代设计 —— 吸收了 Vulkan/D3D12/Metal 的最佳实践
  ✅ 未来核心 —— 被认为是 Web 3D 的未来

  ❌ 新标准 —— 浏览器支持还在完善中
  ❌ 学习成本 —— 比 WebGL 更复杂
```

### 现状

```
Web 图形 API 在 2024 年的地位：

  ✅ WebGL 仍然是主力 —— 兼容性最好，所有浏览器都支持
  ✅ WebGPU 逐步推广 —— Chrome 已支持，Safari 和 Firefox 在跟进
  ✅ 3D 网页应用爆发 —— 产品展示、在线游戏、数据可视化
  ❌ 性能不如原生 —— 隔了一层浏览器，始终有额外开销
```

---

## 八、一张表看懂所有图形 API

### 全平台总览

| API | 发布年份 | 平台 | 层级 | 定位 |
|:----|:-------:|:----|:----:|:-----|
| **OpenGL** | 1992 | Windows/Linux/macOS（已停） | 上层 | 跨平台教学与兼容 |
| **OpenGL ES** | 2003 | Android/嵌入式/iPhone（已停） | 上层 | 移动端兼容 |
| **Direct3D 11** | 2009 | Windows/Xbox | 中上层 | Windows 游戏兼容 |
| **Metal** | 2014 | iOS/macOS/tvOS | 底层 | Apple 生态唯一选择 |
| **Direct3D 12** | 2015 | Windows 10+/Xbox Series | 底层 | AAA Windows 游戏 |
| **Vulkan** | 2016 | Windows/Linux/Android/Switch | 底层 | **跨平台高性能首选** |
| **WebGL** | 2011 | 所有浏览器 | 上层（Web） | 浏览器 3D 兼容 |
| **WebGPU** | 2023 | 现代浏览器 | 底层（Web） | **下一代 Web 3D** |

### 用"驾驶"来类比

```
图形 API 就像一个"开车"的过程：

  OpenGL / WebGL = 自动挡普通轿车
    自动换挡、自动刹车辅助
    好开，但不能飙车
  
  D3D11 = 自动挡"运动模式"
    可以手动换挡，也有自动辅助
    平衡选择

  D3D12 / Vulkan / Metal = 手动挡赛车
    离合、油门、换挡全手动
    最难开，但开好了最快
  
  WebGPU = 智能辅助驾驶
    让电脑帮你做决定
    但比全手动的更智能
```

### 性能光谱

```
驱动帮你做所有事 ←──────────────→ 你亲自做所有事
  开销最高                          开销最低
  最容易上手                        最难学

  WebGL → OpenGL → D3D11 → Metal → D3D12 → Vulkan
    |        |       |        |       |       |
  浏览器   教学     Windows  Apple   Xbox  跨平台
  3D      标准     兼容     独有    旗舰   之王
```

---

## 九、游戏引擎的 RHI 抽象层：一次编写，多端运行

### 游戏引擎不能只支持一种 API

现代游戏引擎的做法是：**抽象出一层统一的接口，底层对接多个 API。**

```
游戏逻辑（使用 RHI 抽象接口）
        ↓
  ┌─────────────┐
  │  RHI 抽象层  │  ← 统一的渲染接口
  └──────┬──────┘
         │
    ┌────┼────┬────┬────┐
    ↓    ↓    ↓    ↓    ↓
 OpenGL D3D11 D3D12 Vulkan Metal
    ↓    ↓    ↓    ↓    ↓
 不同平台 / 不同操作系统
```

### DSEngine 的实际架构

```
DSEngine 当前支持的图形后端：

  OpenGL（默认）—— 最广泛的兼容性
  Vulkan（可选）—— 高性能 Linux/Android 路径
  D3D11（可选）—— Windows 高性能路径

  通过环境变量选择：
    set DSE_RHI_BACKEND=opengl
    set DSE_RHI_BACKEND=vulkan
    set DSE_RHI_BACKEND=d3d11
```

### 为什么引擎做 RHI 抽象？

```
① 一次开发，多平台运行
   写一套渲染代码 → 编译到 Windows/Linux/Android
   不需要为每个平台重写渲染逻辑

② 灵活切换
   玩家 A 的显卡 Vulkan 驱动有问题？
   → 切换到 OpenGL 或 D3D11
   不需要改游戏代码

③ 面向未来
   将来出了新 API？
   → 只要新写一个后端实现
   → 所有游戏代码不需要改动
```

### 各大引擎的选择

| 引擎 | 后端支持 |
|:----|:--------|
| **Unreal Engine 5** | D3D12、Vulkan、Metal、OpenGL（后备） |
| **Unity** | D3D11、D3D12、Vulkan、Metal、OpenGL ES、WebGL、WebGPU |
| **Godot** | OpenGL ES、Vulkan、D3D12（开发中） |
| **DSEngine** | OpenGL、Vulkan、D3D11 |

---

## 十、一句话总结

> **图形 API 就是游戏和显卡之间的"翻译官"——OpenGL 和 WebGL 是自动挡，好开但不够快；D3D11 是平衡之选，Windows 游戏的中流砥柱；Vulkan、D3D12、Metal 是手动挡赛车，性能最强但也最难驾驭，分别统治着跨平台、Xbox 和 Apple 生态；WebGPU 则是浏览器的未来之星。游戏引擎通过 RHI 抽象层同时支持多个 API，让你写一次代码就能跑遍天下。**
>
> **下次在游戏设置里看到"图形 API"选项——Vulkan、Direct3D 12、OpenGL——你终于知道该选哪个了：想榨干性能选 Vulkan 或 D3D12，遇到兼容问题切回 D3D11 或 OpenGL。至于 Metal……除非你用的是 Mac。** 🎮
