# WebGPU 后端真·manager 类拆分方案（P1，已确认方案 2）

> 状态：**设计定稿 + 基础头文件就绪，实现未开始**。本会话产出 `webgpu_common.h`（共享类型/工具，
> 当前未被任何 TU include，**不影响 web 构建**）+ 本设计文档。后续新会话据此实施。

目标：把单体 `WebGPURhiDevice`（cpp 3518 行 / h 664 行）拆成独立 manager 类（各自持状态），
device 退化为 orchestrator，对齐 GL/DX11/Vulkan 的分层范式。**零功能改动、零调用方 API 改动**。
WebGPU 仅 web 构建可验证（native 守卫 `__EMSCRIPTEN__ && DSE_ENABLE_WEBGPU` 排除）。

## 风险定性（务必知悉）
- 这是**纯结构性整理，零功能收益**；动的是**唯一有 web 回归覆盖**的后端。
- 运行时正确性的唯一安全网 = web 双后端视觉回归（webgpu/webgl2）。部分路径（点光 cube 阴影回退、
  mega VAO、GPU-driven、compute 异步回读）回归未必覆盖 → 存在潜在 latent bug 残留风险。用户已知悉并接受。

## 验证回路
- 单 TU 语法校验：`ninja CMakeFiles/dse_engine.dir/engine/render/rhi/webgpu/<f>.cpp.o`（~5s/文件），
  先 `source /c/emsdk/emsdk_env.sh`，cwd=`out/build/web-release-3d`。
- 新文件经 CMake `GLOB_RECURSE + CONFIGURE_DEPENDS` 自动纳入（无需手改 CMake）；新增 .cpp 后先
  `cmake --build --preset web-release-3d` 触发一次 reconfigure。
- 最终门槛：web-release-3d 全量构建 exit 0 + 双后端回归 webgpu/webgl2 **<2%**
  （基线参考：webgpu 0.346% / webgl2 0.004%）。回归命令：`tests/web/` 下
  `DSE_DUMP_LOGS=1 node visual_regression.mjs --backend webgpu|webgl2 --name <name>`。
- harness（`DSE_WEBGPU_SELFTEST`，默认 OFF，回归构建不含）：单独 configure 一个 SELFTEST=ON 构建编译验证。
- 提交方式：**单原子 commit & push 到 `feature/engine-lib`，不发 PR**。

## 降低风险的关键手法

### 1) 同名稳定句柄缓存（device_/queue_）
各 manager 缓存 **同名** 稳定句柄 `device_`/`queue_`（device 生命周期内不变，AcquireDevice 设、
Shutdown 清）。→ 迁移过来的方法体里对 `device_`/`queue_` 的引用**文本不变**、无需 sed、无 staleness。

### 2) 每帧瞬态走 live 转发（不缓存，杜绝 staleness）
`frame_encoder_`/`backbuffer_view_`/`swapchain_format_`/`width_`/`height_` 由 **WebGPUContext 持有**，
其余 manager 通过 `ctx_->frame_encoder()` 等 live 访问器读取（**不缓存**）。迁移时对这些 bare 成员名做
**确定性 sed 改写**（带 `\b` 词界，与同名局部变量 `width`/`height` 不冲突，因成员有尾下划线）：
- `frame_encoder_` → `ctx_->frame_encoder()`（res/draw/shader 中的读取处；ctx 自身不变）
- `backbuffer_view_` → `ctx_->backbuffer_view()`（draw）
- `swapchain_format_` → `ctx_->swapchain_format()`（draw）
- `width_`/`height_` → `ctx_->width()`/`ctx_->height()`（读取处）

### 3) 机械抽取（避免手抄 3500 行的转写错误）
按下表的行区间用 `sed -n 'a,bp'` 抽方法体，仅做两步 sed：
1. 先剥离已提升到 namespace 作用域的类型限定：
   `s/WebGPURhiDevice::\(BufferEntry\|TextureEntry\|RenderTargetEntry\|ShaderEntry\|ComputeShaderEntry\|BindingInfo\|UboVersion\)/\1/g`
