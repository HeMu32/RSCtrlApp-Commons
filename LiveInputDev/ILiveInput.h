// Heavy header.
// Defines callback-based interfaces for A/V capture.
// This module is intentionally heavy and directly couples UniAVFrame semantics.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "../UniAVFrame/UniAVFrame.h"

/**
 * @brief 采集侧统一错误码。
 */
enum class ELiveInputErrorCode : std::uint32_t
{
	Ok = 0,
	InvalidArgument,
	DeviceNotFound,
	DeviceBusy,
	DeviceDisconnected,
	UnsupportedMode,
	BackendFailure,
	Timeout,
	InternalError
};

/**
 * @brief 输入源运行状态。
 */
enum class ELiveInputState : std::uint8_t
{
	Idle = 0,
	Opening,
	Streaming,
	Stopping,
	Error
};

/**
 * @brief 音视频捕获模式描述。
 */
struct TLiveInputCaptureMode
{
	std::int32_t nVideoWidth = 0;
	std::int32_t nVideoHeight = 0;
	std::int32_t nVideoFpsNum = 0;
	std::int32_t nVideoFpsDen = 1;
	bool bEnableVideo = true;
	bool bEnableAudio = false;
	std::int32_t nAudioSampleRate = 0;
	std::int32_t nAudioChannels = 0;
};

/**
 * @brief 采集时序信息。
 */
struct TLiveInputFrameTiming
{
	std::int64_t nCaptureMonotonicUs = 0;
	std::int64_t nFrameTime = 0;
	std::int64_t nFrameDuration = 0;
	std::int64_t nTimeScale = 0;
	bool bHardwareTimestamp = false;
};

/**
 * @brief 采集统计信息（用于背压与掉帧观测）。
 */
struct TLiveInputStats
{
	std::int64_t nFramesIn = 0;
	std::int64_t nFramesDropped = 0;
	std::int64_t nFramesDelivered = 0;
	std::int32_t nApproxQueueDepth = 0;
};

/**
 * @brief 打开设备参数。
 * @tparam TNativeId 设备底层唯一标识类型。
 * @tparam TBackendConfig 后端特定配置，由具体实现定义。
 */
template <typename TNativeId, typename TBackendConfig>
struct TLiveInputOpenParams
{
	std::int32_t nDeviceIndex = -1;
	TNativeId nativeDeviceId{};
	TLiveInputCaptureMode stMode;
	TBackendConfig stBackendConfig{};
	bool bPreferLowLatency = false;
};

/**
 * @brief 帧回调上下文。
 */
template <typename TFrame>
struct TLiveInputFrameContext
{
	TFrame frame;
	TLiveInputFrameTiming stTiming;
	std::int64_t nSequence = 0;
	bool bIsKeyFrameHint = false;
};

/**
 * @brief 统一回调集合。
 * @tparam TFrame 传输给调用方的帧类型。
 */
template <typename TFrame>
struct TLiveInputCallbacks
{
	std::function<void(const TLiveInputFrameContext<TFrame>&)> fnOnFrame;
	std::function<void(ELiveInputState, const TLiveInputStats&)> fnOnState;
	std::function<void(ELiveInputErrorCode, const std::string&)> fnOnError;
	std::function<void(const TLiveInputStats&)> fnOnStats;
};

/**
 * @brief 统一回调驱动采集模板接口。
 *
 * ### 状态机
 * @code
 *  Idle ──Open()──> Opening ──成功──> Idle
 *                          └─失败──> Idle  (fnOnError 触发)
 *  Idle ──Start()─> Opening ──成功──> Streaming
 *                          └─失败──> Idle  (fnOnError 触发)
 *  Streaming ──Stop()──> Stopping ──> Idle
 *  任意状态 ──Close()──> Idle
 * @endcode
 *
 * ### 错误码约定
 * - 调用时设备状态不满足前置条件（非 Idle）：返回 `DeviceBusy`。
 * - 参数本身非法（如空句柄、无效索引）：返回 `InvalidArgument`。
 * - 设备不存在或已断开：返回 `DeviceNotFound` / `DeviceDisconnected`。
 * - 后端 SDK / 驱动调用失败：返回 `BackendFailure`。
 * - 格式/模式不受支持：返回 `UnsupportedMode`。
 * - 非致命子组件启动失败（例如仅音频失败、视频继续）：
 *   不中止主流程，但须通过 `fnOnError` 通知调用方，并降级继续。
 *
 * ### 线程安全
 * - `State()`、`Stats()`、`DeviceName()` 可在任意线程调用。
 * - `fnOnFrame`、`fnOnState`、`fnOnError` 可能在后端内部线程中被调用，
 *   调用方须自行保证回调内部的线程安全。
 * 
 * @todo add device friendly name string getter.
 *
 * @tparam TFrame 输出帧类型。
 * @tparam TNativeId 设备底层唯一标识类型。
 * @tparam TBackendConfig 后端配置类型。
 */
/**
 * @brief 默认帧类型（重型依赖）：统一使用 UniAVFrame。
 */
using TLiveInputFramePtr = std::shared_ptr<UniAV::UniAVFrame>;

