#include "FrameDispatcher.h"

void FrameDispatcher::AddReceiver(std::shared_ptr<IFrameRecv> spReceiver)
{
    if (!spReceiver)
    {
        return;
    }

    std::lock_guard<std::mutex> stLock(m_mtxReceivers);

    // 防重：同一对象不重复注册
    // 注意：这里不主动压缩所有 dead weak_ptr。
    // 当前实现仅在 ReceiveFrame() 的快照阶段做机会式清理，
    // 以避免在注册路径引入额外整理逻辑。
    for (const auto& wpExisting : m_vecReceivers)
    {
        if (auto spExisting = wpExisting.lock())
        {
            if (spExisting.get() == spReceiver.get())
            {
                return;
            }
        }
    }

    m_vecReceivers.emplace_back(spReceiver);
}

void FrameDispatcher::RemoveReceiver(const std::shared_ptr<IFrameRecv>& spReceiver)
{
    if (!spReceiver)
    {
        return;
    }

    std::lock_guard<std::mutex> stLock(m_mtxReceivers);

    m_vecReceivers.erase(
        std::remove_if(
            m_vecReceivers.begin(),
            m_vecReceivers.end(),
            [&spReceiver](const std::weak_ptr<IFrameRecv>& wp)
            {
                auto sp = wp.lock();
                return !sp || sp.get() == spReceiver.get();
            }),
        m_vecReceivers.end());
}

std::size_t FrameDispatcher::ReceiverCount() const
{
    std::lock_guard<std::mutex> stLock(m_mtxReceivers);

    std::size_t nCount = 0u;
    for (const auto& wp : m_vecReceivers)
    {
        if (!wp.expired())
        {
            ++nCount;
        }
    }
    return nCount;
}

void FrameDispatcher::ReceiveFrame(const TFrameRecvFramePtr& spFrame)
{
    // nullptr 防御性 no-op
    if (!spFrame)
    {
        return;
    }

    // 持锁仅用于快照，释放锁后再调用各接收方，避免死锁
    std::vector<std::shared_ptr<IFrameRecv>> vecSnapshot;
    {
        std::lock_guard<std::mutex> stLock(m_mtxReceivers);

        vecSnapshot.reserve(m_vecReceivers.size());
        for (auto& wp : m_vecReceivers)
        {
            if (auto sp = wp.lock())
            {
                vecSnapshot.push_back(std::move(sp));
            }
        }

        // 顺便清理已过期的 weak_ptr
        // 注意：此处采用“拍快照后再解锁分发”的语义。
        // 若某个接收方在解锁后、真正 ReceiveFrame() 前被并发 RemoveReceiver()，
        // 它仍可能收到当前这一帧；移除只保证对后续帧生效。
        m_vecReceivers.erase(
            std::remove_if(
                m_vecReceivers.begin(),
                m_vecReceivers.end(),
                [](const std::weak_ptr<IFrameRecv>& wp) { return wp.expired(); }),
            m_vecReceivers.end());
    }

    for (const auto& spRecv : vecSnapshot)
    {
        spRecv->ReceiveFrame(spFrame);
    }
}
