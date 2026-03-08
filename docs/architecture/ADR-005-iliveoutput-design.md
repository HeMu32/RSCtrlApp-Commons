# ADR-005: ILiveOutput 接口设计

- 状态: Accepted
- 日期: 2026-03-08

## 背景

项目已完成 `IFrameRecv` / `FrameDispatcher` 的设计（ADR-003），并在彼时推迟了 `ILiveOutput` 的改造。
当前需要定义统一的输出端抽象，以覆盖显示预览窗口、硬件输出、编码器写入等多种消费侧场景。

## 决策驱动因素

- 输出设备需要接收帧（is-a `IFrameRecv`），且可直接挂接 `FrameDispatcher`。
- 绝大多数输出实现（显示窗口、编码器、硬件输出）都需要目标分辨率与帧率，接口层应统一提供配置入口。
- 输出端的"打开 / 关闭"生命周期与帧接收是正交的，需独立建模。
- 不引入模板化：输出侧不像输入侧那样需要泛型后端配置；差异化配置通过 `Open(sConfig)` 字符串或子类自定义方法处理。
- 最简参考实现使用 Qt 窗口（`QtPreviewOutput`），无额外第三方依赖。

## 决策

### 1. ILiveOutput 继承 IFrameRecv

`ILiveOutput` 直接继承 `IFrameRecv`，而非组合持有。

**理由：**
- 输出设备在语义上 *本身就是* 一个帧接收者，继承比组合更自然。
- 可以直接通过 `FrameDispatcher::AddReceiver()` 注册，与现有分发体系零改造兼容。
- 若采用组合，调用方需要额外 `.GetReceiver()` 才能获得 `IFrameRecv*`，增加不必要的间接层。

### 2. 视频格式配置接口（SetVideoFormat / GetVideoFormat）

```cpp
struct TLiveOutputVideoFormat
{
    std::int32_t nWidth  = 0;
    std::int32_t nHeight = 0;
    std::int32_t nFpsNum = 0;
    std::int32_t nFpsDen = 1;
};
```

- `SetVideoFormat()` 返回 `ELiveOutputError`，不关心格式的实现返回 `UnsupportedFormat`（非致命软性失败）。
- `GetVideoFormat()` 返回 `bool`，未配置或不支持时返回 `false`。
- 与 `Open()` 解耦：允许先配置格式再打开，也允许打开后重协商（如编码器动态切换分辨率）。

### 3. 生命周期方法

| 方法 | 语义 |
|---|---|
| `Open(sConfig)` | 激活输出端（打开窗口、初始化编码器等） |
| `Close()` | 停止并释放资源，状态回 `Idle`，幂等 |
| `IsOpen()` | 状态快捷查询（`== Open`） |
| `GetState()` | 完整状态查询 |

- `Open()` 接受 `const std::string& sConfig`，语义由实现自定义（窗口标题、URL、文件路径等）。
- `ReceiveFrame()` 在 `Open()` 之前或 `Close()` 之后被调用时，实现应做防御性 no-op（不崩溃）。

### 4. 错误码设计

| 值 | 含义 |
|---|---|
| `Ok` | 操作成功 |
| `NotOpen` | 尚未打开，无法执行该操作 |
| `AlreadyOpen` | 已处于打开状态，不可重复打开 |
| `InvalidConfig` | 配置参数无效（硬性失败） |
| `UnsupportedFormat` | 实现不支持格式配置（软性，非致命） |
| `BackendFailure` | 后端内部错误 |

`UnsupportedFormat` 特指"实现不关心格式配置"，与 `InvalidConfig` / `BackendFailure` 的硬性失败区分。
调用方在收到 `UnsupportedFormat` 后可以继续正常使用该输出端。

### 5. 不引入模板化

输出侧后端差异远小于输入侧（无原生 ID 类型差异、无采集时序结构差异）。
`sConfig` 字符串足以传递差异化参数；需要强类型配置的实现可在子类中扩展，不污染基础接口。

## 备选方案

### 方案 A：组合而非继承 IFrameRecv
- 优点：接口更灵活，ILiveOutput 可以不直接暴露 ReceiveFrame。
- 缺点：调用方需额外 `.GetFrameRecv()` 才能接入 FrameDispatcher，增加间接层。
- 结论：不采用。

### 方案 B：模板化 ILiveOutputT\<TBackendConfig\>
- 优点：强类型后端配置。
- 缺点：模板化后无法跨翻译单元多态持有，与 FrameDispatcher 模型冲突；输出侧差异不足以需要泛型。
- 结论：不采用。

### 方案 C：imshow / OpenCV 预览实现
- 优点：实现极简。
- 缺点：引入额外 OpenCV 依赖；Qt 已提供等价能力；imshow 的跨线程 GUI 问题在 Windows 上较难处置。
- 结论：不采用，改用 `QtPreviewOutput`（纯 Qt）。

## 落地范围（本次）

- `LiveOutDev/ILiveOutput.h`：接口定义、枚举、格式结构体、契约注释。

## 未落地（下一步）

- `LiveOutDev/QtPreviewOutput.h/.cpp`：Qt 窗口预览参考实现。
  - 线程模型：`ReceiveFrame()` 以 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 通知主线程渲染，
    接收线程立即返回；若帧速超过主线程处理能力，始终展示最新帧，中间帧自然丢弃。
  - `Open(sWindowTitle)` 创建并 `show()` 包含 `QLabel` 的窗口。
  - `SetVideoFormat()` 调整窗口初始尺寸并保存帧率信息。
  - 必须在 Qt 主线程构造，文档注明此约束。
- `scaffold/scaffold_liveoutput.cpp`：端到端 smoke test（加载图片 → UniAVFrame → QtPreviewOutput → QApplication::exec()）。
- `CMakeLists.txt`：新增 `RSCA_Scaffold_LiveOutput` 构建目标。

## 影响

- **正向**：输出端可直接挂接 FrameDispatcher，生产方无感知；格式配置入口统一；实现自由度高。
- **负向**：Heavy 接口依赖 IFrameRecv.h（进而依赖 UniAVFrame.h）；`Open(string)` 弱类型，实现侧需自行解析。

## 风险与缓解

- 风险：`ReceiveFrame()` 在 `Close()` 后被调用导致 use-after-free。
  - 缓解：接口契约要求实现方对 `Idle` 状态下的 `ReceiveFrame()` 做防御性 no-op。
- 风险：`SetVideoFormat()` 与 `Open()` 顺序不当。
  - 缓解：接口不约束调用顺序；文档声明推荐顺序（SetVideoFormat → Open），实现侧自行处理反序情况。
- 风险：`UnsupportedFormat` 被调用方误判为致命错误，导致流程中断。
  - 缓解：错误码注释与 ADR 均明确标注为"软性非致命"，调用方应在收到此值后继续正常流程。
