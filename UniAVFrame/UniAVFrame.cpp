#include "UniAVFrame.h"

#include <cstring>
#include <limits>

#if defined(__has_include)
#if __has_include(<libavutil/frame.h>) && __has_include(<libavutil/pixfmt.h>) && __has_include(<libswscale/swscale.h>)
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

#if UNIAVFRAME_HAS_BMDSDK
struct BMDFrameBytesAccess
{
    IDeckLinkVideoBuffer* pVideoBuffer = nullptr;
    void* pBytes = nullptr;
    bool bAccessStarted = false;
};

static bool beginBMDFrameBytesAccess(IDeckLinkVideoFrame* pSrcFrame, BMDFrameBytesAccess* pOutAccess)
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

static void endBMDFrameBytesAccess(BMDFrameBytesAccess* pAccess)
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
            bSuccess = copyPackedRows(
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

        const bool bSuccess = copyPackedRows(
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

        const bool bSuccess = copyPackedRows(
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

        BMDFrameBytesAccess stAccess;
        if (!beginBMDFrameBytesAccess(pSrcFrame, &stAccess))
        {
            return UniAVError::CacheBuildFailed;
        }

        iDstStride = iWidth * 4;
        const std::size_t nNeededBytes = static_cast<std::size_t>(iDstStride) * static_cast<std::size_t>(iHeight);
        PoolBuffer stBuffer = m_pool->acquire(nNeededBytes, 32);
        if (stBuffer.data == nullptr || stBuffer.sizeBytes < nNeededBytes)
        {
            endBMDFrameBytesAccess(&stAccess);
            return UniAVError::PoolAllocationFailed;
        }

        bool bSuccess = false;
        const BMDPixelFormat emFmt = pSrcFrame->GetPixelFormat();
        if (emFmt == bmdFormat8BitBGRA)
        {
            bSuccess = convertBGRAtoRGBA(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight);
        }
        else if (emFmt == bmdFormat8BitARGB)
        {
            bSuccess = convertARGBtoRGBA(
                stBuffer.data,
                iDstStride,
                static_cast<const std::uint8_t*>(stAccess.pBytes),
                iSrcStride,
                iWidth,
                iHeight);
        }

        endBMDFrameBytesAccess(&stAccess);

        if (!bSuccess)
        {
            m_pool->release(stBuffer);
            return (emFmt == bmdFormat8BitBGRA || emFmt == bmdFormat8BitARGB)
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
