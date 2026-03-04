#include "UniAVFrame.h"

#include <cstring>

#if defined(__has_include)
#if __has_include(<libavutil/frame.h>) && __has_include(<libavutil/pixfmt.h>) && __has_include(<libswscale/swscale.h>)
#define UNIAVFRAME_HAS_FFMPEG 1
#if defined(__cplusplus)
extern "C" {
#endif
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#if defined(__cplusplus)
}
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
#else
#define UNIAVFRAME_HAS_FFMPEG 0
#define UNIAVFRAME_HAS_QT 0
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

std::atomic<std::int64_t> UniAVFrame::s_activeCount{0};

UniAVFrame::UniAVFrame()
{
    ++s_activeCount;
}

UniAVFrame::~UniAVFrame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool && m_rgbaBuffer.data)
    {
        m_pool->release(m_rgbaBuffer);
        m_rgbaBuffer = {};
        m_rgbaCache = {};
    }
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

        m_rgbaBuffer = stBuffer;
        m_rgbaCache = {stBuffer.data, iWidth, iHeight, iDstStride};
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

        m_rgbaBuffer = stBuffer;
        m_rgbaCache = {stBuffer.data, iWidth, iHeight, iDstStride};
        m_videoDesc = {iWidth, iHeight, iDstStride, PixelFormat::RGBA8888};
        return UniAVError::Ok;
#else
        return UniAVError::UnsupportedFormat;
#endif
    }

    return UniAVError::UnsupportedFormat;
}

static std::pair<std::shared_ptr<UniAVFrame>, UniAVError> makeNotImplementedFrame()
{
    return {nullptr, UniAVError::NotImplemented};
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
    (void)cvMat;
    (void)timestamp;
    (void)pool;
    (void)options;
    return makeNotImplementedFrame();
}

std::pair<std::shared_ptr<UniAVFrame>, UniAVError> UniAVFrameFactory::createFromBMDSDK(
    void* bmdVideoFrame,
    const FrameTimestamp& timestamp,
    std::shared_ptr<IUniAVFramePool> pool,
    const UniAVFrameCreateOptions& options)
{
    (void)bmdVideoFrame;
    (void)timestamp;
    (void)pool;
    (void)options;
    return makeNotImplementedFrame();
}

}
