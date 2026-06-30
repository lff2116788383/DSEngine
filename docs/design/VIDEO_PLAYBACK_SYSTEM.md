# DSE 视频播放系统设计文档

## 1. 概述

为 DSEngine 添加视频播放能力，支持过场动画、UI 背景视频、世界内 3D 屏幕等场景。解码输出为 GPU 纹理，供渲染管线直接采样。

### 1.1 设计目标

- 支持主流视频编码（MPEG-1、H.264、H.265、VP9）
- 跨平台（Windows/Linux/macOS/Android/Web）
- 低延迟纹理上传（PBO 双缓冲 / Staging Buffer）
- 音视频精确同步
- 与现有 CutscenePlayer、UI 系统、ECS 无缝集成
- Lua 完整可脚本化

### 1.2 方案选型

| 方案 | 编解码 | 平台覆盖 | 依赖体积 | 硬件加速 | 许可证 |
|------|--------|----------|----------|----------|--------|
| **A: FFmpeg (libavcodec)** | H.264/H.265/VP9/AV1 全格式 | Win/Linux/macOS/Android | ~15-30MB | 可选 NVDEC/VAAPI/DXVA2 | LGPL2.1 (动态链接) |
| **B: 平台原生 API** | MF(Win)/AVF(macOS)/MC(Android) | 各自平台 | 0 | 默认硬件加速 | 无 |
| **C: Theora** | 仅 Theora/OGG | 全平台 | ~1MB | 无 | BSD |
| **D: pl_mpeg (单头文件)** | 仅 MPEG-1 | 全平台 | ~50KB | 无 | MIT |

**决策：方案 A (FFmpeg) 为主解码器 + 方案 D (pl_mpeg) 为零依赖轻量回退。**

理由：
- FFmpeg 覆盖所有主流编码，LGPL 动态链接不传染
- pl_mpeg 零依赖嵌入即用，适合菜单背景等低清场景
- 双解码器架构：无 FFmpeg 环境（如 Web/嵌入式）自动降级到 pl_mpeg

---

## 2. 架构设计

### 2.1 模块结构

```
engine/video/
├── video_types.h              // 枚举、配置、帧数据结构
├── video_decoder.h            // IVideoDecoder 抽象接口
├── video_decoder_plmpeg.h/cpp // pl_mpeg MPEG-1 解码器实现
├── video_decoder_ffmpeg.h/cpp // FFmpeg 解码器实现（动态加载）
├── video_player.h/cpp         // 高层播放控制器（Play/Pause/Seek/Loop/Speed）
├── video_texture.h/cpp        // 解码帧 → GPU 纹理上传（YUV shader / CPU 转换）
├── video_audio_sync.h/cpp     // 音视频同步（PTS 时钟对齐 + 帧环形缓冲）
└── video_screen_component.h   // ECS 组件：3D 世界内视频屏幕
```

### 2.2 依赖关系

```
VideoPlayer
  ├── IVideoDecoder (接口)
  │     ├── PlmpegDecoder (内嵌，零依赖)
  │     └── FFmpegDecoder (动态加载 libavcodec/libavformat/libswscale)
  ├── VideoTexture (GPU 上传)
  │     └── RhiDevice (纹理创建/更新)
  └── VideoAudioSync
        └── AudioSystem (PCM 推送 + 时钟回调)
```

### 2.3 线程模型

```
主线程(Game Loop)                     解码线程(per-player)
─────────────────                     ──────────────────
VideoPlayer::Update(dt)               while (playing) {
  │                                     frame = decoder->DecodeNext()
  ├─ 查询 audio_clock                   ring_buffer.push(frame)
  ├─ 从 ring_buffer 取匹配帧             cond_var.wait(buffer_not_full)
  ├─ VideoTexture::Upload(frame)       }
  └─ 返回 TextureHandle
```

- 解码线程提前解码 3-5 帧到无锁环形缓冲
- 主线程每帧从缓冲区取最接近当前时钟的帧
- 避免解码阻塞主线程渲染

---

## 3. 核心接口设计

### 3.1 video_types.h