2. 再改方法定义前缀：`s/WebGPURhiDevice::/WebGPU<Manager>::/g`
   （span 内除「方法定义前缀」与「已提升类型限定」外无其它 `WebGPURhiDevice::` 出现，故安全。
   draw 的 nested 类型 `PipelineCacheEntry`/`ComputePipelineCacheEntry`/`VbBinding`/... 在 draw span 内
   去限定后按 nested 解析正常。）

## 共享类型归属（webgpu_common.h，已就绪）
已提升到 `dse::render::webgpu`（或同 namespace）作用域、供跨 manager 共享：
`BufferEntry / TextureEntry / RenderTargetEntry / ShaderEntry / ComputeShaderEntry / BindingInfo`
+ 工具 `AlignUp4 / ParseWgslBindings / ToVertexFormat / ToViewDimension / ToTopology / ToCullMode /
ToCompareFunc / ToBlendFactor / IsDepthFormat / ToAddressMode / ToFilterMode / FullMipCount /
WriteTextureLayerRGBA8`。

> **待补**：`UboVersion {WGPUBuffer buffer; uint64_t offset; uint64_t size;}` 也须提升到 common.h
> （res 写、draw 读，见下「版本环跨界」）。`MakeFaceView` 建议改为 common.h 自由函数
> `MakeFaceViewImpl(WGPUDevice, const TextureEntry&, int)`，各使用方加 2 参同名内联 shim。
>
> **注意**：device.h 当前仍内嵌 `BindingInfo`/`ComputeShaderEntry` 等 nested 定义。拆分时须删除
> device.h 内嵌定义、改 include `webgpu_common.h`，否则与 common.h 重定义冲突。

## 文件 / 类划分 + 方法路由表（行号基于 HEAD=b0f9f5f1 的 webgpu_rhi_device.cpp）

### A. `webgpu_context.{h,cpp}` — WebGPUContext（叶子，无 sibling 依赖）
状态：`instance_/device_/queue_/surface_/swapchain_/swapchain_format_/backbuffer_view_/frame_encoder_/
width_/height_/initialized_/max_color_attachments_/next_handle_(+NextHandle())`。
访问器：`device()/queue()/frame_encoder()/backbuffer_view()/swapchain_format()/width()/height()/NextHandle()`
+ 帧编码器助手 `CreateFrameEncoder()`（取 backbuffer_view_ + 建 frame_encoder_）/`ReleaseFrameEncoder()`/
`SubmitEncoder()`（finish+QueueSubmit+release）——供 orchestrator BeginFrame/EndFrame 调用。
方法：`242 AcquireDevice / 280 CreateSwapChain / 317 ReleaseSwapChain / 324 InitDevice /
335 OnWindowResized / 405 WaitIdle / 409 EnsureInitialized`。

### B. `webgpu_resource_manager.{h,cpp}` — WebGPUResourceManager（依赖 ctx）
状态：`buffers_/textures_/render_targets_/ubo_ring_(+size/cursor)/geom_ring_(+size/cursor)/
ubo_versions_/geom_versions_/async_rb_*(staging[2]/capacity[2]/write_idx/has_pending/pending_size/
pending_idx/mapped[2]/ready/result + DeferredReadbackCtx)/hiz_textures_`。缓存 `device_/queue_`，持 `ctx_`。
方法：`421 AllocUboVersion / 456 AllocGeomVersion / 681 CreateSampler / 696 DestroyTextureEntry /
703 CreateTextureImpl / 765 FindBuffer / 769 FindTexture / 773 FindRenderTarget / 784 CreateRenderTarget /
831 DeleteRenderTarget / 845+849 GetRenderTargetColorTexture(x2) / 855 GetRenderTargetDepthTexture /
860 ReadRenderTargetColorRgba8 / 864 ReadRenderTargetColorRgba8WithSize / 874+878 CreateTexture2D(x2) /
887 CreateComputeWriteTexture2D / 1056 CreateTextureCube / 1066 CreateTextureCubeWithMips /
1088 CreateTexture3D / 1115 DeleteTexture / 1899 CreateBufferRaw / 1929 CreateBuffer / 1941 CreateGpuBuffer /
1957 UpdateGpuBuffer / 1995 DeleteGpuBuffer / 2003 BeginGpuReadback / 2035 GetLastReadbackResult /
2040 OnDeferredReadbackMapped(static) / 2058 KickDeferredReadback / 2069 UpdateBuffer / 2105 DeleteBuffer /
3076 CreateHiZTexture / 3106 DeleteHiZTexture / 3113 GetHiZMipCount / 3120 GetHiZGpuTexture`。
供 draw 的访问器（见「版本环跨界」）：`FindUboVersion(h)/FindGeomVersion(h)/ubo_ring()`。

