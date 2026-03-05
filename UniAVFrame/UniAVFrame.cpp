#include "UniAVFrame.h"

#include <cstring>
#include <limits>

#if defined(__has_include)
#if __has_include(<libavutil/frame.h>) && __has_include(<libavutil/pixfmt.h>) && __has_include(<libswscale/swscale.h>) && __has_include(<libavcodec/avcodec.h>)
#define UNIAVFRAME_HAS_FFMPEG 1
#if defined(slots)
#pragma push_macro("slots")
#undef slots
#define UNIAVFRAME_RESTORE_QT_SLOTS 1
#endif
#if defined(signals)
#pragma push_macro("signals")
#undef signals
#define UNIAVFRAME_RESTORE_QT_SIGNALS 1
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#if defined(__cplusplus)
}
#endif
#if defined(UNIAVFRAME_RESTORE_QT_SIGNALS)
#pragma pop_macro("signals")
#undef UNIAVFRAME_RESTORE_QT_SIGNALS
#endif
#if defined(UNIAVFRAME_RESTORE_QT_SLOTS)
#pragma pop_macro("slots")
#undef UNIAVFRAME_RESTORE_QT_SLOTS
#endif
#else
#define UNIAVFRAME_HAS_FFMPEG 0
#endif

#if __has_include(<QImage>)
#define UNIAVFRAME_HAS_QT 1
#include <QImage>
#else
#define UNIAVFRAME_HAS_QT 0
#endif

#if __has_include(<opencv2/core.hpp>) && __has_include(<opencv2/imgproc.hpp>)
#define UNIAVFRAME_HAS_OPENCV 1
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#else
#define UNIAVFRAME_HAS_OPENCV 0
#endif

#if __has_include(<DeckLinkAPI.h>)
#define UNIAVFRAME_HAS_BMDSDK 1
#include <DeckLinkAPI.h>
#else
#define UNIAVFRAME_HAS_BMDSDK 0
#endif
#else
#define UNIAVFRAME_HAS_FFMPEG 0
#define UNIAVFRAME_HAS_QT 0
#define UNIAVFRAME_HAS_OPENCV 0
#define UNIAVFRAME_HAS_BMDSDK 0
#endif

#if UNIAVFRAME_HAS_FFMPEG
#ifndef UNIAVFRAME_BMDSDK_PREFER_FFMPEG
#define UNIAVFRAME_BMDSDK_PREFER_FFMPEG 1
#endif
#else
#define UNIAVFRAME_BMDSDK_PREFER_FFMPEG 0
#endif

#include "UniAVFrame_Convert.h"

