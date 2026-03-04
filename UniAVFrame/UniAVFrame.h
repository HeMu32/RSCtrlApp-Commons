// Heavy header. 
// An impl. for managing Qt, OpenCV, ffmpeg, and BMD DeckLink SDK audio&video frames. 
// Manages original pixel fmt and offer single-step conversion, also a cache for data reuse for Qt and OpenCV, in compatible pixel fmt. 
// Manages a reference-based lifecycle management, thread-safe for frames.
// Manages pixel data and metadata, and timestamps. Timestamps recorded in a structure similar to BMD DeckLink SDK.


#ifndef UNIAVFRAME_H
#define UNIAVFRAME_H

#include "UniAVFramePool.h"
#include "UniAVFrameTypes.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <utility>

namespace UniAV
{

/**
 * @brief 统一音视频帧封装，承载 FFmpeg、Qt、OpenCV、BMD DeckLink SDK 的输入帧数据。
 *
 * ## 核心设计目标
 *
 * ### 1. RGBA8888 缓存防止重复转码
 * FFmpeg 和 BMD 采集卡采集的数据通常为非 RGB 格式（YUV、BGRA、ARGB、10/12-bit packed）。
 * Qt 和 OpenCV 侧对同一帧的多次访问若每次触发转码，开销不可接受。
 * UniAVFrame 在创建时立即（Eager）构建 RGBA8888 缓存，后续访问直接读取，转码仅发生一次。
 *
 * ### 2. 保留原始帧引用以支持后续高精度访问
 * `m_nativeOwner` 持有原始输入对象的所有权，生命周期与 UniAVFrame 绑定。
 * 这为后续高精度转换（如 BMD 10-bit / 12-bit 图像处理）预留了扩展空间，
 * 即使当前实现尚未提供对应接口。Qt/OpenCV 输入采用深拷贝保证内存安全；
 * FFmpeg/BMDSDK 输入通过 av_frame_clone / AddRef 挂载，析构时自动释放。
 *
 * ### 3. BMD 帧生命周期完全由 UniAVFrame 接管
 * `createFromBMDSDK()` 调用 `AddRef()` 后，调用方不得再独立 `Release()` 该帧。
 * 原始字节仅在 `buildRGBA8888Cache()` 内通过 StartAccess/EndAccess 短暂访问，符合 SDK 规约。
 * 后续扩展其他输入后端时，应遵循同样的生命周期接管原则。
 *
 * @note `originalMemory().data` 对 BMDSDK 后端返回 `nullptr`，因为 SDK 不保证
 *       EndAccess 后字节指针仍然有效。像素内容已完整保存于 RGBA 缓存中。
 *       若后续需要访问原始位深数据，应扩展独立的 nativePixelView() 接口配合池化缓冲区。
 */
class UniAVFrame final
{
public:
	~UniAVFrame();

	UniAVFrame(const UniAVFrame&) = delete;
	UniAVFrame& operator=(const UniAVFrame&) = delete;

	MediaType mediaType() const;
	InputBackend backend() const;
	FrameTimestamp timestamp() const;

	bool hasVideo() const;
	VideoDesc videoDesc() const;

	bool hasAudio() const;
	AudioDesc audioDesc() const;

	NativeFrameHandle nativeHandle() const;

	/**
	 * @brief 返回原始输入帧的内存视图（非拥有指针）。
	 * @note 对于 BMDSDK 后端，`data` 始终为 `nullptr`：BMD SDK 不承诺
	 *       EndAccess() 后字节指针有效，存储该指针不安全。
	 *       像素内容已通过 rgbaCacheView() / rgbaCacheRef() 提供。
	 *       对于 FFmpeg、Qt、OpenCV 后端，`data` 的有效期与 UniAVFrame 生命周期一致。
	 */
	MemoryView originalMemory() const;
	RGBAImageView rgbaCacheView() const;
	std::shared_ptr<const std::uint8_t> rgbaCacheRef() const;

	static std::int64_t activeFrameCount();

private:
	friend class UniAVFrameFactory;

	UniAVFrame();

	UniAVError buildRGBA8888Cache();

private:
	MediaType m_mediaType = MediaType::Unknown;
	InputBackend m_backend = InputBackend::Unknown;
	FrameTimestamp m_timestamp;

	bool m_hasVideo = false;
	VideoDesc m_videoDesc;

	bool m_hasAudio = false;
	AudioDesc m_audioDesc;

	NativeFrameHandle m_native;
	std::shared_ptr<void> m_nativeOwner;
	MemoryView m_originalMemory;

	RGBAImageView m_rgbaCache;
	std::shared_ptr<std::uint8_t> m_rgbaOwner;

	std::shared_ptr<IUniAVFramePool> m_pool;
	mutable std::mutex m_mutex;

	static std::atomic<std::int64_t> s_activeCount;
};

class UniAVFrameFactory
{
public:
	static std::pair<std::shared_ptr<UniAVFrame>, UniAVError> createFromFFmpeg(
		void* avFrame,
		const FrameTimestamp& timestamp,
		std::shared_ptr<IUniAVFramePool> pool,
		const UniAVFrameCreateOptions& options);

	static std::pair<std::shared_ptr<UniAVFrame>, UniAVError> createFromQtImage(
		void* qImage,
		const FrameTimestamp& timestamp,
		std::shared_ptr<IUniAVFramePool> pool,
		const UniAVFrameCreateOptions& options);

	static std::pair<std::shared_ptr<UniAVFrame>, UniAVError> createFromOpenCV(
		void* cvMat,
		const FrameTimestamp& timestamp,
		std::shared_ptr<IUniAVFramePool> pool,
		const UniAVFrameCreateOptions& options);

	static std::pair<std::shared_ptr<UniAVFrame>, UniAVError> createFromBMDSDK(
		void* bmdVideoFrame,
		const FrameTimestamp& timestamp,
		std::shared_ptr<IUniAVFramePool> pool,
		const UniAVFrameCreateOptions& options);
};

}

#endif // UNIAVFRAME_H
