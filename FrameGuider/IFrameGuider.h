// Heavy header.
// Frame guider consumes UniAVFrame objects and produces guidance / tracking outputs
// for PTZ composition control. Inherits IFrameRecv.
//
// Boundary rule (must-not): IFrameGuider implementations must not hold or bind
// GimbalDev::IGimbalDev directly. Any gimbal command execution belongs to
// IPTZCamObj implementations that consume guider outputs.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../FrameRecv/IFrameRecv.h"

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
 * @brief Unified joystick value used for both caller input and guider output.
 *
 * The value range of each axis is normalized to `[-15000, 15000]`.
 * This numeric convention is intentionally aligned with DJI R SDK joystick
 * semantics so caller, guider, and gimbal-control layers can share a stable
 * boundary.
 *
 * `uFlags` is reserved for state/source/validity bits defined by the concrete
 * guider contract.
 * `nTimestamp` records when this value was last updated, using the current
 * guider instance's internal clock reference. It is filled by
 * `updateJoystickStat(...)` when the cached state is updated. This field is not
 * intended to replace the implementation's internal monotonic clock logic for
 * cache-expiry checks, even if both use the same underlying clock reference.
 */
struct TFrameGuiderJoystickValue
{
    std::int16_t nX = 0;
    std::int16_t nY = 0;
    std::uint32_t uFlags = 0;
    std::int64_t nTimestamp = 0;
};

/**
 * @brief Latest cached FoV state associated with the current capture/view path.
 * Unit: tangent of half FoV angle.
 *
 * `fTanHalfFovH` and `fTanHalfFovV` express the horizontal and vertical half-
 * FoV tangent values used by tracking / guidance logic.
 *
 * Unlike joystick state, FoV state does not use timeout-to-zero semantics in
 * the current design. Implementations cache and return the latest written value
 * until it is explicitly replaced.
 *
 * `nTimestamp` records when this FoV state was last updated, using the current
 * guider instance's internal clock reference. It is filled by
 * `updateFovStat(...)`.
 */
struct TFrameGuiderFovStat
{
    float fTanHalfFovH = 0.0F;
    float fTanHalfFovV = 0.0F;
    std::uint32_t uFlags = 0;
    std::int64_t nTimestamp = 0;
};

/**
 * @brief Tracker/guider output box in oriented-rectangle form.
 *
 * Coordinates are expressed in the original input-frame coordinate space.
 * They are allowed to exceed the current frame boundary; implementations must
 * preserve the raw tracking geometry instead of forcibly clipping to the image
 * rectangle.
 */
struct TFrameGuiderObjectPartBox
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
 * @brief Describe one tracked object and the contiguous part-box range that
 * belongs to it inside `TFrameGuiderResult::vParts`.
 *
 * This expresses the logical structure:
 *
 *   Container[i][p]
 *
 * where:
 * - `i` is the tracked-object index in `vObjects`
 * - `p` is the part index within that object, resolved by:
 *   `vParts[nPartOffset + p]`
 *
 * Part-index semantics are implementation-defined rather than globally fixed.
 * Callers MUST consult the concrete guider implementation documentation before
 * interpreting `p`.
 *
 * Example for the current `basetrk` design intent:
 * - `p = 0`: detection box
 * - `p = 1`: head indication box
 * - `p = 2`: body indication box
 *
 * Other guider implementations may legally use a different mapping.
 */
struct TFrameGuiderObjectPartsSpan
{
    std::int32_t nObjectIndex = -1;
    std::int32_t nPartOffset = 0;
    std::int32_t nPartCount = 0;
    std::int32_t nTrackId = -1;
    std::uint32_t uFlags = 0;
};

struct TFrameGuiderMoveToCommand
{
    std::int16_t nYaw = 0;
    std::int16_t nRoll = 0;
    std::int16_t nPitch = 0;
    std::uint32_t nTimeMs = 0;
};

struct TFrameGuiderZoomCommand
{
    std::int16_t nSpeed = 0;
};

struct TFrameGuiderFNoCommand
{
    float fFNo = 0.0F;
};