### C. `webgpu_shader_manager.{h,cpp}` — WebGPUShaderManager（依赖 ctx + res + pso）
状态：`shaders_/compute_shaders_/wgsl_program_cache_/skybox_cube_vbo_/logged_incomplete_programs_/
gpu_driven_pbr_program_/gpu_driven_pbr_pso_/gpu_driven_pbr_failed_/gpu_driven_perframe_ubo_/
gpu_driven_perscene_ubo_/white_texture_`。内建 WGSL 常量（cpp 匿名 ns，约 1170–1840）随之迁入 shader.cpp。
方法：`1125 CreateShaderProgram / 1155 DeleteShaderProgram / 1841 GetOrCreateWgslProgram /
1850 GetBuiltinProgram / 1866 GetGenPPShaderProgram / 1892 GetSkyboxCubeVertexBuffer /
2169 CompileWGSL / 2981 EnsureGpuDrivenPBRShader / 3023 HasGPUDrivenPBRShader /
3131 CreateComputeShader / 3159 CreateComputeShaderEx / 3170 DeleteComputeShader`。
依赖：res（白纹理/skybox vbo/PBR PerFrame·PerScene UBO via CreateGpuBuffer/CreateTexture）、
pso（PBR PSO via CreatePipelineState）、ctx（device_/queue_）。

### D. `webgpu_pipeline_state_manager.{h,cpp}` — WebGPUPipelineStateManager（薄，无 sibling 依赖）
状态：`pipeline_states_`（`unordered_map<unsigned,PipelineStateDesc>`）。
方法：`777 FindPipelineState / 1162 CreatePipelineState`。
（真实 `WGPURenderPipeline` 的惰性建/缓存在 draw executor，不在此。）