/**
 * @brief 非模板基础输入接口。
 *
 * 该接口允许在不关心具体后端配置类型的情况下对输入对象进行统一管理。
 * 例如 `LocalPTZCam` 仅需要注册回调、查询状态等，因此使用该基类实现类型擦除。
 */
class ILiveInputBase
{
public:
    virtual ~ILiveInputBase() = default;

    virtual bool SetCallbacks(const TLiveInputCallbacks<TLiveInputFramePtr>& stCallbacks) = 0;
    virtual void Close() = 0;
    virtual ELiveInputErrorCode Start() = 0;
    virtual void Stop() = 0;
    virtual ELiveInputState State() const = 0;
    virtual TLiveInputStats Stats() const = 0;
    virtual std::string DeviceName() const = 0;
};

template <typename TFrame, typename TNativeId, typename TBackendConfig>
class ILiveInputT : public ILiveInputBase
{
public:
	virtual ~ILiveInputT() = default;

	/**
	 * @brief 注册回调集合。
	 *
	 * **前置条件**：State() == Idle。
	 * **若已处于 Streaming 或 Stopping**：立即返回 false，不修改已注册的回调。
	 * 调用方应在 Open() 之前完成回调注册，以确保 Open() 期间的状态/错误通知
	 * 能够被正确接收。
	 *
	 * @return true 注册成功；false 当前状态不允许修改回调。
	 */
	virtual bool SetCallbacks(const TLiveInputCallbacks<TFrame>& stCallbacks) = 0;

	/**
	 * @brief 打开设备，完成参数验证和资源预分配，进入可 Start() 的状态。
	 *
	 * **前置条件**：State() == Idle。
	 * **状态流转**：Idle → Opening → Idle（成功 或 失败后均回 Idle）。
	 * **错误通知**：失败时触发 fnOnError（若已注册），同时返回对应错误码。
	 * **成功后**：State() == Idle，可立即调用 Start()。
	 * **失败后**：State() == Idle，内部资源已全部清理，可重新调用 Open()。
	 *
	 * @return Ok / DeviceBusy / DeviceNotFound / InvalidArgument /
	 *         UnsupportedMode / BackendFailure
	 */
	virtual ELiveInputErrorCode Open(const TLiveInputOpenParams<TNativeId, TBackendConfig>& stParams) = 0;

	/**
	 * @brief 停止采集（若正在进行）并释放全部设备资源，State() 回到 Idle。
	 *
	 * **可在任意状态下调用**，包括 Idle（此时为空操作）。
	 * Close() 内部优先调用 Stop() 再释放资源，保证采集线程安全退出。
	 * Close() 之后可再次调用 Open() 重新打开设备。
	 */
	virtual void Close() = 0;

	/**
	 * @brief 启动硬件采集，开始通过 fnOnFrame 分发帧。
	 *
	 * **前置条件**：State() == Idle（即已成功调用过 Open()）。
	 * **状态流转**：Idle → Opening → Streaming（成功）
	 *             或 Idle → Opening → Idle（失败，fnOnError 触发）。
	 * **非致命子组件失败**（例如音频启动失败、视频正常）：
	 *   实现必须通过 fnOnError 通知调用方，但继续进入 Streaming 状态，
	 *   不得以此为由返回失败或停止视频采集。
	 * **若 State() != Idle**：立即返回 DeviceBusy，不改变状态。
	 *
	 * @return Ok / DeviceBusy / BackendFailure / UnsupportedMode / InternalError
	 */
	virtual ELiveInputErrorCode Start() = 0;

	/**
	 * @brief 停止硬件采集，保留已 Open() 的设备资源。
	 *
	 * **若 State() 不为 Streaming 或 Opening**：立即返回（空操作）。
	 * **状态流转**：Streaming → Stopping → Idle。
	 * Stop() 后可再次调用 Start() 而无需重新 Open()。
	 */
	virtual void Stop() = 0;

	/**
	 * @brief 查询当前运行状态（线程安全）。
	 */
	virtual ELiveInputState State() const = 0;

	/**
	 * @brief 查询当前统计信息（线程安全）。
	 */
	virtual TLiveInputStats Stats() const = 0;

	/**
	 * @brief 获取当前设备描述名，用于日志和 UI（线程安全）。
	 * @return Open() 成功后的设备名称；Open() 前为空字符串。
	 */
	virtual std::string DeviceName() const = 0;
};

/**
 * @brief 统一默认设备 ID 类型。
 * @note 仅为默认占位类型，具体实现可替换为更合适的 native id 类型。
 */
using TLiveInputNativeId = std::string;

/**
 * @brief 默认后端配置占位类型。
 * @note 具体设备实现应自行定义并传入自有配置类型。
 */
struct TLiveInputBackendConfig
{
};

/**
 * @brief 默认重型回调类型。
 */
using TLiveInputDefaultCallbacks = TLiveInputCallbacks<TLiveInputFramePtr>;

/**
 * @brief 默认重型采集接口类型。
 */
using ILiveInput = ILiveInputT<TLiveInputFramePtr, TLiveInputNativeId, TLiveInputBackendConfig>;