#pragma once

#include <cstdint>

namespace FrameGuiderStaticConfig
{
inline constexpr const char* kDefaultTrackerRuntimeDllPath = "./tracker/rsca_trk_runtime.dll";
inline constexpr const char* kDefaultTrackerModelPath480x288 = "./models/yolo26n-pose-ort-w480-h288.onnx";
inline constexpr std::int32_t kDefaultTrackerInputFpsLimit = 15;
inline constexpr std::int32_t kDefaultTrackerInputWidth = 480;
inline constexpr std::int32_t kDefaultTrackerInputHeight = 288;
}
