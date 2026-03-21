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

enum class AudioSampleFormat
{
    Unknown = 0,
    U8,
    S16,
    S32,
    F32,
    F64
};

enum class AudioSampleLayout
{
    Unknown = 0,
    Interleaved,
    Planar
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
    AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
    AudioSampleLayout sampleLayout = AudioSampleLayout::Unknown;
};

// May pay attention to byte order when ported to other platforms. 
struct BMDAudioPacketDesc
{
    // Expected BMD capture sample rate, e.g. 48000.
    std::int32_t sampleRate = 0;
    // Channel count configured in EnableAudioInput.
    std::int32_t channels = 0;
    // BMD sample bit width. Current valid values: 16, 32.
    std::int32_t sampleTypeBits = 0;
    // Time scale for GetPacketTime. Set <=0 to skip packet-time query.
    std::int64_t packetTimeScale = 0;
};

struct MemoryView
{
    std::uint8_t* data = nullptr;
    std::size_t sizeBytes = 0;
};

struct ConstMemoryView
{
    const std::uint8_t* data = nullptr;
    std::size_t sizeBytes = 0;
};

struct RGBAImageView
{
    std::uint8_t* data = nullptr;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t strideBytes = 0;
};

struct ConstRGBAImageView
{
    const std::uint8_t* data = nullptr;
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
