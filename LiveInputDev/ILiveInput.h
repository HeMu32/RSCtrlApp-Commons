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
 * @tparam TFrame 输出帧类型。
 * @tparam TNativeId 设备底层唯一标识类型。
 * @tparam TBackendConfig 后端配置类型。
 */
template <typename TFrame, typename TNativeId, typename TBackendConfig>
class ILiveInputT
{
public:
	virtual ~ILiveInputT() = default;

	/**
	 * @brief 注册回调。应在 Open() 前调用。
	 */
	virtual bool SetCallbacks(const TLiveInputCallbacks<TFrame>& stCallbacks) = 0;

	/**
	 * @brief 打开设备，进入可启动状态。
	 */
	virtual ELiveInputErrorCode Open(const TLiveInputOpenParams<TNativeId, TBackendConfig>& stParams) = 0;

	/**
	 * @brief 关闭设备并释放资源。
	 */
	virtual void Close() = 0;

	/**
	 * @brief 启动采集。
	 */
	virtual ELiveInputErrorCode Start() = 0;

	/**
	 * @brief 停止采集。
	 */
	virtual void Stop() = 0;

	/**
	 * @brief 查询当前运行状态。
	 */
	virtual ELiveInputState State() const = 0;

	/**
	 * @brief 查询当前统计信息。
	 */
	virtual TLiveInputStats Stats() const = 0;

	/**
	 * @brief 获取当前设备描述名（用于日志和 UI）。
	 */
	virtual std::string DeviceName() const = 0;
};

/**
 * @brief 默认帧类型（重型依赖）：统一使用 UniAVFrame。
 */
using TLiveInputFramePtr = std::shared_ptr<UniAV::UniAVFrame>;

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