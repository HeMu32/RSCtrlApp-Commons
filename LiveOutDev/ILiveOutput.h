// Heavy header.
// Defines an interface for A/V output. Inherits IFrameRecv.
// In: UniAVFrames. Out: display, encoder, HW output, etc.
#pragma once

#include <cstdint>
#include <string>

#include "../FrameRecv/IFrameRecv.h"

/**
 * @brief 输出端视频格式配置。
 *
 * 绝大多数输出实现（显示窗口、编码器、硬件输出）均需要目标分辨率与帧率信息。
 * 不关心格式配置的实现可在 `SetVideoFormat()` 中返回 `ELiveOutputError::UnsupportedFormat`，
 * 并在 `GetVideoFormat()` 中返回 `false`。
 */
struct TLiveOutputVideoFormat
{
    std::int32_t nWidth  = 0;   ///< 目标宽度（像素）
    std::int32_t nHeight = 0;   ///< 目标高度（像素）
    std::int32_t nFpsNum = 0;   ///< 帧率分子
    std::int32_t nFpsDen = 1;   ///< 帧率分母（默认 1）
    std::string sPixelFormat;   ///< 像素格式标识，可空
};

/**
 * @brief 输出端统一错误码。
 */
enum class ELiveOutputError : std::uint32_t
{
    Ok               = 0,  ///< 操作成功
    NotOpen,               ///< 尚未打开，无法执行该操作
    AlreadyOpen,           ///< 已处于打开状态，不可重复打开
    InvalidConfig,         ///< 配置参数无效（硬性失败）
    UnsupportedFormat,     ///< 实现不支持格式配置（软性，非致命）
    BackendFailure         ///< 后端内部错误
};

/**
 * @brief 输出端运行状态。
 */
enum class ELiveOutputState : std::uint8_t
{
    Idle  = 0,  ///< 未打开
    Open,       ///< 已打开，正常运行
    Error       ///< 发生后端错误
};

/**
 * @brief 音视频输出端基础接口（Heavy）。
 *
 * 继承 `IFrameRecv`，任何实现均可直接注册到 `FrameDispatcher`。
 *
 * ## 生命周期
 * 推荐调用顺序：`SetVideoFormat()` → `Open()` → `ReceiveFrame()` × N → `Close()`。
 * 接口不强制约束调用顺序，但实现方应处理反序情况（如 `Open()` 后再 `SetVideoFormat()`）。
 *
 * ## 帧接收契约（继承自 IFrameRecv）
 * 1. **共享所有权**：帧以 `std::shared_ptr` 传入，实现者可持有或转发副本。
 * 2. **快速返回**：`ReceiveFrame()` 应尽快返回，耗时逻辑移至后台线程。
 * 3. **nullptr 防御**：`spFrame == nullptr` 时做 no-op，不得崩溃。
 * 4. **状态防御**：在 `Idle` / `Error` 状态下收到帧时，实现应做 no-op，不得崩溃。
 *
 * ## 格式配置
 * `SetVideoFormat()` 与 `Open()` 解耦，允许先配置格式再打开，也允许打开后重新协商。
 * 不关心格式的实现应返回 `ELiveOutputError::UnsupportedFormat`（非致命）。
 *
 * @note 该接口为 Heavy 接口，直接依赖 IFrameRecv（进而依赖 UniAVFrame 语义）。
 */
class ILiveOutput : public IFrameRecv
{
public:
    virtual ~ILiveOutput() = default;

    /**
     * @brief 打开并激活输出端。
     * @param sConfig 后端特定配置字符串（语义由实现定义，如窗口标题、文件路径、URL 等）。
     * @return `Ok` 表示成功；`AlreadyOpen` 表示重复打开；其余为错误。
     * @note 推荐在调用前先通过 `SetVideoFormat()` 配置好格式。
     */
    virtual ELiveOutputError Open(const std::string& sConfig) = 0;

    /**
     * @brief 关闭输出端并释放资源。
     * @post 状态回到 `Idle`，内部帧缓冲应被清空。
     * @note 幂等：在 `Idle` 状态下调用应为 no-op。
     */
    virtual void Close() = 0;

    /**
     * @brief 查询输出端是否已打开。
     * @return `true` 当且仅当当前状态为 `Open`。
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief 查询当前完整状态。
     */
    virtual ELiveOutputState GetState() const = 0;

    /**
     * @brief 配置目标视频格式（分辨率、帧率）。
     * @param stFormat 目标格式描述。
     * @return `Ok` 表示已接受；`UnsupportedFormat` 表示实现不关心格式配置（非致命）；
     *         `InvalidConfig` 表示参数有效性错误（如宽高为负）。
     * @note 可在 `Open()` 之前或之后调用；实现应处理两种时序。
     */
    virtual ELiveOutputError SetVideoFormat(const TLiveOutputVideoFormat& stFormat) = 0;

    /**
     * @brief 查询当前已配置的视频格式。
     * @param[out] stFormat 若返回 `true`，填入当前格式。
     * @return `true` 表示有效格式已配置；`false` 表示未配置或实现不支持格式查询。
     */
    virtual bool GetVideoFormat(TLiveOutputVideoFormat& stFormat) const = 0;
};