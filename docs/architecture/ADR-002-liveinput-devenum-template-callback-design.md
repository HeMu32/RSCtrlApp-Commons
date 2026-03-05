# ADR-002: LiveInput 设备枚举与回调采集模板设计

- 状态: Accepted
- 日期: 2026-03-05

## 背景

当前项目需要同时支持 BMD DeckLink 采集卡与 Qt WebCam，并对调用方提供一致的设备打开与音视频采集接口。
同时，`DevEnum` 需要承担更通用的设备管理能力（不限于视频采集设备）。

项目约束如下：

- 设备枚举应保持最小依赖（Lite）抽象。
- 音视频采集实现无法保持轻量，必须作为 Heavy 接口并绑定 UniAVFrame 语义。
- 采集链路需要 callback 驱动，以适配实时场景。

## 决策驱动因素

- 调用方希望“先枚举、再打开、再回调消费”的一致流程。
- BMD 与 Qt WebCam 在设备标识、能力与时间戳来源上有显著差异。
- 需要将“枚举的轻量边界”与“采集的重型边界”明确拆分，避免职责混淆。
- 需要支持不写定返回类型的创建方式，并将具体设备类型处理留给实现层与 caller。

## 决策

1. 在 `DevEnum/IDevEnum.h` 采用最小原型接口（Lite）：
    - `IDevEnum<TCreateParams, TDeviceInfo>` 只定义抽象原型，不定义具体设备类型逻辑。
   - 接口仅提供三个方法：
     - `Refresh()`
       - `ListDevices()`
     - `OpenByIndex()`
    - `ListDevices()` 返回设备展示结构体（至少含索引/名称/类型/位置，字段允许为空）。
   - `OpenByIndex()` 返回 `std::shared_ptr<void>`，由 caller 在已知枚举器上下文中自行强转。

2. 在 `LiveInputDev/ILiveInput.h` 引入回调驱动采集模板（Heavy）：
   - `ILiveInputT<TFrame, TNativeId, TBackendConfig>` 作为统一采集接口。
   - `TLiveInputCallbacks<TFrame>` 统一帧、状态、错误、统计回调。
   - `TLiveInputOpenParams<TNativeId, TBackendConfig>` 将设备选择、模式、后端配置解耦。
   - 默认接口别名 `ILiveInput` 直接绑定 `std::shared_ptr<UniAV::UniAVFrame>`。
   - 接口层不定义 BMD/Qt 专属配置结构，后端细节由具体实现类自定义。

3. 通过“模板参数 + 实现自定义配置”处理差异：
   - 具体设备（BMD、Qt WebCam 等）在实现层声明各自配置结构。
   - 接口层不直接包含 Qt 或 DeckLink 具体头。

4. 与 UniAVFrame 的衔接原则：
   - LiveInput 默认接口强绑定 `std::shared_ptr<UniAV::UniAVFrame>`。
   - 模板保留用于后端配置与原生 ID 类型扩展，而非规避重型依赖。

## 备选方案

### 方案 A：直接定义非模板统一类，字段全部固化

- 优点：调用侧类型更直观。
- 缺点：原生 ID 与后端特有参数难以扩展，容易引入大量可选字段和分支。

结论：不采用。

### 方案 B：按后端分别定义两套独立接口（Qt 一套，BMD 一套）

- 优点：各自实现简单直接。
- 缺点：调用方必须维护双分支，无法实现“统一打开/回调消费”流程。

结论：不采用。

## 影响

- 正向：
   - 调用方 API 使用路径统一且足够简洁。
   - 设备枚举保持 Lite，可复用于非采集链路。
   - 可实现“一次枚举(含可展示信息) + 选择序号 + OpenByIndex”的最短调用路径。
   - 类型擦除返回避免 `DevEnum` 侧绑定具体设备类型。
   - 接口头文件仅保留原型，职责边界清晰。
   - 采集接口与 UniAVFrame 强一致，减少实现分叉。

- 负向：
   - Heavy 采集接口会引入更高编译依赖。
   - 实现层仍需处理各后端线程与时序差异。

## 落地范围

- 本次仅定义抽象模板与通用数据结构。
- 不包含 BMD/Qt 具体实现类与线程模型实现。

## 风险与缓解

- 风险：枚举器承担打开职责后，接口仍可能膨胀。
   - 缓解：严格限制为三方法接口，不在枚举层扩展额外控制语义。

- 风险：类型擦除句柄存在误转型风险。
   - 缓解：caller 基于其已知枚举器类型与上下文执行转换，并在实现侧保证返回类型一致。

- 风险：callback 执行过慢导致背压和丢帧。
  - 缓解：接口层暴露 `TLiveInputStats`，实现层应采用解耦队列并明确丢帧策略。

- 风险：设备热插拔导致索引变化。
   - 缓解：要求 caller 在每次用户选择前先执行 `Refresh()` 并重新读取 `ListDevices()`。