// Heavy header.
// Frame guider consumes UniAVFrame objects and produces guidance / tracking outputs
// for PTZ composition control. Inherits IFrameRecv and may hold a reference to a
// GimbalDev::IGimbalDev implementation.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../FrameRecv/IFrameRecv.h"
#include "../GimbalDev/IGimbalDev.h"

/**
 * @brief Frame guider unified error code.
 */
enum class EFrameGuiderError : std::uint32_t
{
    Ok = 0,
    NotOpen,
    AlreadyOpen,
    InvalidConfig,
    QueueFull,
    UnsupportedMode,
    BackendFailure,
    InternalError
};

/**
 * @brief Frame guider runtime state.
 */
enum class EFrameGuiderState : std::uint8_t
{
    Idle = 0,
    Opening,
    Open,
    Closing,
    Error
};

/**
 * @brief Frame guider work mode.
 */
enum class EFrameGuiderMode : std::uint8_t
{
    SingleTarget = 0,
    MultiTarget,
    ObserveOnly
};

/**
 * @brief Tracker/guider output box in oriented-rectangle form.
 *
 * Coordinates are expressed in the original input-frame coordinate space.
 * They are allowed to exceed the current frame boundary; implementations must
 * preserve the raw tracking geometry instead of forcibly clipping to the image
 * rectangle.
 */
struct TFrameGuiderTrackBox
{
    float fX1 = 0.0F;
    float fY1 = 0.0F;
    float fX2 = 0.0F;
    float fY2 = 0.0F;
    float fX3 = 0.0F;
    float fY3 = 0.0F;
    float fX4 = 0.0F;
    float fY4 = 0.0F;
    std::int32_t nTrackId = -1;
    std::int32_t nClassId = -1;
    std::uint32_t uFlags = 0;
    float fScore = 0.0F;
};

/**
 * @brief Per-frame guider output context.
 */
struct TFrameGuiderResult
{
    std::int64_t nFrameTime = 0;
    std::int64_t nFrameDuration = 0;
    std::int64_t nTimeScale = 0;
    std::int64_t nSequence = 0;
    EFrameGuiderMode eMode = EFrameGuiderMode::SingleTarget;
    bool bDroppedEarlierFrames = false;
    std::vector<TFrameGuiderTrackBox> vTracks;
};

/**
 * @brief Frame guider runtime statistics.
 */
struct TFrameGuiderStats
{
    std::uint64_t uFramesReceived = 0;
    std::uint64_t uFramesSubmitted = 0;
    std::uint64_t uFramesProcessed = 0;
    std::uint64_t uFramesDropped = 0;
    std::uint32_t uQueueDepth = 0;
    double dLastProcessLatencyMs = 0.0;
    double dAvgProcessLatencyMs = 0.0;
};

/**
 * @brief Frame guider open/config parameters.
 *
 * `sBackendConfig` is intentionally backend-defined. It may encode model path,
 * tracker mode, queue policy, runtime DLL path, thresholds, or future options.
 */
struct TFrameGuiderOpenParams
{
    std::string sBackendConfig;
    EFrameGuiderMode eMode = EFrameGuiderMode::SingleTarget;
    bool bPreferLatestFrame = true;
    std::uint32_t uMaxQueueDepth = 2;
};

/**
 * @brief Unified frame-guider callback collection.
 *
 * @note Callbacks may be triggered on internal worker threads. Callers are
 *       responsible for thread-safe marshaling to UI or control threads.
 */
struct TFrameGuiderCallbacks
{
    std::function<void(const TFrameGuiderResult&)> fnOnResult;
    std::function<void(EFrameGuiderState, const TFrameGuiderStats&)> fnOnState;
    std::function<void(EFrameGuiderError, const std::string&)> fnOnError;
    std::function<void(const TFrameGuiderStats&)> fnOnStats;
};