struct TFrameGuiderShutterCommand
{
    bool bTrigger = false;
};

struct TFrameGuiderAfPointCommand
{
    float fNormX = 0.0F;
    float fNormY = 0.0F;
};

/**
 * @brief Per-frame guider output context.
 *
 * This result object is intended to carry both:
 * - tracking geometry results
 * - joystick guidance output for downstream PTZ/gimbal control
 *
 * Result data is stored as two coordinated arrays:
 * - `vObjects`: object-level spans / metadata
 * - `vParts`: flat storage of all part boxes for the frame
 *
 * To iterate one object's boxes:
 *
 * ```cpp
 * const auto& obj = vObjects[i];
 * for (int p = 0; p < obj.nPartCount; ++p) {
 *     const auto& box = vParts[obj.nPartOffset + p];
 * }
 * ```
 *
 * The meaning of each `p` is defined by the concrete `IFrameGuider`
 * implementation, not by this shared interface alone.
 *
 * Joystick guidance fields, when present in concrete implementations, should
 * use the normalized range `[-15000, 15000]`. This numeric convention is
 * intentionally aligned with DJI R SDK joystick semantics so caller, guider,
 * and gimbal-control layers can share a stable value range.
 */
struct TFrameGuiderResult
{
    std::int64_t nFrameTime = 0;
    std::int64_t nFrameDuration = 0;
    std::int64_t nTimeScale = 0;
    std::int64_t nSequence = 0;
    EFrameGuiderMode eMode = EFrameGuiderMode::SingleTarget;
    bool bDroppedEarlierFrames = false;
    bool bHasJoystickGuide = false;
    TFrameGuiderJoystickValue stJoystickGuide;

    bool bReqMoveTo = false;
    TFrameGuiderMoveToCommand stMoveTo;

    bool bReqZoom = false;
    TFrameGuiderZoomCommand stZoomSpd;

    bool bReqFNo = false;
    TFrameGuiderFNoCommand stFNo;

    bool bReqShutter = false;
    TFrameGuiderShutterCommand stShutter;

    bool bReqAfPoint = false;
    TFrameGuiderAfPointCommand stAfPoint;

    std::vector<TFrameGuiderObjectPartsSpan> vObjects;
    std::vector<TFrameGuiderObjectPartBox> vParts;
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
    // Queue-depth semantics are implementation-defined. A value of 0 is allowed
    // for implementations that support a direct-drop policy: if backend work is
    // already in flight, ReceiveFrame() may discard the new frame immediately
    // instead of building additional pending work.
    std::uint32_t uMaxQueueDepth = 2;
};

