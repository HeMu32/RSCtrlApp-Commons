#pragma once

#include <cstdint>
#include <cstddef>

#if UNIAVFRAME_HAS_FFMPEG
#if defined(__cplusplus)
extern "C" {
#endif
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#if defined(__cplusplus)
}
#endif
#endif

#if UNIAVFRAME_HAS_BMDSDK
// forward declarations to avoid duplicate IID definitions when DeckLinkAPI
// is included elsewhere.  The headers are already pulled in by UniAVFrame.cpp
// during its own detection, so conversion utilities can rely on the types.
struct IDeckLinkVideoBuffer;
struct IDeckLinkVideoFrame;
#endif

namespace UniAV {
namespace Convert {

bool copyPackedRows(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iRowBytes,
    std::int32_t iHeight);

bool convertBGRAtoRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);

bool convertARGBtoRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);

std::uint8_t clampToByte(std::int32_t iVal);
std::uint8_t scaleLimited10To8(std::uint16_t uVal10);
std::uint8_t scaleFull10To8(std::uint16_t uVal10);

void yuv8ToRgb8(
    std::uint8_t uY,
    std::uint8_t uCb,
    std::uint8_t uCr,
    std::uint8_t* pOutR,
    std::uint8_t* pOutG,
    std::uint8_t* pOutB);

void yuv10ToRgb8(
    std::uint16_t uY10,
    std::uint16_t uCb10,
    std::uint16_t uCr10,
    std::uint8_t* pOutR,
    std::uint8_t* pOutG,
    std::uint8_t* pOutB);

std::uint32_t readBigEndianU32(const std::uint8_t* pData);
std::uint32_t readLittleEndianU32(const std::uint8_t* pData);

#if UNIAVFRAME_HAS_FFMPEG
bool convertByFFmpegSws(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight,
    AVPixelFormat emSrcFmt);

bool convertBMDV210ToRGBAByCodec(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);
#endif

bool convertBMD2vuyToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);

bool convertBMDV210ToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);

bool convertBMDR210ToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);

bool convertBMDAy10ToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight);

#if UNIAVFRAME_HAS_BMDSDK
struct BMDFrameBytesAccess
{
    IDeckLinkVideoBuffer* pVideoBuffer;
    void* pBytes;
    bool bAccessStarted;
};

bool beginBMDFrameBytesAccess(IDeckLinkVideoFrame* pSrcFrame, BMDFrameBytesAccess* pOutAccess);
void endBMDFrameBytesAccess(BMDFrameBytesAccess* pAccess);
#endif

} // namespace Convert
} // namespace UniAV
