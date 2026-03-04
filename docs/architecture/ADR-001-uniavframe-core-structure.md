# ADR-001: UniAVFrame 核心结构与生命周期策略

- 状态: Accepted
- 日期: 2026-03-04

## 背景

项目需要在同一对象内统一承载 FFmpeg、Qt、OpenCV、BMD SDK 的输入帧数据，并支持多线程共享、引用计数生命周期、可复用内存池与可观测性（活跃对象计数）。

当前阶段目标是先解决“可用性”，在本地进程内稳定运行，再逐步优化到 4K/8K 高吞吐。

## 决策驱动因素

- 统一跨库访问语义，减少调用方分支复杂度
- 保证线程安全与内存安全
- 提供明确的失败语义，避免半可用对象
- 为后续性能优化保留清晰扩展点

## 决策

1. UniAVFrame 在创建时同时持有两份数据：
   - 原始输入帧（按来源保真存储/挂载）
   - RGBA8888 缓存（统一访问格式）

2. 采用 Eager Cache：
   - 收到输入帧时立即构建 RGBA8888 缓存
   - 不使用懒加载缓存
   - 缓存构建失败则对象创建失败，不返回半初始化对象
   - **设计目标**：FFmpeg 和 BMD 采集卡采集的数据通常为非 RGB 格式（如 YUV、BGRA、ARGB、10bit/12bit packed），
     Qt 和 OpenCV 侧对同一帧的多次访问若每次都触发转码，开销不可接受。
     RGBA8888 缓存确保转码只发生一次，后续访问直接读取缓存。

3. 并发与生命周期：
   - 对象通过共享所有权模型跨线程传递
   - 全局活跃计数使用原子变量，创建 +1，销毁 -1
   - RGBA 缓存在析构时归还池化内存

4. 原始帧引用的保留策略：
   - **设计目标**：保留对原始输入对象的引用（`m_nativeOwner`），以便在后续需要时提供精度更高的转换路径。
     当前 RGBA8888 缓存仅适用于显示和 8-bit 处理。BMD 设备支持采集 10-bit、12-bit 图像，
     未来高精度处理（如色彩科学、HDR 合成）需要访问原始位深数据，必须保留原始帧对象。
   - 对于 Qt/OpenCV 输入：业务上极少涉及向 FFmpeg/BMDSDK 格式转换，从内存安全角度考虑，
     保持当前深拷贝策略是合理的，`m_nativeOwner` 持有克隆后的对象。
   - 对于 FFmpeg/BMDSDK 输入：直接通过 `AddRef`（BMDSDK）或 `av_frame_clone`（FFmpeg）挂载，
     生命周期绑定至 UniAVFrame 析构。
   - **扩展原则**：后续如需支持其他输入后端，也应遵循相同模式——原始帧周期交给 UniAVFrame 管理，
     不允许调用方在 UniAVFrame 存活期间独立释放原始帧。

5. BMD 帧生命周期管理：
   - **设计目标**：来自 BMD 采集卡的 `IDeckLinkVideoFrame*` 的生命周期完全由 UniAVFrame 接管。
     `createFromBMDSDK()` 调用 `AddRef()` 增加引用计数，`m_nativeOwner` 持有该引用，
     析构时通过 custom deleter 调用 `Release()`。调用方在交出帧指针后不得再独立调用 `Release()`。
   - 原始字节数据的访问遵循 BMD SDK 规约：`StartAccess` → `GetBytes` → `EndAccess`，
     该访问窗口仅在 `buildRGBA8888Cache()` 内部短暂开启，转换完成后立即关闭。
     这是 API 合规要求，同时规避了 DMA 锁定等硬件资源冲突风险。

6. 内存池策略（第一阶段）：
   - 先提供最小可用池接口（acquire/release）
   - 复杂分桶、上限治理、统计信息在后续 ADR 扩展

## 备选方案

### 方案 A：懒加载 RGBA 缓存

- 优点：可能减少无效转换
- 缺点：并发下需要复杂双检锁，首次访问抖动大，不符合“到帧即建缓存”的需求

结论：不采用。

### 方案 B：只保留 RGBA，不保留原始输入

- 优点：结构更简单
- 缺点：丢失原始格式语义，不利于库间精确互操作与后续调试

结论：不采用。

## 影响

- 内存占用上升（原始 + RGBA 双持有）
- 访问路径更稳定（RGBA 获取不再触发临时转换）
- 后续可独立优化“输入解析”和“RGBA 构建”两段性能

## 实施范围（本次）

- 先落地类型定义、类接口、工厂入口、池接口与占位实现
- 暂不落地具体像素转换和后端细节

## 已知限制与设计备注

### BMDSDK 后端：`originalMemory().data` 为 `nullptr`

`MemoryView originalMemory()` 对 BMDSDK 帧返回 `{nullptr, N}`（`data` 为空，`sizeBytes` 为帧字节大小估算值）。

原因：`MemoryView` 是非拥有的裸指针结构，不具备生命周期绑定能力。BMD SDK 不承诺
`EndAccess()` 调用后字节指针仍然有效，因此存储该指针是不安全的行为。
对于 FFmpeg、Qt、OpenCV，`data` 指向的内存由 `m_nativeOwner` 的生命周期保护，BMDSDK 无法提供同等保证。

**现状**：像素内容已通过 RGBA8888 缓存完整保留，对于当前 8-bit 处理场景没有数据丢失。
**后续**：当需要访问原始位深数据时（如 10-bit BMD 帧），应扩展 `buildRGBA8888Cache()` 同时
将原始数据拷贝至独立池化缓冲区，并通过新接口（如 `nativePixelView()`）暴露。

### 后续像素格式接口扩展预留

当前 `buildRGBA8888Cache()` 仅处理 `bmdFormat8BitBGRA` 和 `bmdFormat8BitARGB`。
BMD 设备支持的 10-bit、12-bit 格式（如 `bmdFormat10BitRGB`、`bmdFormat12BitRGB`）
在当前实现中返回 `UniAVError::UnsupportedFormat`。扩展时在 BMDSDK 分支内增加对应转换路径，
并在 `PixelFormat` 枚举与相关接口中同步添加高位深类型。

## 风险与缓解

- 风险：接口过早固化导致后续扩展成本
- 缓解：通过 `InputBackend`、`UniAVError`、`UniAVFrameCreateOptions` 预留扩展位