/**
 * @brief Unified frame-guider callback collection.
 *
 * `fnOnResult` is the primary asynchronous output path for guider results.
 * Callers are expected to register callbacks before `Open()`. The result
 * payload is intended to carry both tracking boxes and joystick-guidance
 * information for the same guider context.
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
 * `SetCallbacks()` -> `Open()` -> `ReceiveFrame()` x N -> `Close()`。
 *
 * 当前约定中，caller 应在 `Open()` 前设置 callbacks，尤其是 `fnOnResult`。
 * guider 的主结果语义通过 callback 异步输出，而不是依赖额外轮询接口。
 *
 * ## 帧接收契约（继承自 IFrameRecv）
 * 1. `ReceiveFrame()` 应尽快返回，不得在调用线程内执行长耗时推理。
 * 2. 实现若需异步处理，必须持有 `shared_ptr` 副本，不得依赖调用方栈对象。
 * 3. 在 `Idle` / `Error` 状态下收到帧时，实现应做防御性 no-op，不得崩溃。
 * 4. 若内部队列已满，实现可丢弃旧帧或新帧；某些实现也可选择 0 深度兼容
 *    模式，在后端忙碌时直接丢弃新帧。无论采用哪种策略，都应更新统计并按
 *    实现策略通知。
 *
 * ## 云台控制边界
 * - `IFrameGuider` 只输出 tracking/guidance 结果，不直接执行云台命令。
 * - 云台命令执行必须在 `IPTZCamObj` 实现层完成，以保持装配与生命周期边界清晰。
 *
 * ## 结果坐标语义
 * - 输出框允许超出当前输入帧边界。
 * - 若上层需要显示裁剪、OSD 限幅或屏幕内绘制，应在消费侧自行裁剪。
 *
 * ## 摇杆数值语义
 * - guider 输入侧的 joystick state，以及 guider 输出侧的 joystick guidance，
 *   都应使用统一归一化区间 `[-15000, 15000]`。
 * - 这一数值约定取自 DJI R SDK 当前常用的 joystick 数值语义。
 * - `0` 表示该轴当前无输入或无导引；正负号表示相反方向。
 * - joystick 输入按上层控制链路的推送频率更新，不要求与视频帧率一致。
 * - guider 每次向 tracking 链路提交一帧时，都应同步采样当前缓存 joystick
 *   state，并将其作为这次 tracking 请求的附加上下文一并传下去。
 *   当前 tracker 实现可以忽略该输入，但 guider 不应省略这一步传递。
 * - FoV state 建议按逐帧视频节奏更新（或尽可能接近逐帧），以降低 zoom/
 *   视角变化时的帧级失配。
 * - guider 还应独立缓存当前 `TFrameGuiderFovStat`，并在每次向 tracking 链路
 *   提交一帧时同步采样该 FoV 状态。FoV 与 joystick 是独立输入流，不共享
 *   更新节奏或超时语义。
 */
class IFrameGuider : public IFrameRecv
{
public:
    virtual ~IFrameGuider() = default;

    /**
     * @brief Register callback collection.
     *
     * Recommended to call before `Open()`. Implementations should atomically
     * replace the full callback set. The primary result callback `fnOnResult`
     * is the standard output path for both tracking boxes and joystick-guidance
     * output; implementations may reject `Open()` when it is not provided.
     *
     * @return true Success.
     * @return false Current state does not allow callback rebinding.
     */
    virtual bool SetCallbacks(const TFrameGuiderCallbacks& stCallbacks) = 0;

    /**
     * @brief Update the latest caller-side joystick input state.
     *
     * Implementations should cache only the newest value, overwrite
     * `stValue.nTimestamp` with the update time measured against the current
     * instance's internal clock reference, and clamp each axis into the
     * normalized range `[-15000, 15000]`.
     *
     * Expected cadence: caller-driven control push frequency (independent from
     * video frame rate).
     */
    virtual void updateJoystickStat(const TFrameGuiderJoystickValue& stValue) = 0;

    /**
     * @brief Get the current effective joystick input state.
     *
     * Implementations may return a zeroed value when no fresh cached value is
     * available, for example after timeout expiry. The returned `nTimestamp`
     * represents the cached state's last update time under the instance-local
     * clock reference, or an implementation-defined empty value such as `0`.
     */
    virtual TFrameGuiderJoystickValue getJoystickStat() const = 0;

    /**
     * @brief Update the latest caller-side FoV state.
     *
     * Implementations should cache only the newest value and overwrite
     * `stStat.nTimestamp` with the update time measured against the current
     * instance's internal clock reference.
     *
     * Expected cadence: preferably per-frame (or as close as practical),
     * because FoV is a frame-associated optical context.
     */
    virtual void updateFovStat(const TFrameGuiderFovStat& stStat) = 0;

    /**
     * @brief Get the latest cached FoV state.
     *
     * Unlike joystick state, FoV state is not required to expire to zero in the
     * current design. Implementations should return the latest cached value, or
     * an implementation-defined zero/default value if none has ever been set.
     */
    virtual TFrameGuiderFovStat getFovStat() const = 0;

    virtual void updateZoomStat(std::uint8_t nZoomPercent) { (void)nZoomPercent; }

    virtual void updateIrisStat(float fFNo) { (void)fFNo; }

    virtual void updateOrientationStat(
        std::int16_t nYaw, std::int16_t nRoll, std::int16_t nPitch)
    {
        (void)nYaw; (void)nRoll; (void)nPitch;
    }

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
