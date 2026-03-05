// perform the same availability detection that UniAVFrame.cpp does
// so that this translation unit knows whether FFmpeg/BMDSDK headers
// are present.  CMake also sets UNIAVFRAME_HAS_FFMPEG, but BMDSDK needs
// detection here as well.
#if defined(__has_include)
#if __has_include(<libavutil/frame.h>) && __has_include(<libavutil/pixfmt.h>) && __has_include(<libswscale/swscale.h>) && __has_include(<libavcodec/avcodec.h>)
#define UNIAVFRAME_HAS_FFMPEG 1
#else
#define UNIAVFRAME_HAS_FFMPEG 0
#endif

#if __has_include(<DeckLinkAPI.h>)
#define UNIAVFRAME_HAS_BMDSDK 1
#else
#define UNIAVFRAME_HAS_BMDSDK 0
#endif
#else
#define UNIAVFRAME_HAS_FFMPEG 0
#define UNIAVFRAME_HAS_BMDSDK 0
#endif

#include "UniAVFrame_Convert.h"

#include <cstring>
#include <limits>
#if UNIAVFRAME_HAS_FFMPEG
#if defined(__cplusplus)
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#if defined(__cplusplus)
}
#endif
#endif
#if UNIAVFRAME_HAS_BMDSDK
#include <DeckLinkAPI.h>
#endif

namespace UniAV {
namespace Convert {

bool copyPackedRows(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iRowBytes,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride == 0 || iRowBytes <= 0 || iHeight <= 0)
    {
        return false;
    }

    const std::uint8_t* pSrcBase = pSrc;
    if (iSrcStride < 0)
    {
        pSrcBase = pSrc + static_cast<std::ptrdiff_t>(iHeight - 1) * static_cast<std::ptrdiff_t>(-iSrcStride);
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrcBase + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);
        std::memcpy(pDstRow, pSrcRow, static_cast<std::size_t>(iRowBytes));
    }

    return true;
}

bool convertBGRAtoRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrc + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);
        for (std::int32_t iCol = 0; iCol < iWidth; ++iCol)
        {
            const std::uint8_t* pSrcPx = pSrcRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            std::uint8_t* pDstPx = pDstRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            pDstPx[0] = pSrcPx[2];
            pDstPx[1] = pSrcPx[1];
            pDstPx[2] = pSrcPx[0];
            pDstPx[3] = pSrcPx[3];
        }
    }

    return true;
}

bool convertARGBtoRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrc + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);
        for (std::int32_t iCol = 0; iCol < iWidth; ++iCol)
        {
            const std::uint8_t* pSrcPx = pSrcRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            std::uint8_t* pDstPx = pDstRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            pDstPx[0] = pSrcPx[1];
            pDstPx[1] = pSrcPx[2];
            pDstPx[2] = pSrcPx[3];
            pDstPx[3] = pSrcPx[0];
        }
    }

    return true;
}

std::uint8_t clampToByte(std::int32_t iVal)
{
    if (iVal < 0)
    {
        return 0;
    }

    if (iVal > 255)
    {
        return 255;
    }

    return static_cast<std::uint8_t>(iVal);
}

std::uint8_t scaleLimited10To8(std::uint16_t uVal10)
{
    const std::int32_t iScaled = (static_cast<std::int32_t>(uVal10) - 64) * 255 / 876;
    return clampToByte(iScaled);
}

std::uint8_t scaleFull10To8(std::uint16_t uVal10)
{
    const std::int32_t iScaled = static_cast<std::int32_t>(uVal10) * 255 / 1023;
    return clampToByte(iScaled);
}

void yuv8ToRgb8(
    std::uint8_t uY,
    std::uint8_t uCb,
    std::uint8_t uCr,
    std::uint8_t* pOutR,
    std::uint8_t* pOutG,
    std::uint8_t* pOutB)
{
    const std::int32_t iC = static_cast<std::int32_t>(uY) - 16;
    const std::int32_t iD = static_cast<std::int32_t>(uCb) - 128;
    const std::int32_t iE = static_cast<std::int32_t>(uCr) - 128;

    const std::int32_t iY1 = (iC < 0) ? 0 : iC;
    const std::int32_t iR = (298 * iY1 + 409 * iE + 128) >> 8;
    const std::int32_t iG = (298 * iY1 - 100 * iD - 208 * iE + 128) >> 8;
    const std::int32_t iB = (298 * iY1 + 516 * iD + 128) >> 8;

    *pOutR = clampToByte(iR);
    *pOutG = clampToByte(iG);
    *pOutB = clampToByte(iB);
}

