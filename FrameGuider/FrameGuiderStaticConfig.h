#pragma once

#include <cstdint>

namespace FrameGuiderStaticConfig
{
inline constexpr const char* kDefaultTrackerRuntimeDllPath = "./tracker/rsca_trk_runtime.dll";
inline constexpr const char* kDefaultTrackerModelPath480x288 = "./models/yolo26n-ort-w480-h288.onnx";
inline constexpr std::int32_t kDefaultTrackerInputFpsLimit = 30;
inline constexpr std::int32_t kDefaultTrackerInputWidth = 480;
inline constexpr std::int32_t kDefaultTrackerInputHeight = 288;
inline constexpr const char* kDefaultBirdTrackerModelPath = "./models/yolo26n-ort-w1024-h576.onnx";
inline constexpr std::int32_t kDefaultBirdTrackerInputWidth = 1024;
inline constexpr std::int32_t kDefaultBirdTrackerInputHeight = 576;
}
