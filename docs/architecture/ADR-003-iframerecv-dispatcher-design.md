# ADR-003: IFrameRecv 接口与 FrameDispatcher 设计

- 状态: Accepted
- 日期: 2026-03-07

## 背景

项目需要定义一个统一的"帧接收者"抽象，使帧的生产方（`LiveInputDev` 采集链路）与消费方（实时显示、目标跟踪、编码写入等）之间解耦。
消费方种类多样，且同一路帧可能需要同时投递给多个消费方（如：显示 + 录制同时进行）。

## 决策驱动因素

- LiveInputDev 已采用 callback 驱动模型，接收侧应保持对称的 push 语义。
- 生产方不应感知消费方类型，只持有 `IFrameRecv*` 引用。
- 同一路帧需要可选地分发给多个消费方。
- 接口应保持最小，v1 不纳入背压、生命周期管理、流控等机制。
- 调用假设：消费方在回调线程上同步调用，且能立即返回；无需 bool 返回值传递背压信号。

## 决策

### 1. IFrameRecv 接口（Heavy，全局命名空间）

```cpp
using TFrameRecvFramePtr = std::shared_ptr<UniAV::UniAVFrame>;

class IFrameRecv
{
public:
    virtual ~IFrameRecv() = default;
    virtual void ReceiveFrame(const TFrameRecvFramePtr& spFrame) = 0;
};
```

**关键选择：**

- 返回类型为 `void`：调用假设为同步且立即返回，无需 bool 表达背压；如未来需要背压，引入独立接口扩展。
- 方法命名 `ReceiveFrame`：与项目匈牙利命名风格一致，语义直观。
- 接口不区分音频/视频：消费方通过 `mediaType()` 自行过滤，接口保持最小。
- 接口声明 4 条契约（共享所有权、异步时需复制 `shared_ptr`、快速返回、nullptr 防御），通过 Doxygen 注释记录。
- 不放入 `UniAV` 命名空间：该接口面向业务层，是应用层胶水，不属于内部数据层。

### 2. FrameDispatcher（扇出分发器）

- 实现 `IFrameRecv`，本身也是接收方，可级联。
- 内部以 `std::weak_ptr<IFrameRecv>` 持有订阅方，不延长其生命周期。
- 采用 **BestEffort** 策略：分发前先快照订阅表（持锁），再锁外依次调用各接收方，避免持锁调用导致死锁。
- 失效的 `weak_ptr` 在每次 `ReceiveFrame` 时自动清理。
- `nullptr` 帧在 `FrameDispatcher` 层被拦截，不转发给任何接收方。
- 防重注册：`AddReceiver` 对同一对象的重复注册静默忽略。

### 3. 线程模型

- 接口不约束调用线程；契约声明实现方应尽快返回。
- `FrameDispatcher` 对订阅表的读写用 `std::mutex` 保护，持锁期间不调用接收方（快照模式）。

## 备选方案

### 方案 A：返回 `bool` 表达背压
- 优点：调用方可实现丢帧/重试逻辑。
- 缺点：v1 同步调用语义下无意义；过早引入复杂度。
- 结论：不采用，后续如需背压，以独立接口或 trait 扩展。

### 方案 B：模板化接口 `IFrameRecvT<TFrame>`
- 优点：可适配非 UniAVFrame 帧类型。
- 缺点：引入模板后无法通过虚函数实现多态分发，与 FrameDispatcher 模型不兼容。
- 结论：不采用，固定绑定 `std::shared_ptr<UniAV::UniAVFrame>`。

### 方案 C：本轮同时改造 ILiveOutput 继承关系
- 优点：接口联通更完整。
- 缺点：扩大本次改动范围，ILiveOutput 的生命周期方法设计尚未最终确定。
- 结论：推迟至下一轮。

## 影响

- **正向**：生产方与消费方完全解耦；FrameDispatcher 支持运行时动态增减接收方；weak_ptr 持有避免循环引用。
- **负向**：Heavy 接口引入对 UniAVFrame 的编译依赖；调用方需注意 ReceiveFrame 快速返回契约。

## 落地范围（本次）

- `FrameRecv/IFrameRecv.h`：接口定义与契约注释。
- `FrameRecv/FrameDispatcher.h/.cpp`：扇出分发器实现。
- `scaffold/scaffold_iframerecv.cpp`：端到端 smoke test（防重、nullptr 防御、weak_ptr 清理、RemoveReceiver）。
- `CMakeLists.txt`：新增 `RSCA_Scaffold_IFrameRecv` 构建目标。

## 未落地（推迟）

- `ILiveOutput` / `FrameGuider` 继承 `IFrameRecv` 的改造。
- `FrameDispatcher` 的异步模式（带工作线程与队列）。
- 背压与丢帧策略扩展。

## 风险与缓解

- 风险：接收方阻塞导致所有分发卡住。
  - 缓解：契约明确"快速返回"；如有慢速消费方，由其自行维护内部队列，接口层不强制。
- 风险：接口过早固化。
  - 缓解：v1 保持最小；扩展通过派生接口而非修改 `IFrameRecv` 完成。