### E. `webgpu_draw_executor.{h,cpp}` — WebGPUDrawExecutor（依赖全部：ctx + res + shader + pso）
状态（全部录制/帧态）：nested 类型 `VbBinding/UboBinding/TexBinding/SsboBinding/ComputeViewBind/
PipelineCacheEntry/ComputePipelineCacheEntry/MegaVaoEntry`；
`cur_pass_/cur_pass_is_backbuffer_/cur_color_formats_/cur_depth_format_/cur_sample_count_/cur_rt_width_/
cur_rt_height_/cur_pass_views_/cur_pass_attachment_texs_/cur_pso_handle_/cur_program_/cur_vbs_/
cur_ib_handle_/cur_ib_format_/cur_ubos_/cur_texs_/cur_ssbos_/cur_compute_images_/cur_compute_textures_/
cur_compute_image_views_/cur_compute_texture_views_/compute_mip_views_/cur_vs_push_/cur_fs_push_/
backbuffer_drawn_/pipeline_cache_/push_pool_(+used)/frame_bindgroups_/compute_named_staging_/
compute_named_offsets_/compute_named_next_/cur_compute_named_buffer_(+offset/size)/compute_pipeline_cache_/
cur_compute_pass_/mega_vaos_(+next_mega_vao_id_)/shadow_fallback_rt_(+depth_tex_+cleared_)/
point_shadow_fallback_rt_(+tex_+cleared_)/nonfilter_sampler_`。缓存 `device_/queue_`，持 `ctx_/res_/shader_/pso_`。
方法：`492 EnsureShadowDepthFallback / 526 EnsurePointShadowFallback / 901 SetComputeTextureImage /
914 SetComputeImageViewExplicit / 924 InvalidateComputeMipViews / 938 SetComputeTextureImageMip /
976 SetComputeTextureSampler / 989 GetOrCreateComputeNamedOffset / 998 WriteComputeNamedStaging /
1003–1050 SetComputeUniform{Int,Float,Vec2i,Vec2f,Vec3,IVec3,Vec4,Mat4} / 1985+1990 BindGpuBuffer(x2) /
2112 CreateVertexArray / 2116 DeleteVertexArray / 2145 ResetDrawState / 2162 ReleasePassViews /
2180 MakeFaceView / 2192 CollectGroupBindings / 2336 GetOrCreateRenderPipeline / 2532 BuildAndSetBindGroups /
2558 BindPassDrawState / 2595 IssueDraw / 2614 CmdBeginRenderPass / 2705 CmdEndRenderPass /
2714 CmdSetViewport / 2733 CmdClearColor / 2738 CmdBindGlobalShadowMap / 2744 CmdBindGlobalSpotShadowMap /
2747 CmdBindGlobalPointShadowMap / 2753 CmdDrawIndexedIndirect / 2768 MultiDrawIndexedIndirect /
2787 CreateMegaVAO / 2807 UpdateMegaVBO / 2812 UpdateMegaIBO / 2817 DeleteMegaVAO / 2825 BindMegaVAO /
2844 UnbindVAO / 3032 SetupGPUDrivenPBRShader / 3060 BindGPUDrivenTextures / 3125 CmdDispatchComputePass /
3177 BeginComputePass / 3183 EndComputePass / 3191 CollectComputeGroupBindings / 3302 GetOrCreateComputePipeline /
3372 DispatchCompute / 3435 CmdBindPipeline / 3442 CmdBindVertexBuffer / 3453 CmdBindIndexBuffer /
3458 CmdBindTexture / 3462 CmdBindUniformBuffer / 3467 CmdBindStorageBuffer / 3472 CmdPushConstants /
3484 CmdDraw / 3488 CmdDrawIndexed / 3492 CmdDrawIndexedInstanced`。
draw 需向 sibling 取的内联 shim（同名转发，使方法体内 bare 调用文本不变）：
`FindBuffer/FindTexture/FindRenderTarget`→`res_`；`FindPipelineState`→`pso_`；
`AllocUboVersion/AllocGeomVersion/CreateRenderTarget/GetRenderTargetDepthTexture/CreateTextureImpl/
CreateSampler/UpdateGpuBuffer/CreateBufferRaw`→`res_`；`CompileWGSL`→`shader_`；
`EnsureGpuDrivenPBRShader/HasGPUDrivenPBRShader` 及 PBR program/pso/ubo/white_texture 访问器→`shader_`；
`NextHandle`→`ctx_`。

### F. `webgpu_command_buffer.h`（新）
`WebGPUCommandBuffer`（当前在 cpp 匿名 ns，约 27–230 行）原样移出为独立 header，逐调用转发到 device 的 `Cmd*`。

