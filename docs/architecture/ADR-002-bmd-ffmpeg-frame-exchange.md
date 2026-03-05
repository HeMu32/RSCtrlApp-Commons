# ADR-002: UniAVFrame 的 BMD 与 FFmpeg 帧交换策略

- 状态: Accepted
- 日期: 2026-03-05

## 背景

当前 UniAVFrame 已具备：

- BMD 视频帧接入与 RGBA 缓存构建
- FFmpeg 视频/音频 `AVFrame` 接入

但仍缺少一条清晰、稳定、可双向扩展的交换路径：

- BMD 音频包（`IDeckLinkAudioInputPacket`）如何进入 UniAVFrame
- UniAVFrame 的音频元数据如何无歧义映射到 FFmpeg `AVFrame`

参考 `Ext_Examples/BMDCapValidation/LiveCapureDemo/live_capture.cpp`，实际链路中 BMD 音频是“已配置采样参数 + 回调包体字节 + 样本数/时间戳”，而非 FFmpeg 原生多平面帧结构。

## 决策驱动因素

- 保持 UniAVFrame 为“数据载体”，不引入 A/V 对齐职责
- 兼容 BMD 回调模型（COM 对象、`AddRef/Release` 生命周期）
- 兼容 FFmpeg 音频模型（可能 planar、多平面、格式多样）
- 在 MinGW + DeckLink LinuxCOM 头环境下保持接口简洁、低耦合

## 决策

1. 引入独立的 BMD 音频工厂入口（建议）
   - 新增 `createFromBMDSDKAudioPacket(...)`
   - 输入 `IDeckLinkAudioInputPacket*` 与捕获配置元数据（sampleRate/sampleType/channels/timeScale）
   - 在工厂内执行 `AddRef`，由 `m_nativeOwner` 在析构时 `Release`

2. 音频元数据采用“帧内完整描述”
   - `AudioDesc` 至少应覆盖：
     - sampleRate
     - channels
     - sampleCount
     - bytesPerSample
     - sampleFormat（建议新增枚举，区分 S16/S32/FLT 等）
     - interleaved/planar（建议新增布尔或枚举）

3. 保持载体边界，不做音频对齐/切片
   - UniAVFrame 仅保存单包/单帧原始数据与描述
   - AAC 1024 对齐、FIFO 拼接、重采样由编码层负责

4. BMD→FFmpeg 交换采用“优先零拷贝、可选拷贝”策略
   - 同步短路径可零拷贝封装 `AVFrame`（引用 UniAVFrame 生命周期）
   - 异步/跨线程长路径可显式复制，防止外部生命周期不匹配

5. FFmpeg→UniAVFrame 音频保真要求
   - 不能只暴露 `data[0]` 语义，需能表达 planar 多平面
   - 若维持 `MemoryView` 单指针，则必须补充“首平面视图”语义说明
   - 推荐后续扩展 `AudioBufferView` 结构来表达多平面

## 备选方案

### 方案 A：继续复用 `createFromBMDSDK(void*)`，传入音频包

- 优点：接口少
- 缺点：类型语义混淆（视频帧与音频包同入口），校验/错误处理复杂

结论：不采用。

### 方案 B：在 UniAVFrame 内直接做重采样并输出 FFmpeg 可编码帧

- 优点：调用方简单
- 缺点：突破“数据载体”边界，耦合编码策略，难以维护

结论：不采用。

## 影响

- API 会新增一个 BMD 音频工厂入口
- 音频描述结构会更明确，减少 BMD 与 FFmpeg 交换歧义
- 后续编码链路更清晰：UniAVFrame 负责承载，Encoder 负责时序与整形

## 实施建议（分阶段）

1. Phase 1（最小可用）
   - 增加 `createFromBMDSDKAudioPacket`
   - 以 BMD 输入参数填充 `AudioDesc`，存储 `GetBytes` 的线性内存视图
   - 时间戳通过 `GetPacketTime` 填入 `FrameTimestamp`

### 当前落地状态（2026-03-05）

- Phase 1 已落地：
   - `UniAVFrameTypes` 增加音频格式/布局枚举与 `BMDAudioPacketDesc`
   - `UniAVFrameFactory` 增加 `createFromBMDSDKAudioPacket`
   - `UniAVFrameFactory` 增加 `createFromQtAudioPCM`（适配 Qt WebCam/麦克风回调的 PCM 缓冲）
   - FFmpeg 音频入口补齐 `sampleFormat` 与 `sampleLayout` 元数据

2. Phase 2（交换增强）
   - 增加 UniAV↔FFmpeg 音频桥接 helper（仅封装，不做重采样）
   - 支持 S16/S32 与 planar 标记

3. Phase 3（结构完善）
   - 增加 `AudioBufferView` 表达多平面
   - 保留现有 `MemoryView` 作为兼容字段

## 风险与缓解

- 风险：BMD 音频格式元数据不全导致描述错误
  - 缓解：工厂参数显式要求 sampleRate/sampleType/channels

- 风险：零拷贝路径被异步线程错误持有
  - 缓解：提供显式 copy 模式，并在接口注释中声明生命周期约束
