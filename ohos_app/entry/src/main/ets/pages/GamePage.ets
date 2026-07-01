/**
 * @file GamePage.ets
 * @brief DSEngine 游戏渲染页面 — XComponent 声明
 *
 * XComponent(type: SURFACE) 创建原生渲染表面（OHNativeWindow*），
 * 通过 libraryname 绑定 NAPI 模块，触发 napi_bridge.cpp 中的
 * OH_NativeXComponent_RegisterCallback 注册 Surface 生命周期和触屏回调。
 *
 * 等价于 Android 的 NativeActivity + SurfaceView。
 */

@Entry
@Component
struct GamePage {
  /**
   * XComponent 控制器（可选，用于获取 XComponent 尺寸等信息）
   */
  private xComponentController: XComponentController = new XComponentController();

  build() {
    Stack() {
      /**
       * XComponent 渲染表面
       *
       * - id: 全局唯一标识，C++ 侧通过此 ID 获取 OH_NativeXComponent*
       * - type: SURFACE 表示创建 OHNativeWindow*（GPU 渲染表面）
       * - libraryname: 对应 NAPI 模块名（napi_bridge.cpp 中 nm_modname = "dse_napi"）
       *
       * XComponent 创建后自动触发：
       *   1. napi_bridge.cpp::NapiInit() — 模块初始化
       *   2. OnSurfaceCreated() — 获取 OHNativeWindow*
       *   3. DispatchTouchEvent() — 触屏事件转发
       */
      XComponent({
        id: 'dse_surface',
        type: XComponentType.SURFACE,
        libraryname: 'dse_napi',
        controller: this.xComponentController
      })
      .width('100%')
      .height('100%')
      .onLoad(() => {
        console.info('[DSEngine] XComponent onLoad');
      })
      .onDestroy(() => {
        console.info('[DSEngine] XComponent onDestroy');
      })
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }
}