void yuv10ToRgb8(
    std::uint16_t uY10,
    std::uint16_t uCb10,
    std::uint16_t uCr10,
    std::uint8_t* pOutR,
    std::uint8_t* pOutG,
    std::uint8_t* pOutB)
{
    const std::int32_t iC = static_cast<std::int32_t>(uY10) - 64;
    const std::int32_t iD = static_cast<std::int32_t>(uCb10) - 512;
    const std::int32_t iE = static_cast<std::int32_t>(uCr10) - 512;

    const std::int32_t iY1 = (iC < 0) ? 0 : iC;

    const std::int32_t iR = (iY1 * 1192 + 1634 * iE + 512) >> 10;
    const std::int32_t iG = (iY1 * 1192 - 401 * iD - 833 * iE + 512) >> 10;
    const std::int32_t iB = (iY1 * 1192 + 2066 * iD + 512) >> 10;

    *pOutR = clampToByte(iR);
    *pOutG = clampToByte(iG);
    *pOutB = clampToByte(iB);
}

std::uint32_t readBigEndianU32(const std::uint8_t* pData)
{
    return (static_cast<std::uint32_t>(pData[0]) << 24) |
           (static_cast<std::uint32_t>(pData[1]) << 16) |
           (static_cast<std::uint32_t>(pData[2]) << 8) |
           static_cast<std::uint32_t>(pData[3]);
}

std::uint32_t readLittleEndianU32(const std::uint8_t* pData)
{
    return static_cast<std::uint32_t>(pData[0]) |
           (static_cast<std::uint32_t>(pData[1]) << 8) |
           (static_cast<std::uint32_t>(pData[2]) << 16) |
           (static_cast<std::uint32_t>(pData[3]) << 24);
}

#if UNIAVFRAME_HAS_FFMPEG
bool convertByFFmpegSws(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight,
    AVPixelFormat emSrcFmt)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    AVFrame stSrcFrame;
    std::memset(&stSrcFrame, 0, sizeof(stSrcFrame));
    stSrcFrame.format = static_cast<int>(emSrcFmt);
    stSrcFrame.width = iWidth;
    stSrcFrame.height = iHeight;
    stSrcFrame.data[0] = const_cast<std::uint8_t*>(pSrc);
    stSrcFrame.linesize[0] = iSrcStride;

    AVFrame stDstFrame;
    std::memset(&stDstFrame, 0, sizeof(stDstFrame));
    stDstFrame.format = AV_PIX_FMT_RGBA;
    stDstFrame.width = iWidth;
    stDstFrame.height = iHeight;
    stDstFrame.data[0] = pDst;
    stDstFrame.linesize[0] = iDstStride;

    SwsContext* pSws = sws_getContext(
        iWidth,
        iHeight,
        emSrcFmt,
        iWidth,
        iHeight,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (pSws == nullptr)
    {
        return false;
    }

    const int iScaled = sws_scale(
        pSws,
        stSrcFrame.data,
        &stSrcFrame.linesize[0],
        0,
        iHeight,
        stDstFrame.data,
        &stDstFrame.linesize[0]);

    sws_freeContext(pSws);
    return (iScaled == iHeight);
}

