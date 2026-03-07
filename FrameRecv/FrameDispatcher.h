// Heavy header.
// A concrete IFrameRecv implementation that fans out one frame to multiple receivers.
#pragma once

#include "IFrameRecv.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

/**
 * @brief 帧多路分发器（BestEffort 策略）。
 *
 * 实现 IFrameRecv，将收到的每个帧同步地、依次分发给所有已注册的接收方。
 * 采用 BestEffort 策略：遇到已失效的 weak_ptr 自动跳过，不影响其他接收方。
 * 
 * ## 接收方的生命周期
 * 生命周期由外部管理
 *
 * ## 线程安全
 * - `AddReceiver` / `RemoveReceiver` 可在任意线程调用。
 * - `ReceiveFrame` 可在任意线程调用（通常是采集回调线程）。
 * - 持锁仅用于快照订阅表；持锁期间不调用接收方，避免死锁。
 *
 * ## 分发策略
 * 分发为同步调用，假定各接收方能立即返回。
 * `nullptr` 帧将被静默忽略，不转发给任何接收方。
 * 内部以 `std::weak_ptr` 持有接收方，不延长其生命周期；
 * 失效的 weak_ptr 在每次 `ReceiveFrame` 时机会式清理。
 * 若系统长期只有注册/注销而几乎不投递帧，内部表可能暂时保留已过期项；
 * 当前实现优先保证实时回调路径简单，不额外引入后台清理机制。
 *
 * ## 并发边界
 * `ReceiveFrame()` 采用“先拍快照、后解锁分发”的语义。
 * 因此若某个接收方在一次分发进行中被并发 `RemoveReceiver()`，
 * 该接收方仍可能收到这一个已经在途（in-flight）的帧；移除保证从后续帧开始生效。
 */
class FrameDispatcher : public IFrameRecv
{
public:
    FrameDispatcher()  = default;
    ~FrameDispatcher() override = default;

    FrameDispatcher(const FrameDispatcher&)            = delete;
    FrameDispatcher& operator=(const FrameDispatcher&) = delete;

    /**
     * @brief 注册一个接收方。
     * @param spReceiver 接收方的 shared_ptr；内部以 weak_ptr 持有，不延长生命周期。
     *        若 spReceiver 为 nullptr 或该对象已注册，则忽略（防重）。
     */
    void AddReceiver(std::shared_ptr<IFrameRecv> spReceiver);

    /**
     * @brief 注销一个接收方。
     * @param spReceiver 要注销的接收方。若不存在则忽略。
     *
     * @note 调用者必须持有传入对象的 `shared_ptr`。
     *       本方法通过比较传入指针与内部 weak_ptr 目标来查找并移除项，
     *       因此若只有裸指针或 weak_ptr，无法完成注销。
     */
    void RemoveReceiver(const std::shared_ptr<IFrameRecv>& spReceiver);

    /**
     * @brief 当前活跃接收方数量（已过期的 weak_ptr 不计入）。
     */
    std::size_t ReceiverCount() const;

    /**
     * @brief 将帧同步分发给所有已注册且仍存活的接收方。
     * @param spFrame 若为 nullptr，静默忽略，不转发。
     * @note 分发使用快照语义；与并发 RemoveReceiver() 竞态时，
     *       被移除接收方仍可能收到当前这一帧，但不会收到后续帧。
     */
    void ReceiveFrame(const TFrameRecvFramePtr& spFrame) override;

private:
    mutable std::mutex                        m_mtxReceivers;
    std::vector<std::weak_ptr<IFrameRecv>>    m_vecReceivers;
};
