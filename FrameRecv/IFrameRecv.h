// Heavy header.
// Defines an interface for frame reception.
// Any object that is to receive a UniAVFrame should implement this interface.
// In: UniAVFrames. Out: Specified by implementation.
#pragma once

#include "../UniAVFrame/UniAVFrame.h"
#include <memory>

/// @brief 帧指针类型别名，便于接口签名与实现保持一致。
using TFrameRecvFramePtr = std::shared_ptr<UniAV::UniAVFrame>;

/**
 * @brief 帧接收者基础接口（Heavy）。
 *
 * 任何需要接收 UniAVFrame 的对象均应实现此接口，
 * 包括实时显示输出、目标跟踪处理器、帧多路分发器等。
 *
 * ## 契约
 * 1. **共享所有权**：帧以 `std::shared_ptr` 传入，实现者持有或转发副本均合法。
 * 2. **异步处理**：若实现需要在其他线程处理，应复制 `shared_ptr`（增加引用计数），
 *    不得在此调用返回后继续使用调用方持有的栈变量。
 * 3. **快速返回**：调用方可能在实时/回调线程上发起调用，实现方应尽快返回，
 *    耗时逻辑应移至后台线程。
 * 4. **nullptr 防御**：实现方应对 `spFrame == nullptr` 做防御性 no-op，不得崩溃。
 *
 * ## 媒体类型
 * 接口不区分音频帧与视频帧；实现方根据自身需求通过
 * `UniAV::UniAVFrame::mediaType()` / `hasVideo()` / `hasAudio()` 自行决定处理或忽略。
 *
 * @note 该接口为 Heavy 接口，直接依赖 UniAVFrame 语义，不提供 Lite 包装。
 */
class IFrameRecv
{
public:
    virtual ~IFrameRecv() = default;

    /**
     * @brief 接收一个音视频帧。
     * @param spFrame 待处理帧。实现方应处理 nullptr（no-op，不得崩溃）。
     */
    virtual void ReceiveFrame(const TFrameRecvFramePtr& spFrame) = 0;
};