### G. `webgpu_rhi_device.{h,cpp}`（改为 orchestrator）
持 5 个成员：`std::unique_ptr<WebGPUContext> ctx_ / WebGPUResourceManager res_ / WebGPUShaderManager shader_ /
WebGPUPipelineStateManager pso_ / WebGPUDrawExecutor exec_`（构造序：ctx→res→pso→shader→exec；
各 manager 在 AcquireDevice 后 `Init(ctx_, ...)` 注入依赖与缓存 device_/queue_）。
device 自留：`221 ctor / 227 dtor / 235 GetDeviceInfo / 2120 CreateCommandBuffer / 2124 Submit /
2130 LastFrameStats / 3501 GetProjectionCorrection / 3510 GetShadowSampleCorrection` +
**orchestrator** `341 Shutdown`（按逆序调各 manager 释放）/`587 BeginFrame`（清版本环/版本图→
EnsureShadow*Fallback→ctx 取帧编码器→ResetDrawState→重置 push 池）/`608 EndFrame`（自检 hook→
backbuffer 兜底清屏 pass→ctx submit→KickDeferredReadback→释放 frame_bindgroups_/帧编码器）/
`666 PresentFrame`（no-op）。
其余所有 RHI 虚函数 = **一行转发**到对应 manager（如 `void CmdDraw(...){ exec_.CmdDraw(...);} `）。
device.h 须删除内嵌的资源/绑定 nested 结构（改用 common.h），保留 friend 访问器（见下）。

## 版本环跨界（res ↔ draw）解决方案
- `ubo_versions_` 写于 `UpdateBuffer`(2094, res)，读于 `BuildAndSetBindGroups`(2248-2249, draw)。
- `geom_versions_` 写于 `UpdateBuffer`(2101, res)，读于 `BindPassDrawState/GetOrCreateRenderPipeline`(2567-2582, draw)。
- `cur_compute_named_buffer_ = ubo_ring_`(3391, draw) 读 res 的环。
- `ubo_versions_.clear()/geom_versions_.clear()`(591/593) 在 orchestrator BeginFrame。
- **决策**：版本环 + 版本图归 **res**；`UboVersion` 提升至 common.h；draw 端 3 处直接 map 访问改为
  res 访问器：`res_->FindUboVersion(h)`（返回 `const UboVersion*` 或 nullptr）、`res_->FindGeomVersion(h)`、
  `res_->ubo_ring()`。BeginFrame 的两处 `.clear()` 改为 `res_.BeginFrameResetVersions()`。

## harness（唯一被迫的偏差，纯机械只读迁移）
harness 是 device 的 `friend`，直接读 device 字段（**全为只读**，无赋值/取址）：
`device_(51) / frame_encoder_(50) / cur_compute_pass_(42) / cur_pass_(34) / queue_(4) / width_(1) / height_(1)`
= 183 处；另调私有方法 `ResetDrawState(59) / CompileWGSL(2) / CreateTextureImpl(7)`。字段移入 manager 后会断。
处理：device 保留 friend 可见只读访问器 `device()/queue()/frame_encoder()/cur_pass()/cur_compute_pass()/
width()/height()`（转发到对应 manager）+ 私有转发 `ResetDrawState/CompileWGSL/CreateTextureImpl`；
harness 做**纯机械** `dev_->字段` → `dev_->字段()` 替换（结构/逻辑 100% 不变），以 SELFTEST=ON 单独编译验证。

## 依赖与施工顺序（低→高，每步单 TU 编译兜错）
1. common.h（含补 `UboVersion`/`MakeFaceViewImpl`）— ✅ 主体已就绪
2. context.{h,cpp}
3. resource_manager.{h,cpp}
4. pipeline_state_manager.{h,cpp}
5. shader_manager.{h,cpp}
6. draw_executor.{h,cpp}
7. command_buffer.h
8. device.{h,cpp} orchestrator 化 + 删 nested + friend 访问器
9. harness 机械 field→accessor 迁移（SELFTEST=ON 编译验证）
10. web-release-3d 全量构建 exit 0 → 双后端回归 <2% → 单原子提交 push（不发 PR）

## 禁止事项
- 不改 WGSL 手写源 / 不改 shader 单源生成消费逻辑 / 不改 native 构建 / 不改 harness 结构（仅机械字段→访问器）。
- 不引入新依赖；不提交无关文件/脚本。