bool convertBMDV210ToRGBAByCodec(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    const AVCodec* pCodec = avcodec_find_decoder(AV_CODEC_ID_V210);
    if (pCodec == nullptr)
    {
        return false;
    }

    AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
    if (pCodecCtx == nullptr)
    {
        return false;
    }

    pCodecCtx->width = iWidth;
    pCodecCtx->height = iHeight;

    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0)
    {
        avcodec_free_context(&pCodecCtx);
        return false;
    }

    AVPacket* pPkt = av_packet_alloc();
    if (pPkt == nullptr)
    {
        avcodec_free_context(&pCodecCtx);
        return false;
    }

    // 零拷贝: 直接指向源数据; pPkt->buf 为 nullptr, av_packet_free 不会释放 data
    pPkt->data = const_cast<std::uint8_t*>(pSrc);
    pPkt->size = iSrcStride * iHeight;

    AVFrame* pDecFrame = av_frame_alloc();
    if (pDecFrame == nullptr)
    {
        pPkt->data = nullptr;
        pPkt->size = 0;
        av_packet_free(&pPkt);
        avcodec_free_context(&pCodecCtx);
        return false;
    }

    bool bSuccess = false;
    if (avcodec_send_packet(pCodecCtx, pPkt) >= 0)
    {
        if (avcodec_receive_frame(pCodecCtx, pDecFrame) >= 0)
        {
            SwsContext* pSws = sws_getContext(
                iWidth,
                iHeight,
                static_cast<AVPixelFormat>(pDecFrame->format),
                iWidth,
                iHeight,
                AV_PIX_FMT_RGBA,
                SWS_BILINEAR,
                nullptr,
                nullptr,
                nullptr);

            if (pSws != nullptr)
            {
                std::uint8_t* pDstData[4] = {pDst, nullptr, nullptr, nullptr};
                int iDstLinesize[4] = {iDstStride, 0, 0, 0};
                const int iScaled = sws_scale(
                    pSws,
                    pDecFrame->data,
                    pDecFrame->linesize,
                    0,
                    iHeight,
                    pDstData,
                    iDstLinesize);
                sws_freeContext(pSws);
                bSuccess = (iScaled == iHeight);
            }
        }
    }

    av_frame_free(&pDecFrame);
    pPkt->data = nullptr;
    pPkt->size = 0;
    av_packet_free(&pPkt);
    avcodec_free_context(&pCodecCtx);
    return bSuccess;
}
#endif

bool convertBMD2vuyToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrc + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);

        for (std::int32_t iCol = 0; iCol < iWidth; iCol += 2)
        {
            const std::ptrdiff_t nOff = static_cast<std::ptrdiff_t>(iCol / 2) * 4;
            const std::uint8_t uCb = pSrcRow[nOff + 0];
            const std::uint8_t uY0 = pSrcRow[nOff + 1];
            const std::uint8_t uCr = pSrcRow[nOff + 2];
            const std::uint8_t uY1 = pSrcRow[nOff + 3];

            std::uint8_t uR = 0;
            std::uint8_t uG = 0;
            std::uint8_t uB = 0;

            yuv8ToRgb8(uY0, uCb, uCr, &uR, &uG, &uB);
            std::uint8_t* pDstPx0 = pDstRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            pDstPx0[0] = uR;
            pDstPx0[1] = uG;
            pDstPx0[2] = uB;
            pDstPx0[3] = 255;

            if (iCol + 1 < iWidth)
            {
                yuv8ToRgb8(uY1, uCb, uCr, &uR, &uG, &uB);
                std::uint8_t* pDstPx1 = pDstPx0 + 4;
                pDstPx1[0] = uR;
                pDstPx1[1] = uG;
                pDstPx1[2] = uB;
                pDstPx1[3] = 255;
            }
        }
    }

    return true;
}