/**
 * @brief Frame-driven PTZ guider interface（Heavy）。
 *
 * 继承 `IFrameRecv`，用于接收 `UniAVFrame` 并异步产生追踪/构图结果。
 * 该接口面向母项目统一 facade：上层通过 `ReceiveFrame()` 送入帧，
 * 实现应尽快返回；耗时识别、推理、追踪和控制决策由内部工作线程处理。
 *
 * ## 设计目标
 * - 统一承载“帧输入 -> 目标跟踪 -> 云台引导”链路。
 * - 保持与 Commons 既有回调风格一致（`std::function` callback set）。
 * - 对上层暴露异步语义，而不要求上层自行搭建异步队列。
 *
 * ## 生命周期
 * 推荐调用顺序：
 * `SetCallbacks()` -> `SetGimbalDev()`(optional) -> `Open()` -> `ReceiveFrame()` x N -> `Close()`。
 *
 * ## 帧接收契约（继承自 IFrameRecv）
 * 1. `ReceiveFrame()` 应尽快返回，不得在调用线程内执行长耗时推理。
 * 2. 实现若需异步处理，必须持有 `shared_ptr` 副本，不得依赖调用方栈对象。
 * 3. 在 `Idle` / `Error` 状态下收到帧时，实现应做防御性 no-op，不得崩溃。
 * 4. 若内部队列已满，实现可丢弃旧帧或新帧，但应更新统计并按实现策略通知。
 *
 * ## 云台依赖
 * - 实现可选择使用 `GimbalDev::IGimbalDev` 进行 PTZ 控制。
 * - 也允许工作在 ObserveOnly 模式，仅输出追踪结果而不发送控制指令。
 *
 * ## 结果坐标语义
 * - 输出框允许超出当前输入帧边界。
 * - 若上层需要显示裁剪、OSD 限幅或屏幕内绘制，应在消费侧自行裁剪。
 */
class IFrameGuider : public IFrameRecv
{
public:
    virtual ~IFrameGuider() = default;

    /**
     * @brief Register callback collection.
     *
     * Recommended to call before `Open()`. Implementations should atomically
     * replace the full callback set.
     *
     * @return true Success.
     * @return false Current state does not allow callback rebinding.
     */
    virtual bool SetCallbacks(const TFrameGuiderCallbacks& stCallbacks) = 0;

    /**
     * @brief Bind or replace the target gimbal device.
     *
     * @param spGimbal Nullable. Passing `nullptr` detaches current gimbal and
     *                 leaves guider in observe-only output mode unless the
     *                 implementation chooses otherwise.
     */
    virtual void SetGimbalDev(const std::shared_ptr<GimbalDev::IGimbalDev>& spGimbal) = 0;

    /**
     * @brief Open guider runtime with backend-defined configuration.
     *
     * @return `Ok` when open/startup sequence has been accepted.
     *         Implementations may finish backend initialization asynchronously;
     *         final readiness should also be reflected via `fnOnState`.
     */
    virtual EFrameGuiderError Open(const TFrameGuiderOpenParams& stParams) = 0;

    /**
     * @brief Close guider and release runtime resources.
     *
     * Must be idempotent. Pending work should be drained or discarded according
     * to implementation policy, and internal queues must be detached safely.
     */
    virtual void Close() = 0;

    /**
     * @brief Query whether guider is in `Open` state.
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief Query full runtime state.
     */
    virtual EFrameGuiderState GetState() const = 0;

    /**
     * @brief Query current work mode.
     */
    virtual EFrameGuiderMode GetMode() const = 0;

    /**
     * @brief Update current work mode.
     *
     * Intended for runtime strategy switching such as single-target / multi-target.
     */
    virtual EFrameGuiderError SetMode(EFrameGuiderMode eMode) = 0;

    /**
     * @brief Query runtime statistics.
     */
    virtual TFrameGuiderStats Stats() const = 0;

    /**
     * @brief Get user-facing backend/device/runtime name.
     */
    virtual std::string GetName() const = 0;
};