```cpp
namespace dse {
namespace video {

enum class VideoState : uint8_t {
    Stopped,
    Playing,
    Paused,
    Finished,
    Error
};

enum class PixelFormat : uint8_t {
    YUV420P,    // 3 平面 Y/U/V
    RGBA8,      // 已转换 RGBA
    RGB8        // 已转换 RGB
};

enum class DecoderBackend : uint8_t {
    Auto,       // 优先 FFmpeg，回退 pl_mpeg
    FFmpeg,
    PlMpeg
};

struct VideoInfo {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double duration = 0.0;   // 秒
    int64_t total_frames = 0;
    bool has_audio = false;
    int audio_sample_rate = 0;
    int audio_channels = 0;
    std::string codec_name;
};

struct VideoFrame {
    PixelFormat format = PixelFormat::YUV420P;
    int width = 0;
    int height = 0;
    double pts = 0.0;         // Presentation Timestamp (秒)
    // YUV420P: planes[0]=Y, planes[1]=U, planes[2]=V
    // RGBA8: planes[0]=RGBA
    const uint8_t* planes[3] = {};
    int strides[3] = {};
};

struct VideoPlayConfig {
    DecoderBackend backend = DecoderBackend::Auto;
    bool loop = false;
    float playback_rate = 1.0f;
    bool decode_audio = true;
    bool hw_accel = false;         // 阶段4
    int prefetch_frames = 4;       // 环形缓冲大小
    bool yuv_gpu_convert = true;   // true=YUV shader, false=CPU sws_scale
};

} // namespace video
} // namespace dse
```

### 3.2 IVideoDecoder

```cpp
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    /// 打开视频文件/流
    virtual bool Open(const std::string& path, bool decode_audio = true) = 0;

    /// 解码下一帧（阻塞直到帧可用）
    virtual bool DecodeNextFrame(VideoFrame& out_frame) = 0;

    /// 解码音频样本到缓冲区
    /// @return 实际写入的样本数
    virtual int DecodeAudio(float* buffer, int max_samples) = 0;

    /// 跳转到指定时间（秒）
    virtual bool Seek(double time_sec) = 0;

    /// 获取视频元信息
    virtual VideoInfo GetInfo() const = 0;

    /// 是否已到末尾
    virtual bool IsEOF() const = 0;

    /// 关闭释放资源
    virtual void Close() = 0;

    /// 获取后端类型
    virtual DecoderBackend GetBackend() const = 0;
};
```

### 3.3 VideoPlayer

```cpp
class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    /// 播放控制
    void Play(const std::string& path, const VideoPlayConfig& config = {});
    void Pause();
    void Resume();
    void Stop();
    void Seek(double time_sec);
    void SetLoop(bool loop);
    void SetPlaybackRate(float rate);

    /// 每帧更新：推进时钟，从解码缓冲取帧，上传 GPU 纹理
    /// @return 当前帧纹理句柄（0=无有效帧）
    TextureHandle Update(float delta_time);

    /// 状态查询
    VideoState GetState() const;
    double GetCurrentTime() const;
    double GetDuration() const;
    const VideoInfo& GetInfo() const;
    TextureHandle GetCurrentTexture() const;

    /// 回调
    using Callback = std::function<void()>;
    void SetOnFinished(Callback cb);
    void SetOnLooped(Callback cb);
    void SetOnError(std::function<void(const std::string&)> cb);

private:
    std::unique_ptr<IVideoDecoder> decoder_;
    std::unique_ptr<VideoTexture> texture_;
    std::unique_ptr<VideoAudioSync> audio_sync_;
    // 解码线程 + 环形缓冲（阶段3）
    // ...
};
```

### 3.4 VideoTexture