bool convertBMDV210ToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrc + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);

        for (std::int32_t iCol = 0; iCol < iWidth; iCol += 6)
        {
            const std::ptrdiff_t nOff = static_cast<std::ptrdiff_t>(iCol / 6) * 16;
            const std::uint32_t uWord0 = readLittleEndianU32(pSrcRow + nOff + 0);
            const std::uint32_t uWord1 = readLittleEndianU32(pSrcRow + nOff + 4);
            const std::uint32_t uWord2 = readLittleEndianU32(pSrcRow + nOff + 8);
            const std::uint32_t uWord3 = readLittleEndianU32(pSrcRow + nOff + 12);

            const std::uint16_t uCb0 = static_cast<std::uint16_t>(uWord0 & 0x03FF);
            const std::uint16_t uY0 = static_cast<std::uint16_t>((uWord0 >> 10) & 0x03FF);
            const std::uint16_t uCr0 = static_cast<std::uint16_t>((uWord0 >> 20) & 0x03FF);

            const std::uint16_t uY1 = static_cast<std::uint16_t>(uWord1 & 0x03FF);
            const std::uint16_t uCb2 = static_cast<std::uint16_t>((uWord1 >> 10) & 0x03FF);
            const std::uint16_t uY2 = static_cast<std::uint16_t>((uWord1 >> 20) & 0x03FF);

            const std::uint16_t uCr2 = static_cast<std::uint16_t>(uWord2 & 0x03FF);
            const std::uint16_t uY3 = static_cast<std::uint16_t>((uWord2 >> 10) & 0x03FF);
            const std::uint16_t uCb4 = static_cast<std::uint16_t>((uWord2 >> 20) & 0x03FF);

            const std::uint16_t uY4 = static_cast<std::uint16_t>(uWord3 & 0x03FF);
            const std::uint16_t uCr4 = static_cast<std::uint16_t>((uWord3 >> 10) & 0x03FF);
            const std::uint16_t uY5 = static_cast<std::uint16_t>((uWord3 >> 20) & 0x03FF);

            const std::uint16_t auY[6] = {uY0, uY1, uY2, uY3, uY4, uY5};
            const std::uint16_t auCb[6] = {uCb0, uCb0, uCb2, uCb2, uCb4, uCb4};
            const std::uint16_t auCr[6] = {uCr0, uCr0, uCr2, uCr2, uCr4, uCr4};

            for (std::int32_t iSub = 0; iSub < 6 && (iCol + iSub) < iWidth; ++iSub)
            {
                std::uint8_t uR = 0;
                std::uint8_t uG = 0;
                std::uint8_t uB = 0;
                yuv10ToRgb8(auY[iSub], auCb[iSub], auCr[iSub], &uR, &uG, &uB);
                std::uint8_t* pDstPx = pDstRow + static_cast<std::ptrdiff_t>(iCol + iSub) * 4;
                pDstPx[0] = uR;
                pDstPx[1] = uG;
                pDstPx[2] = uB;
                pDstPx[3] = 255;
            }
        }
    }

    return true;
}

bool convertBMDR210ToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrc + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);

        for (std::int32_t iCol = 0; iCol < iWidth; ++iCol)
        {
            const std::ptrdiff_t nOff = static_cast<std::ptrdiff_t>(iCol) * 4;
            const std::uint32_t uWord = readBigEndianU32(pSrcRow + nOff);

            const std::uint16_t uR10 = static_cast<std::uint16_t>((uWord >> 20) & 0x03FF);
            const std::uint16_t uG10 = static_cast<std::uint16_t>((uWord >> 10) & 0x03FF);
            const std::uint16_t uB10 = static_cast<std::uint16_t>(uWord & 0x03FF);

            std::uint8_t* pDstPx = pDstRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            pDstPx[0] = scaleLimited10To8(uR10);
            pDstPx[1] = scaleLimited10To8(uG10);
            pDstPx[2] = scaleLimited10To8(uB10);
            pDstPx[3] = 255;
        }
    }

    return true;
}

