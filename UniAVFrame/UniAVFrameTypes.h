#ifndef UNIAVFRAME_TYPES_H
#define UNIAVFRAME_TYPES_H

#include <cstddef>
#include <cstdint>

namespace UniAV
{

enum class MediaType
{
    Unknown = 0,
    Video,
    Audio
};

enum class InputBackend
{
    Unknown = 0,
    FFmpeg,
    Qt,
    OpenCV,
    BMDSDK
};

enum class PixelFormat
{
    Unknown = 0,
    RGBA8888
};

enum class UniAVError
{
    Ok = 0,
    InvalidArgument,
    UnsupportedFormat,
    PoolAllocationFailed,
    CacheBuildFailed,
    NotImplemented
};

struct FrameTimestamp
{
    // Optional metadata carrier only.
    // For audio frames this timestamp may be independent from video timeline,
    // and UniAVFrame does not perform A/V alignment.
    std::int64_t frameTime = 0;
    std::int64_t frameDuration = 0;
    std::int64_t timeScale = 0;
};

struct VideoDesc
{
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t strideBytes = 0;
    PixelFormat pixelFormat = PixelFormat::Unknown;
};

struct AudioDesc
{
    // Metadata for the current audio frame payload.
    // sampleCount is per-frame sample count, not a timeline-aligned value.
    std::int32_t sampleRate = 0;
    std::int32_t channels = 0;
    std::int32_t bytesPerSample = 0;
    std::int32_t sampleCount = 0;
};

struct MemoryView
{
    std::uint8_t* data = nullptr;
    std::size_t sizeBytes = 0;
};

struct RGBAImageView
{
    std::uint8_t* data = nullptr;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t strideBytes = 0;
};

struct NativeFrameHandle
{
    InputBackend backend = InputBackend::Unknown;
    void* nativePtr = nullptr;
};

struct UniAVFrameCreateOptions
{
    bool copyInputIfNeeded = false;
};

}

#endif // UNIAVFRAME_TYPES_H