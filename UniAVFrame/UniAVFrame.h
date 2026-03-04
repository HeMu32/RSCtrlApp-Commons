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
	MemoryView originalMemory() const;
	RGBAImageView rgbaCacheView() const;

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
	PoolBuffer m_rgbaBuffer;

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