bool convertBMDAy10ToRGBA(
    std::uint8_t* pDst,
    std::int32_t iDstStride,
    const std::uint8_t* pSrc,
    std::int32_t iSrcStride,
    std::int32_t iWidth,
    std::int32_t iHeight)
{
    if (pDst == nullptr || pSrc == nullptr || iDstStride <= 0 || iSrcStride <= 0 || iWidth <= 0 || iHeight <= 0)
    {
        return false;
    }

    for (std::int32_t iRow = 0; iRow < iHeight; ++iRow)
    {
        const std::uint8_t* pSrcRow = pSrc + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iSrcStride);
        std::uint8_t* pDstRow = pDst + static_cast<std::ptrdiff_t>(iRow) * static_cast<std::ptrdiff_t>(iDstStride);
        
        for (std::int32_t iCol = 0; iCol < iWidth; iCol += 2)
        {
            const std::ptrdiff_t nOff = static_cast<std::ptrdiff_t>(iCol / 2) * 8;
            const std::uint32_t uWord0 = readBigEndianU32(pSrcRow + nOff + 0);
            const std::uint32_t uWord1 = readBigEndianU32(pSrcRow + nOff + 4);

            const std::uint16_t uY0 = static_cast<std::uint16_t>(((uWord0 >> 14) & 0x0300) | ((uWord0 >> 24) & 0x00FF));
            const std::uint16_t uCb = static_cast<std::uint16_t>(((uWord0 >> 8) & 0x0300) | ((uWord0 >> 16) & 0x00FF));
            const std::uint16_t uA0 = static_cast<std::uint16_t>(((uWord0 >> 10) & 0x000F) << 6 | (uWord0 & 0x003F));

            const std::uint16_t uY1 = static_cast<std::uint16_t>(((uWord1 >> 14) & 0x0300) | ((uWord1 >> 24) & 0x00FF));
            const std::uint16_t uCr = static_cast<std::uint16_t>(((uWord1 >> 8) & 0x0300) | ((uWord1 >> 16) & 0x00FF));
            const std::uint16_t uA1 = static_cast<std::uint16_t>(((uWord1 >> 10) & 0x000F) << 6 | (uWord1 & 0x003F));

            std::uint8_t uR = 0;
            std::uint8_t uG = 0;
            std::uint8_t uB = 0;

            yuv10ToRgb8(uY0, uCb, uCr, &uR, &uG, &uB);
            std::uint8_t* pDstPx0 = pDstRow + static_cast<std::ptrdiff_t>(iCol) * 4;
            pDstPx0[0] = uR;
            pDstPx0[1] = uG;
            pDstPx0[2] = uB;
            pDstPx0[3] = scaleFull10To8(uA0);

            if (iCol + 1 < iWidth)
            {
                yuv10ToRgb8(uY1, uCb, uCr, &uR, &uG, &uB);
                std::uint8_t* pDstPx1 = pDstPx0 + 4;
                pDstPx1[0] = uR;
                pDstPx1[1] = uG;
                pDstPx1[2] = uB;
                pDstPx1[3] = scaleFull10To8(uA1);
            }
        }
    }

    return true;
}

#if UNIAVFRAME_HAS_BMDSDK

bool beginBMDFrameBytesAccess(IDeckLinkVideoFrame* pSrcFrame, BMDFrameBytesAccess* pOutAccess)
{
    if (pSrcFrame == nullptr || pOutAccess == nullptr)
    {
        return false;
    }

    *pOutAccess = {};

    IDeckLinkVideoBuffer* pVideoBuffer = nullptr;
    if (pSrcFrame->QueryInterface(IID_IDeckLinkVideoBuffer, reinterpret_cast<void**>(&pVideoBuffer)) != S_OK ||
        pVideoBuffer == nullptr)
    {
        return false;
    }

    if (pVideoBuffer->StartAccess(bmdBufferAccessRead) != S_OK)
    {
        pVideoBuffer->Release();
        return false;
    }

    void* pBytes = nullptr;
    if (pVideoBuffer->GetBytes(&pBytes) != S_OK || pBytes == nullptr)
    {
        pVideoBuffer->EndAccess(bmdBufferAccessRead);
        pVideoBuffer->Release();
        return false;
    }

    pOutAccess->pVideoBuffer = pVideoBuffer;
    pOutAccess->pBytes = pBytes;
    pOutAccess->bAccessStarted = true;
    return true;
}

void endBMDFrameBytesAccess(BMDFrameBytesAccess* pAccess)
{
    if (pAccess == nullptr)
    {
        return;
    }

    if (pAccess->pVideoBuffer != nullptr)
    {
        if (pAccess->bAccessStarted)
        {
            pAccess->pVideoBuffer->EndAccess(bmdBufferAccessRead);
        }

        pAccess->pVideoBuffer->Release();
    }

    *pAccess = {};
}
#endif

} // namespace Convert
} // namespace UniAV
