#ifndef UNIAVFRAME_POOL_H
#define UNIAVFRAME_POOL_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace UniAV
{

struct PoolBuffer
{
    std::uint8_t* data = nullptr;
    std::size_t sizeBytes = 0;
};

class IUniAVFramePool
{
public:
    virtual ~IUniAVFramePool() = default;

    virtual PoolBuffer acquire(std::size_t requestedBytes, std::size_t alignmentBytes) = 0;
    virtual void release(PoolBuffer buffer) = 0;
};

class UniAVFramePool final : public IUniAVFramePool
{
public:
    UniAVFramePool();
    ~UniAVFramePool() override;

    PoolBuffer acquire(std::size_t requestedBytes, std::size_t alignmentBytes) override;
    void release(PoolBuffer buffer) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}

#endif // UNIAVFRAME_POOL_H