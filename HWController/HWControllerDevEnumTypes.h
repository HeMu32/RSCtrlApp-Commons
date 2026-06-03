/**
 * @file    HWControllerDevEnumTypes.h
 * @brief   PTZ 硬件控制器枚举与打开参数类型定义（Lite header）
 */

#ifndef HWCONTROLLER_DEV_ENUM_TYPES_H
#define HWCONTROLLER_DEV_ENUM_TYPES_H

#include <string>

#include "DevEnum/IDevEnum.h"
#include "HWController/IHWController.h"

/**
 * @brief 控制器枚举设备信息
 */
struct THWControllerDeviceInfo : public TDevEnumDeviceInfo
{
};

/**
 * @brief 控制器打开参数
 */
struct THWControllerOpenParams
{
    HWController::THWControllerCallbacks stCallbacks;
    std::string sBackendConfig;
};

#endif // HWCONTROLLER_DEV_ENUM_TYPES_H

// 这个设计完全来自LLM对一设备一类型枚举的不理解产生的幻觉. 
// 实际上后续应该改为每种类型的控制器自己实现一个枚举, 而不是尝试使用枚举+
// 特定类型标识结构完成识别. 