```cpp
class VideoTexture {
public:
    VideoTexture(RhiDevice* rhi);
    ~VideoTexture();

    /// 初始化纹理资源（首帧时调用）
    void Initialize(int width, int height, PixelFormat format);

    /// 上传帧数据到 GPU 纹理
    /// @param frame 解码后的视频帧
    /// @return 可供采样的纹理句柄
    TextureHandle Upload(const VideoFrame& frame);

    /// 获取当前纹理句柄
    TextureHandle GetTexture() const;

    /// 释放 GPU 资源
    void Destroy();

private:
    RhiDevice* rhi_ = nullptr;
    TextureHandle texture_y_;     // Y 平面（YUV 模式）
    TextureHandle texture_u_;     // U 平面
    TextureHandle texture_v_;     // V 平面
    TextureHandle texture_rgba_;  // RGBA 模式
    TextureHandle output_tex_;    // 最终输出（YUV shader 渲染目标或直接 RGBA）
    unsigned int yuv_shader_ = 0; // YUV→RGB 转换 shader
    int width_ = 0, height_ = 0;
    PixelFormat format_ = PixelFormat::YUV420P;
    // PBO 双缓冲（GL）/ Staging Buffer（VK/D3D11）
    unsigned int pbo_[2] = {};
    int pbo_index_ = 0;
};
```

---

## 4. 实现阶段

### 阶段 1：pl_mpeg + VideoPlayer 基础架构 + Lua 绑定（1 周）

**目标**：最小可用版本，能播放 MPEG-1 视频并输出 GPU 纹理。

**实现内容**：
1. 引入 pl_mpeg 单头文件（`depends/pl_mpeg/pl_mpeg.h`，MIT 许可）
2. 实现 `PlmpegDecoder : IVideoDecoder`
   - Open: `plm_create(path)`
   - DecodeNextFrame: `plm_decode_video()` → YUV420P 或 RGBA
   - DecodeAudio: `plm_decode_audio()` → float interleaved
   - Seek: `plm_seek(time, FALSE)`
3. 实现 `VideoPlayer` 基础版（单线程，同步解码）
4. 实现 `VideoTexture` CPU 转换路径（YUV→RGBA via CPU，glTexSubImage2D）
5. Lua 绑定：`dse.video.create_player()` / `play` / `pause` / `stop` / `seek` / `get_texture`
6. GTest 单元测试（解码帧正确性、播放状态机、Seek 准确性）

**验收标准**：
- 能播放 `.mpg` 文件，输出正确颜色的 GPU 纹理
- Lua 脚本可控制播放
- 所有测试通过

### 阶段 2：FFmpeg 软解 H.264/H.265 + YUV Shader（2 周）

**目标**：支持现代视频格式，GPU 端 YUV→RGB 转换。

**实现内容**：
1. FFmpeg 动态加载封装（`FFmpegLoader`）
   - Windows: `LoadLibrary("avcodec-60.dll")` 等
   - Linux: `dlopen("libavcodec.so.60")`
   - 运行时检测：无 FFmpeg 则自动降级 pl_mpeg
2. 实现 `FFmpegDecoder : IVideoDecoder`
   - `avformat_open_input` → `avcodec_find_decoder` → `avcodec_open2`
   - `av_read_frame` + `avcodec_send_packet` / `avcodec_receive_frame`
   - 支持格式：H.264 (AVC)、H.265 (HEVC)、VP9、AV1
3. YUV→RGB GPU Shader
   - GLSL 片段着色器采样 Y/U/V 三纹理 → BT.709 矩阵转换
   - 渲染到 output_tex_（fullscreen quad 或 compute shader）
   - 三后端适配：GL texture / VK image / D3D11 SRV
4. PBO 双缓冲（GL 后端）
   - Frame N 上传到 PBO[0]，同时 GPU 异步 DMA PBO[1]→texture
   - 每帧交替，消除 CPU↔GPU 同步等待
5. Staging Buffer（VK/D3D11 后端）
   - 创建 HOST_VISIBLE staging buffer
   - 主线程 memcpy → 异步 copy command → device-local texture

**验收标准**：
- 播放 H.264/H.265 MP4 文件正确解码
- YUV shader 输出颜色准确（与 CPU 转换结果一致）
- 无 FFmpeg 环境时自动降级 pl_mpeg 不崩溃
- 1080p@30fps 软解 CPU 占用 < 15%

### 阶段 3：音视频同步 + 异步解码 + 3D 世界屏幕集成（1 周）

**目标**：专业级 A/V 同步，解码不阻塞主线程，视频可贴到 3D 物体表面。

**实现内容**：
1. `VideoAudioSync`
   - 音频主时钟：AudioSystem 每次填充 buffer 时更新 `audio_clock`
   - 视频帧匹配：取 `pts <= audio_clock` 的最新帧
   - 跳帧策略：video 落后 > 2 帧时丢弃中间帧
   - 等待策略：video 超前时保持上一帧不更新
