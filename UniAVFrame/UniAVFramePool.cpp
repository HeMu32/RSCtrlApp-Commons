#include "UniAVFramePool.h"

#include <cstdlib>
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
            std::free(stBuffer.data);
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
    (void)alignmentBytes;
    if (requestedBytes == 0)
    {
        return {};
    }

    std::lock_guard<std::mutex> lk(m_impl->mtx);
    for (auto it = m_impl->vecFreeBuffers.begin(); it != m_impl->vecFreeBuffers.end(); ++it)
    {
        if (it->sizeBytes >= requestedBytes)
        {
            PoolBuffer stBuffer = *it;
            m_impl->vecFreeBuffers.erase(it);
            return stBuffer;
        }
    }

    std::uint8_t* pData = static_cast<std::uint8_t*>(std::malloc(requestedBytes));
    if (pData == nullptr)
    {
        return {};
    }

    return {pData, requestedBytes};
}

void UniAVFramePool::release(PoolBuffer buffer)
{
    if (buffer.data == nullptr || buffer.sizeBytes == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lk(m_impl->mtx);
    m_impl->vecFreeBuffers.push_back(buffer);
}

}