namespace UniAV
{


static bool copyPackedRows(
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

static bool convertBGRAtoRGBA(
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

static bool convertARGBtoRGBA(
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

static std::uint8_t clampToByte(std::int32_t iVal)
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

static std::uint8_t scaleLimited10To8(std::uint16_t uVal10)
{
    const std::int32_t iScaled = (static_cast<std::int32_t>(uVal10) - 64) * 255 / 876;
    return clampToByte(iScaled);
}

static std::uint8_t scaleFull10To8(std::uint16_t uVal10)
{
    const std::int32_t iScaled = static_cast<std::int32_t>(uVal10) * 255 / 1023;
    return clampToByte(iScaled);
}

static void yuv8ToRgb8(
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

static void yuv10ToRgb8(
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

static std::uint32_t readBigEndianU32(const std::uint8_t* pData)
{
    return (static_cast<std::uint32_t>(pData[0]) << 24) |
           (static_cast<std::uint32_t>(pData[1]) << 16) |
           (static_cast<std::uint32_t>(pData[2]) << 8) |
           static_cast<std::uint32_t>(pData[3]);
}

static std::uint32_t readLittleEndianU32(const std::uint8_t* pData)
{
    return static_cast<std::uint32_t>(pData[0]) |
           (static_cast<std::uint32_t>(pData[1]) << 8) |
           (static_cast<std::uint32_t>(pData[2]) << 16) |
           (static_cast<std::uint32_t>(pData[3]) << 24);
}

#if UNIAVFRAME_HAS_FFMPEG
static bool convertByFFmpegSws(
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

/**
 * @brief 使用 FFmpeg v210 编解码器解码 BMD 10-bit YUV (v210) 帧并转换为 RGBA8888.
 *
 * v210 在 FFmpeg 中是一个视频编解码器 (AV_CODEC_ID_V210), 而非 swscale
 * 可直接处理的原始像素格式. 本函数走 avcodec_send_packet /
 * avcodec_receive_frame 路径解码, 再通过 sws_scale 输出到目标缓冲区.
 *
 * @param pDst       目标 RGBA 缓冲区
 * @param iDstStride 目标行字节步长
 * @param pSrc       源 v210 数据 (BMD GetBytes 所得指针, 生命周期由调用者保证)
 * @param iSrcStride 源行字节步长
 * @param iWidth     图像宽度 (像素)
 * @param iHeight    图像高度 (像素)
 * @return true 转换成功; false 编解码器不可用或解码/缩放失败.
 */
static bool convertBMDV210ToRGBAByCodec(
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

static bool convertBMD2vuyToRGBA(
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

static bool convertBMDV210ToRGBA(
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

static bool convertBMDR210ToRGBA(
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

static bool convertBMDAy10ToRGBA(
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

// BMDFrameBytesAccess structure and associated helpers moved to
// UniAVFrame_Convert; this block removed to avoid duplication.
std::atomic<std::int64_t> UniAVFrame::s_activeCount{0};

UniAVFrame::UniAVFrame()
{
    ++s_activeCount;
}

UniAVFrame::~UniAVFrame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rgbaOwner.reset();
    m_rgbaCache = {};
    --s_activeCount;
}

MediaType UniAVFrame::mediaType() const
{
    return m_mediaType;
}

InputBackend UniAVFrame::backend() const
{
    return m_backend;
}

FrameTimestamp UniAVFrame::timestamp() const
{
    return m_timestamp;
}

bool UniAVFrame::hasVideo() const
{
    return m_hasVideo;
}

VideoDesc UniAVFrame::videoDesc() const
{
    return m_videoDesc;
}

bool UniAVFrame::hasAudio() const
{
    return m_hasAudio;
}

AudioDesc UniAVFrame::audioDesc() const
{
    return m_audioDesc;
}

NativeFrameHandle UniAVFrame::nativeHandle() const
{
    return m_native;
}

MemoryView UniAVFrame::originalMemory() const
{
    return m_originalMemory;
}

RGBAImageView UniAVFrame::rgbaCacheView() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rgbaCache;
}

std::shared_ptr<const std::uint8_t> UniAVFrame::rgbaCacheRef() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rgbaOwner;
}

std::int64_t UniAVFrame::activeFrameCount()
{
    return s_activeCount.load();
}

UniAVError UniAVFrame::buildRGBA8888Cache()
{
    if (!m_hasVideo)
    {
        return UniAVError::InvalidArgument;
    }

    if (!m_pool)
    {
        return UniAVError::InvalidArgument;
    }

    std::int32_t iWidth = 0;
    std::int32_t iHeight = 0;
    std::int32_t iDstStride = 0;

    if (m_backend == InputBackend::FFmpeg)
    {
#if UNIAVFRAME_HAS_FFMPEG
        AVFrame* pSrcFrame = static_cast<AVFrame*>(m_native.nativePtr);
        if (pSrcFrame == nullptr || pSrcFrame->width <= 0 || pSrcFrame->height <= 0)
        {
            return UniAVError::InvalidArgument;
        }

        iWidth = pSrcFrame->width;
        iHeight = pSrcFrame->height;
        iDstStride = iWidth * 4;

        const std::size_t nNeededBytes = static_cast<std::size_t>(iDstStride) * static_cast<std::size_t>(iHeight);
        PoolBuffer stBuffer = m_pool->acquire(nNeededBytes, 32);
        if (stBuffer.data == nullptr || stBuffer.sizeBytes < nNeededBytes)
        {
            return UniAVError::PoolAllocationFailed;
        }

        bool bSuccess = false;
        if (pSrcFrame->format == AV_PIX_FMT_RGBA)
        {
            bSuccess = Convert::copyPackedRows(
                stBuffer.data,
                iDstStride,
                pSrcFrame->data[0],
                pSrcFrame->linesize[0],
                iDstStride,
                iHeight);
        }
        else
        {
            SwsContext* pSws = sws_getContext(
                iWidth,
                iHeight,
                static_cast<AVPixelFormat>(pSrcFrame->format),
                iWidth,
                iHeight,
                AV_PIX_FMT_RGBA,
                SWS_BILINEAR,
                nullptr,
                nullptr,
                nullptr);

            if (pSws != nullptr)
            {
                std::uint8_t* pDstData[4] = {stBuffer.data, nullptr, nullptr, nullptr};
                int iDstLinesize[4] = {iDstStride, 0, 0, 0};
                const int iScaled = sws_scale(
                    pSws,
                    pSrcFrame->data,
                    pSrcFrame->linesize,
                    0,
                    iHeight,
                    pDstData,
                    iDstLinesize);
                sws_freeContext(pSws);
                bSuccess = (iScaled == iHeight);
            }
        }

        if (!bSuccess)
        {
            m_pool->release(stBuffer);
            return UniAVError::CacheBuildFailed;
        }

        std::shared_ptr<std::uint8_t> spOwner(
            stBuffer.data,
            [spPool = m_pool, stBuffer](std::uint8_t*)
            {
                if (spPool)
                {
                    spPool->release(stBuffer);
                }
            });

        m_rgbaOwner = spOwner;
        m_rgbaCache = {spOwner.get(), iWidth, iHeight, iDstStride};
        m_videoDesc = {iWidth, iHeight, iDstStride, PixelFormat::RGBA8888};
        return UniAVError::Ok;
#else
        return UniAVError::UnsupportedFormat;
#endif
    }

    if (m_backend == InputBackend::Qt)
    {
#if UNIAVFRAME_HAS_QT
        QImage* pSrcImage = static_cast<QImage*>(m_native.nativePtr);
        if (pSrcImage == nullptr || pSrcImage->isNull())
        {
            return UniAVError::InvalidArgument;
        }

        QImage stRGBA = pSrcImage->convertToFormat(QImage::Format_RGBA8888);
        if (stRGBA.isNull())
        {
            return UniAVError::CacheBuildFailed;
        }

        iWidth = stRGBA.width();
        iHeight = stRGBA.height();
        iDstStride = stRGBA.bytesPerLine();

        const std::size_t nNeededBytes = static_cast<std::size_t>(iDstStride) * static_cast<std::size_t>(iHeight);
        PoolBuffer stBuffer = m_pool->acquire(nNeededBytes, 32);
        if (stBuffer.data == nullptr || stBuffer.sizeBytes < nNeededBytes)
        {
            return UniAVError::PoolAllocationFailed;
        }

        const bool bSuccess = Convert::copyPackedRows(
            stBuffer.data,
            iDstStride,
            stRGBA.bits(),
            stRGBA.bytesPerLine(),
            iDstStride,
            iHeight);
        if (!bSuccess)
        {
            m_pool->release(stBuffer);
            return UniAVError::CacheBuildFailed;
        }

        std::shared_ptr<std::uint8_t> spOwner(
            stBuffer.data,
            [spPool = m_pool, stBuffer](std::uint8_t*)
            {
                if (spPool)
                {
                    spPool->release(stBuffer);
                }
            });

        m_rgbaOwner = spOwner;
        m_rgbaCache = {spOwner.get(), iWidth, iHeight, iDstStride};
        m_videoDesc = {iWidth, iHeight, iDstStride, PixelFormat::RGBA8888};
        return UniAVError::Ok;
#else
        return UniAVError::UnsupportedFormat;
#endif
    }

    if (m_backend == InputBackend::OpenCV)
    {
#if UNIAVFRAME_HAS_OPENCV
        cv::Mat* pSrcMat = static_cast<cv::Mat*>(m_native.nativePtr);
        if (pSrcMat == nullptr || pSrcMat->empty())
        {
            return UniAVError::InvalidArgument;
        }

        cv::Mat stRGBA;
        if (pSrcMat->type() == CV_8UC4)
        {
            cv::cvtColor(*pSrcMat, stRGBA, cv::COLOR_BGRA2RGBA);
        }
        else if (pSrcMat->type() == CV_8UC3)
        {
            cv::cvtColor(*pSrcMat, stRGBA, cv::COLOR_BGR2RGBA);
        }
        else if (pSrcMat->type() == CV_8UC1)
        {
            cv::cvtColor(*pSrcMat, stRGBA, cv::COLOR_GRAY2RGBA);
        }
        else
        {
            return UniAVError::UnsupportedFormat;
        }

        if (stRGBA.empty())
        {
            return UniAVError::CacheBuildFailed;
        }

        if (stRGBA.cols > std::numeric_limits<std::int32_t>::max() ||
            stRGBA.rows > std::numeric_limits<std::int32_t>::max() ||
            stRGBA.step[0] > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            return UniAVError::UnsupportedFormat;
        }

        iWidth = static_cast<std::int32_t>(stRGBA.cols);
        iHeight = static_cast<std::int32_t>(stRGBA.rows);
        iDstStride = static_cast<std::int32_t>(stRGBA.step[0]);

        const std::size_t nNeededBytes = static_cast<std::size_t>(iDstStride) * static_cast<std::size_t>(iHeight);
        PoolBuffer stBuffer = m_pool->acquire(nNeededBytes, 32);
        if (stBuffer.data == nullptr || stBuffer.sizeBytes < nNeededBytes)
        {
            return UniAVError::PoolAllocationFailed;
        }

        const bool bSuccess = Convert::copyPackedRows(
            stBuffer.data,
            iDstStride,
            stRGBA.data,
            static_cast<std::int32_t>(stRGBA.step[0]),
            iWidth * 4,
            iHeight);
        if (!bSuccess)
        {
            m_pool->release(stBuffer);
            return UniAVError::CacheBuildFailed;
        }

        std::shared_ptr<std::uint8_t> spOwner(
            stBuffer.data,
            [spPool = m_pool, stBuffer](std::uint8_t*)
            {
                if (spPool)
                {
                    spPool->release(stBuffer);
                }
            });

        m_rgbaOwner = spOwner;
        m_rgbaCache = {spOwner.get(), iWidth, iHeight, iDstStride};
        m_videoDesc = {iWidth, iHeight, iDstStride, PixelFormat::RGBA8888};
        return UniAVError::Ok;
#else
        return UniAVError::UnsupportedFormat;
#endif
    }

    if (m_backend == InputBackend::BMDSDK)
    {
#if UNIAVFRAME_HAS_BMDSDK
        IDeckLinkVideoFrame* pSrcFrame = static_cast<IDeckLinkVideoFrame*>(m_native.nativePtr);
        if (pSrcFrame == nullptr)
        {
            return UniAVError::InvalidArgument;
        }

        iWidth = static_cast<std::int32_t>(pSrcFrame->GetWidth());
        iHeight = static_cast<std::int32_t>(pSrcFrame->GetHeight());
        const std::int32_t iSrcStride = static_cast<std::int32_t>(pSrcFrame->GetRowBytes());
        if (iWidth <= 0 || iHeight <= 0 || iSrcStride <= 0)
        {
            return UniAVError::InvalidArgument;
        }

        Convert::BMDFrameBytesAccess stAccess;
        if (!Convert::beginBMDFrameBytesAccess(pSrcFrame, &stAccess))
        {
            return UniAVError::CacheBuildFailed;
        }

        iDstStride = iWidth * 4;
        const std::size_t nNeededBytes = static_cast<std::size_t>(iDstStride) * static_cast<std::size_t>(iHeight);
        PoolBuffer stBuffer = m_pool->acquire(nNeededBytes, 32);
        if (stBuffer.data == nullptr || stBuffer.sizeBytes < nNeededBytes)
        {
            Convert::endBMDFrameBytesAccess(&stAccess);
            return UniAVError::PoolAllocationFailed;
        }

        bool bSuccess = false;
        const BMDPixelFormat emFmt = pSrcFrame->GetPixelFormat();
        if (emFmt == bmdFormat8BitBGRA)
        {
            bSuccess = Convert::convertBGRAtoRGBA(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight);
        }
        else if (emFmt == bmdFormat8BitARGB)
        {
            bSuccess = Convert::convertARGBtoRGBA(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight);
        }
        else if (emFmt == bmdFormat8BitYUV)
        {
#if UNIAVFRAME_BMDSDK_PREFER_FFMPEG && UNIAVFRAME_HAS_FFMPEG
            bSuccess = Convert::convertByFFmpegSws(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight,
                AV_PIX_FMT_UYVY422);

            if (!bSuccess)
#endif
            bSuccess = Convert::convertBMD2vuyToRGBA(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight);
        }
        else if (emFmt == bmdFormat10BitYUV)
        {
#if UNIAVFRAME_BMDSDK_PREFER_FFMPEG && UNIAVFRAME_HAS_FFMPEG
            // 优先路径: 部分较新版本的 FFmpeg 在 swscale 中直接支持 AV_PIX_FMT_V210,
            // 此时可跳过 codec decoder, 与 sws_scale YUV→RGBA 的标准用法一致.
#if defined(AV_PIX_FMT_V210)
            bSuccess = Convert::convertByFFmpegSws(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight,
                AV_PIX_FMT_V210);
#endif
            // 次级路径: 当前 FFmpeg 不支持 v210 作为 swscale 像素格式时,
            // 通过 AV_CODEC_ID_V210 解码器得到中间 AVFrame, 再 sws_scale → RGBA.
            if (!bSuccess)
            {
                bSuccess = Convert::convertBMDV210ToRGBAByCodec(
                    stBuffer.data,
                    iDstStride,
                    static_cast<const std::uint8_t*>(stAccess.pBytes),
                    iSrcStride,
                    iWidth,
                    iHeight);
            }
#endif
            // 最终兜底: 无 FFmpeg 或以上路径均失败时, 使用本地手动解包.
            if (!bSuccess)
            {
                bSuccess = Convert::convertBMDV210ToRGBA(
                    stBuffer.data,
                    iDstStride,
                    static_cast<const std::uint8_t*>(stAccess.pBytes),
                    iSrcStride,
                    iWidth,
                    iHeight);
            }
        }
        else if (emFmt == bmdFormat10BitYUVA)
        {
            if (!bSuccess)
            {
                bSuccess = Convert::convertBMDAy10ToRGBA(
                    stBuffer.data,
                    iDstStride,
                    static_cast<const std::uint8_t*>(stAccess.pBytes),
                    iSrcStride,
                    iWidth,
                    iHeight);
            }
        }
        else if (emFmt == bmdFormat10BitRGB)
        {
#if UNIAVFRAME_BMDSDK_PREFER_FFMPEG && UNIAVFRAME_HAS_FFMPEG
#if defined(AV_PIX_FMT_X2RGB10BE)
            bSuccess = convertByFFmpegSws(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight,
                AV_PIX_FMT_X2RGB10BE);
#endif
#endif

            if (!bSuccess)
            {
                bSuccess = convertBMDR210ToRGBA(
                    stBuffer.data,
                    iDstStride,
                    static_cast<const std::uint8_t*>(stAccess.pBytes),
                    iSrcStride,
                    iWidth,
                    iHeight);
            }
        }

        Convert::endBMDFrameBytesAccess(&stAccess);

        if (!bSuccess)
        {
            m_pool->release(stBuffer);
            return (emFmt == bmdFormat8BitBGRA ||
                    emFmt == bmdFormat8BitARGB ||
                    emFmt == bmdFormat8BitYUV ||
                    emFmt == bmdFormat10BitYUV ||
                    emFmt == bmdFormat10BitYUVA ||
                    emFmt == bmdFormat10BitRGB)
                ? UniAVError::CacheBuildFailed
                : UniAVError::UnsupportedFormat;
        }

        std::shared_ptr<std::uint8_t> spOwner(
            stBuffer.data,
            [spPool = m_pool, stBuffer](std::uint8_t*)
            {
                if (spPool)
                {
                    spPool->release(stBuffer);
                }
            });

        m_rgbaOwner = spOwner;
        m_rgbaCache = {spOwner.get(), iWidth, iHeight, iDstStride};
        m_videoDesc = {iWidth, iHeight, iDstStride, PixelFormat::RGBA8888};
        return UniAVError::Ok;
#else
        return UniAVError::UnsupportedFormat;
#endif
    }

    return UniAVError::UnsupportedFormat;
}

std::pair<std::shared_ptr<UniAVFrame>, UniAVError> UniAVFrameFactory::createFromFFmpeg(
    void* avFrame,
    const FrameTimestamp& timestamp,
    std::shared_ptr<IUniAVFramePool> pool,
    const UniAVFrameCreateOptions& options)
{
#if UNIAVFRAME_HAS_FFMPEG
    (void)options;

    AVFrame* pInputFrame = static_cast<AVFrame*>(avFrame);
    if (pInputFrame == nullptr)
    {
        return {nullptr, UniAVError::InvalidArgument};
    }

    std::shared_ptr<IUniAVFramePool> spPool = pool;
    if (!spPool)
    {
        spPool = std::make_shared<UniAVFramePool>();
    }

    AVFrame* pClonedFrame = av_frame_clone(pInputFrame);
    if (pClonedFrame == nullptr)
    {
        return {nullptr, UniAVError::CacheBuildFailed};
    }

    std::shared_ptr<UniAVFrame> spFrame(new UniAVFrame());
    spFrame->m_mediaType = MediaType::Video;
    spFrame->m_backend = InputBackend::FFmpeg;
    spFrame->m_timestamp = timestamp;
    spFrame->m_hasVideo = true;
    spFrame->m_pool = spPool;

    spFrame->m_native.backend = InputBackend::FFmpeg;
    spFrame->m_native.nativePtr = pClonedFrame;
    spFrame->m_nativeOwner = std::shared_ptr<void>(
        pClonedFrame,
        [](void* pObj)
        {
            AVFrame* pFrame = static_cast<AVFrame*>(pObj);
            av_frame_free(&pFrame);
        });

    spFrame->m_originalMemory.data = pClonedFrame->data[0];
    if (pClonedFrame->linesize[0] > 0 && pClonedFrame->height > 0)
    {
        spFrame->m_originalMemory.sizeBytes =
            static_cast<std::size_t>(pClonedFrame->linesize[0]) * static_cast<std::size_t>(pClonedFrame->height);
    }

    const UniAVError emCacheRet = spFrame->buildRGBA8888Cache();
    if (emCacheRet != UniAVError::Ok)
    {
        return {nullptr, emCacheRet};
    }

    return {spFrame, UniAVError::Ok};
#else
    (void)avFrame;
    (void)timestamp;
    (void)pool;
    (void)options;
    return {nullptr, UniAVError::UnsupportedFormat};
#endif
}

std::pair<std::shared_ptr<UniAVFrame>, UniAVError> UniAVFrameFactory::createFromQtImage(
    void* qImage,
    const FrameTimestamp& timestamp,
    std::shared_ptr<IUniAVFramePool> pool,
    const UniAVFrameCreateOptions& options)
{
#if UNIAVFRAME_HAS_QT
    (void)options;

    QImage* pInputImage = static_cast<QImage*>(qImage);
    if (pInputImage == nullptr || pInputImage->isNull())
    {
        return {nullptr, UniAVError::InvalidArgument};
    }

    std::shared_ptr<IUniAVFramePool> spPool = pool;
    if (!spPool)
    {
        spPool = std::make_shared<UniAVFramePool>();
    }

    QImage* pStoredImage = new QImage(*pInputImage);
    if (pStoredImage->isNull())
    {
        delete pStoredImage;
        return {nullptr, UniAVError::CacheBuildFailed};
    }

    std::shared_ptr<UniAVFrame> spFrame(new UniAVFrame());
    spFrame->m_mediaType = MediaType::Video;
    spFrame->m_backend = InputBackend::Qt;
    spFrame->m_timestamp = timestamp;
    spFrame->m_hasVideo = true;
    spFrame->m_pool = spPool;

    spFrame->m_native.backend = InputBackend::Qt;
    spFrame->m_native.nativePtr = pStoredImage;
    spFrame->m_nativeOwner = std::shared_ptr<void>(
        pStoredImage,
        [](void* pObj)
        {
            delete static_cast<QImage*>(pObj);
        });

    spFrame->m_originalMemory.data = pStoredImage->bits();
    if (pStoredImage->bytesPerLine() > 0 && pStoredImage->height() > 0)
    {
        spFrame->m_originalMemory.sizeBytes =
            static_cast<std::size_t>(pStoredImage->bytesPerLine()) * static_cast<std::size_t>(pStoredImage->height());
    }

    const UniAVError emCacheRet = spFrame->buildRGBA8888Cache();
    if (emCacheRet != UniAVError::Ok)
    {
        return {nullptr, emCacheRet};
    }

    return {spFrame, UniAVError::Ok};
#else
    (void)qImage;
    (void)timestamp;
    (void)pool;
    (void)options;
    return {nullptr, UniAVError::UnsupportedFormat};
#endif
}

std::pair<std::shared_ptr<UniAVFrame>, UniAVError> UniAVFrameFactory::createFromOpenCV(
    void* cvMat,
    const FrameTimestamp& timestamp,
    std::shared_ptr<IUniAVFramePool> pool,
    const UniAVFrameCreateOptions& options)
{
#if UNIAVFRAME_HAS_OPENCV
    (void)options;

    cv::Mat* pInputMat = static_cast<cv::Mat*>(cvMat);
    if (pInputMat == nullptr || pInputMat->empty())
    {
        return {nullptr, UniAVError::InvalidArgument};
    }

    std::shared_ptr<IUniAVFramePool> spPool = pool;
    if (!spPool)
    {
        spPool = std::make_shared<UniAVFramePool>();
    }

    cv::Mat* pStoredMat = new cv::Mat(pInputMat->clone());
    if (pStoredMat->empty())
    {
        delete pStoredMat;
        return {nullptr, UniAVError::CacheBuildFailed};
    }

    std::shared_ptr<UniAVFrame> spFrame(new UniAVFrame());
    spFrame->m_mediaType = MediaType::Video;
    spFrame->m_backend = InputBackend::OpenCV;
    spFrame->m_timestamp = timestamp;
    spFrame->m_hasVideo = true;
    spFrame->m_pool = spPool;

    spFrame->m_native.backend = InputBackend::OpenCV;
    spFrame->m_native.nativePtr = pStoredMat;
    spFrame->m_nativeOwner = std::shared_ptr<void>(
        pStoredMat,
        [](void* pObj)
        {
            delete static_cast<cv::Mat*>(pObj);
        });

    spFrame->m_originalMemory.data = pStoredMat->data;
    if (pStoredMat->step[0] > 0 && pStoredMat->rows > 0)
    {
        spFrame->m_originalMemory.sizeBytes =
            static_cast<std::size_t>(pStoredMat->step[0]) * static_cast<std::size_t>(pStoredMat->rows);
    }

    const UniAVError emCacheRet = spFrame->buildRGBA8888Cache();
    if (emCacheRet != UniAVError::Ok)
    {
        return {nullptr, emCacheRet};
    }

    return {spFrame, UniAVError::Ok};
#else
    (void)cvMat;
    (void)timestamp;
    (void)pool;
    (void)options;
    return {nullptr, UniAVError::UnsupportedFormat};
#endif
}

std::pair<std::shared_ptr<UniAVFrame>, UniAVError> UniAVFrameFactory::createFromBMDSDK(
    void* bmdVideoFrame,
    const FrameTimestamp& timestamp,
    std::shared_ptr<IUniAVFramePool> pool,
    const UniAVFrameCreateOptions& options)
{
#if UNIAVFRAME_HAS_BMDSDK
    (void)options;

    IDeckLinkVideoFrame* pInputFrame = static_cast<IDeckLinkVideoFrame*>(bmdVideoFrame);
    if (pInputFrame == nullptr)
    {
        return {nullptr, UniAVError::InvalidArgument};
    }

    pInputFrame->AddRef();

    std::shared_ptr<IUniAVFramePool> spPool = pool;
    if (!spPool)
    {
        spPool = std::make_shared<UniAVFramePool>();
    }

    std::shared_ptr<UniAVFrame> spFrame(new UniAVFrame());
    spFrame->m_mediaType = MediaType::Video;
    spFrame->m_backend = InputBackend::BMDSDK;
    spFrame->m_timestamp = timestamp;
    spFrame->m_hasVideo = true;
    spFrame->m_pool = spPool;

    spFrame->m_native.backend = InputBackend::BMDSDK;
    spFrame->m_native.nativePtr = pInputFrame;
    spFrame->m_nativeOwner = std::shared_ptr<void>(
        pInputFrame,
        [](void* pObj)
        {
            IDeckLinkVideoFrame* pFrame = static_cast<IDeckLinkVideoFrame*>(pObj);
            pFrame->Release();
        });

    spFrame->m_originalMemory.data = nullptr;

    const std::int32_t iRowBytes = static_cast<std::int32_t>(pInputFrame->GetRowBytes());
    const std::int32_t iHeight = static_cast<std::int32_t>(pInputFrame->GetHeight());
    if (iRowBytes > 0 && iHeight > 0)
    {
        spFrame->m_originalMemory.sizeBytes = static_cast<std::size_t>(iRowBytes) * static_cast<std::size_t>(iHeight);
    }

    const UniAVError emCacheRet = spFrame->buildRGBA8888Cache();
    if (emCacheRet != UniAVError::Ok)
    {
        return {nullptr, emCacheRet};
    }

    return {spFrame, UniAVError::Ok};
#else
    (void)bmdVideoFrame;
    (void)timestamp;
    (void)pool;
    (void)options;
    return {nullptr, UniAVError::UnsupportedFormat};
#endif
}

}