2. 解码线程 + 无锁环形缓冲
   - `std::thread` 循环调用 `decoder->DecodeNextFrame()`
   - 环形缓冲容量 = `config.prefetch_frames`（默认 4 帧）
   - 条件变量：缓冲满时 wait，主线程消费后 notify
3. `VideoScreenComponent`（ECS 组件）
   ```cpp
   struct VideoScreenComponent {
       std::string video_path;
       bool auto_play = true;
       bool loop = true;
       float playback_rate = 1.0f;
       // 运行时
       uint32_t player_id = 0;
       TextureHandle current_texture;
   };
   ```
4. `VideoSystem`（ECS 系统）
   - 每帧遍历 VideoScreenComponent，调用对应 VideoPlayer::Update()
   - 将输出纹理写入 MeshRendererComponent 的 albedo_override slot
5. CutscenePlayer 集成
   - 新增 `VideoTrack`：时间线上的视频播放
   - 全屏模式：渲染到 fullscreen quad（UI 层之上）
6. Lua 扩展
   ```lua
   -- 3D 世界内视频
   local e = ecs.create_entity()
   ecs.add_component(e, "VideoScreen", { path = "videos/ad.mp4", loop = true })

   -- 过场视频
   cutscene.add_video_track(seq, { path = "cutscenes/intro.mp4", start = 0, fullscreen = true })
   ```

**验收标准**：
- 音视频同步误差 < 30ms
- 解码不阻塞主线程（60fps 下无掉帧）
- 3D 物体表面正确显示视频纹理
- CutscenePlayer 全屏视频播放正常
- 所有测试通过

### 阶段 4：硬件加速（可选，后续实现）（1-2 周）

**目标**：4K/高帧率视频场景下利用 GPU 硬件解码器降低 CPU 负载。

**实现内容**：
1. DXVA2 硬件加速（Windows/D3D11）
   - `avcodec_find_decoder` 时指定 `AV_HWDEVICE_TYPE_DXVA2`
   - 解码输出直接为 D3D11 Texture2D（零拷贝）
   - 需要 `av_hwframe_transfer_data` 或直接 SRV 绑定
2. NVDEC 硬件加速（NVIDIA GPU）
   - FFmpeg `AV_HWDEVICE_TYPE_CUDA` 后端
   - CUDA surface → OpenGL texture (interop) 或 copy to staging
3. VAAPI 硬件加速（Linux/Intel/AMD）
   - `AV_HWDEVICE_TYPE_VAAPI`
   - VA surface → EGL image → GL texture
4. VideoToolbox（macOS/iOS）
   - `AV_HWDEVICE_TYPE_VIDEOTOOLBOX`
   - CVPixelBuffer → Metal texture / IOSurface → GL texture
5. 自动降级
   - 硬件加速初始化失败时自动回退软件解码
   - 运行时检测 GPU 能力（vendor ID + driver version）

**验收标准**：
- 4K@60fps H.265 解码 CPU 占用 < 5%
- 硬件加速不可用时平滑降级
- 无画面撕裂或颜色空间错误

---

## 5. 纹理上传策略详细设计

### 5.1 YUV420P GPU 转换（默认路径）

```glsl
// video_yuv_to_rgb.frag
uniform sampler2D u_tex_y;
uniform sampler2D u_tex_u;
uniform sampler2D u_tex_v;

in vec2 v_uv;
out vec4 frag_color;

void main() {
    float y = texture(u_tex_y, v_uv).r;
    float u = texture(u_tex_u, v_uv).r - 0.5;
    float v = texture(u_tex_v, v_uv).r - 0.5;

    // BT.709 转换矩阵
    frag_color = vec4(
        y + 1.5748 * v,
        y - 0.1873 * u - 0.4681 * v,
        y + 1.8556 * u,
        1.0
    );
}
```

上传流程：
1. 创建 3 个纹理：Y (w×h, R8)、U (w/2×h/2, R8)、V (w/2×h/2, R8)
2. 每帧 `glTexSubImage2D` 更新三个平面
3. 绑定 fullscreen quad shader 渲染到 output_tex_ (RGBA8)
4. output_tex_ 供外部采样

