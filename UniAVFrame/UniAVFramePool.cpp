#include "UniAVFramePool.h"

#include <cstdlib>
#include <new>
#include <mutex>
#include <vector>

namespace UniAV
{

struct UniAVFramePool::Impl
{
    std::mutex mtx;
    std::vector<PoolBuffer> vecFreeBuffers;

    ~Impl()
    {
        for (const PoolBuffer& stBuffer : vecFreeBuffers)
        {
            if (stBuffer.data != nullptr)
            {
                ::operator delete[](stBuffer.data, std::align_val_t(stBuffer.alignmentBytes > 0 ? stBuffer.alignmentBytes : alignof(std::max_align_t)));
            }
        }
        vecFreeBuffers.clear();
    }
};

UniAVFramePool::UniAVFramePool()
    : m_impl(new Impl())
{
}

UniAVFramePool::~UniAVFramePool() = default;

PoolBuffer UniAVFramePool::acquire(std::size_t requestedBytes, std::size_t alignmentBytes)
{
    if (requestedBytes == 0)
    {
        return {};
    }

    std::size_t nAlignment = alignmentBytes;
    if (nAlignment < alignof(std::max_align_t))
    {
        nAlignment = alignof(std::max_align_t);
    }

    std::lock_guard<std::mutex> lk(m_impl->mtx);
    for (auto it = m_impl->vecFreeBuffers.begin(); it != m_impl->vecFreeBuffers.end(); ++it)
    {
        if (it->sizeBytes >= requestedBytes && it->alignmentBytes >= nAlignment)
        {
            PoolBuffer stBuffer = *it;
            m_impl->vecFreeBuffers.erase(it);
            return stBuffer;
        }
    }

    std::uint8_t* pData = static_cast<std::uint8_t*>(::operator new[](requestedBytes, std::align_val_t(nAlignment), std::nothrow));
    if (pData == nullptr)
    {
        return {};
    }

    return {pData, requestedBytes, nAlignment};
}

void UniAVFramePool::release(PoolBuffer buffer)
{
    if (buffer.data == nullptr || buffer.sizeBytes == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lk(m_impl->mtx);
    if (buffer.alignmentBytes < alignof(std::max_align_t))
    {
        buffer.alignmentBytes = alignof(std::max_align_t);
    }
    m_impl->vecFreeBuffers.push_back(buffer);
}

}