### 5.2 PBO 双缓冲时序

```
Frame N:
  CPU → PBO[0] (memcpy, non-blocking)
  GPU ← PBO[1] (DMA to texture, async)

Frame N+1:
  CPU → PBO[1]
  GPU ← PBO[0]

交替使用，CPU 写入和 GPU DMA 完全并行。
```

### 5.3 Vulkan Staging Buffer

```
1. vkMapMemory(staging_buffer)
2. memcpy(mapped_ptr, frame_data, size)
3. vkUnmapMemory(staging_buffer)
4. vkCmdCopyBufferToImage(cmd, staging, device_image, ...)
5. Pipeline barrier: TRANSFER_DST → SHADER_READ
```

---

## 6. 音视频同步算法

### 6.1 时钟模型

```
audio_clock: AudioSystem 回调每次填充 buffer 时更新
             audio_clock = buffer_start_pts + samples_written / sample_rate

video_clock: 当前显示帧的 PTS

sync_threshold = max(0.04, frame_duration * 0.5)  // 40ms 或半帧时长
```

### 6.2 同步决策

```
每帧 Update() 时：
  diff = video_pts - audio_clock

  if diff > sync_threshold:
      // 视频超前，保持上一帧（等待音频追上）
      return last_texture

  if diff < -sync_threshold:
      // 视频落后，跳帧
      while (next_frame.pts < audio_clock - sync_threshold):
          discard(next_frame)
          next_frame = ring_buffer.pop()
      upload(next_frame)

  else:
      // 正常范围内，显示下一帧
      upload(next_frame)
```

### 6.3 无音频模式

当视频无音频轨或 `decode_audio = false` 时：
- 使用系统时钟（`steady_clock`）作为参考
- `video_clock += delta_time * playback_rate`
- 帧匹配：取 `pts <= video_clock` 的最新帧

---

## 7. ECS 与渲染集成

### 7.1 VideoScreenComponent

```cpp
struct VideoScreenComponent {
    bool enabled = true;
    std::string video_path;           // 视频文件路径
    bool auto_play = true;            // 实体激活时自动播放
    bool loop = true;                 // 循环播放
    float playback_rate = 1.0f;       // 播放速率
    DecoderBackend backend = DecoderBackend::Auto;

    // 运行时状态（System 管理）
    uint32_t player_handle = 0;       // VideoPlayerManager 分配的句柄
    TextureHandle current_texture;    // 当前帧纹理
};
```

### 7.2 VideoSystem

```cpp
class VideoSystem {
public:
    void Update(World& world, float delta_time) {
        auto view = world.registry().view<VideoScreenComponent, MeshRendererComponent>();
        for (auto entity : view) {
            auto& vc = view.get<VideoScreenComponent>(entity);
            auto& mr = view.get<MeshRendererComponent>(entity);

            if (!vc.enabled) continue;

            auto* player = GetOrCreatePlayer(vc);
            TextureHandle tex = player->Update(delta_time);

            if (tex) {
                mr.albedo_override = tex;  // 覆盖材质 albedo 纹理
                vc.current_texture = tex;
            }
        }
    }
};
```

### 7.3 CutscenePlayer 集成

CutscenePlayer 现有 Track 类型：Camera / Property / Event / Audio

新增 `VideoTrack`：
```cpp
struct VideoKeyframe {
    double time;          // 在时间线上的起始时间
    std::string path;     // 视频文件
    bool fullscreen;      // 全屏渲染（覆盖 3D 场景）
    float opacity;        // 混合不透明度
};
```

全屏渲染时：在 UI pass 之前插入全屏 quad，采样 VideoPlayer 输出纹理。

---

## 8. Lua API 设计

```lua
-- === 基础播放 ===
local player = dse.video.create_player()
player:play("videos/intro.mp4", {
    loop = false,
    playback_rate = 1.0,
    backend = "auto",       -- "auto" | "ffmpeg" | "plmpeg"
    decode_audio = true,
    prefetch_frames = 4
})

player:pause()
player:resume()
player:stop()
player:seek(10.5)  -- 跳转到 10.5 秒

-- 查询
local state = player:get_state()   -- "playing" | "paused" | "stopped" | "finished"
local time = player:get_time()     -- 当前播放位置（秒）
local duration = player:get_duration()
local info = player:get_info()     -- { width, height, fps, codec, has_audio, ... }
local tex = player:get_texture()   -- 当前帧纹理句柄

-- 回调
player:on("finished", function() print("done") end)
player:on("looped", function() print("loop") end)
player:on("error", function(msg) print("err: " .. msg) end)

-- 销毁
player:destroy()

-- === ECS 集成 ===
local e = ecs.create_entity()
ecs.add_component(e, "Transform", { position = {0, 2, 0} })
ecs.add_component(e, "MeshRenderer", { mesh_path = "meshes/screen.dmesh" })
ecs.add_component(e, "VideoScreen", {
    video_path = "videos/advertisement.mp4",
    auto_play = true,
    loop = true
})

-- === 过场集成 ===
local seq = cutscene.create_sequence("opening")
cutscene.add_video_track(seq, {
    path = "cutscenes/opening.mp4",
    start_time = 0.0,
    fullscreen = true,
    fade_in = 0.5,
    fade_out = 0.5
})
```

---

## 9. 测试策略

### 9.1 单元测试

| 测试类别 | 覆盖内容 |
|----------|----------|
| PlmpegDecoder | Open/Close、帧解码正确性、Seek 准确性、EOF 检测 |
| FFmpegDecoder | 动态加载、H.264/H.265 解码、降级回退 |
| VideoPlayer | 状态机转换、PlaybackRate、Loop、回调触发 |
| VideoTexture | 纹理创建/更新/销毁、YUV→RGB 正确性 |
| VideoAudioSync | 同步精度、跳帧、等待策略 |
| RingBuffer | 并发安全、满/空状态 |

### 9.2 集成测试

- 播放 10 秒 MPEG-1 视频，验证帧数 = fps × duration ± 1
- 播放 H.264 MP4，验证音视频同步误差 < 30ms
- 3D VideoScreen 组件创建/销毁，无资源泄露
- CutscenePlayer VideoTrack 正确触发播放/停止

### 9.3 测试资源

使用程序生成的测试视频（彩条 + 正弦音频），避免提交大文件到仓库：
```cpp
// 测试用：生成 RGB 渐变帧序列，验证解码颜色正确性
VideoFrame GenerateTestFrame(int frame_index, int width, int height);
```

---

## 10. 性能预算

| 场景 | CPU 预算 | GPU 预算 | 内存 |
|------|:--------:|:--------:|:----:|
| 1080p@30 MPEG-1 (pl_mpeg) | < 3ms/帧 | ~0.5ms（纹理上传） | ~12MB |
| 1080p@30 H.264 (FFmpeg 软解) | < 8ms/帧 | ~0.5ms | ~30MB |
| 4K@30 H.265 (FFmpeg 软解) | < 25ms/帧 | ~1ms | ~80MB |
| 4K@60 H.265 (硬件加速) | < 1ms/帧 | ~1ms | ~40MB |

环形缓冲内存 = width × height × 1.5 (YUV420P) × prefetch_frames

---

## 11. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| FFmpeg LGPL 合规 | 法律 | 严格动态链接，不修改 FFmpeg 源码 |
| FFmpeg 不可用（Web/嵌入式） | 功能降级 | pl_mpeg 自动回退，支持 MPEG-1 |
| 4K 软解性能不足 | 卡顿 | 自动降分辨率 + 阶段4硬件加速 |
| 音视频同步漂移 | 体验 | 定期重同步 + 丢帧策略 |
| GPU 纹理上传 stall | 帧率下降 | PBO 双缓冲 / Staging Buffer |

---

## 12. 实现时间线

| 周次 | 阶段 | 交付物 |
|:----:|:----:|--------|
| W1 | 阶段 1 | pl_mpeg 解码 + VideoPlayer + Lua + 测试 |
| W2-W3 | 阶段 2 | FFmpeg 动态加载 + YUV Shader + PBO |
| W4 | 阶段 3 | A/V 同步 + 解码线程 + ECS/Cutscene 集成 |
| W5-W6 | 阶段 4 | DXVA2/NVDEC/VAAPI 硬件加速（可选） |
