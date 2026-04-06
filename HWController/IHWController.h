/**
 * @file    IHWController.h
 * @brief   PTZ 硬件控制器抽象接口（Lite header）
 *
 * 设计原则：
 *  - 仅依赖 C++ 标准库，不依赖 Qt 或具体后端 SDK。
 *  - 输入通过回调异步上送（摇杆、Zoom 方向、按键边沿）。
 *  - 命令调用方（上层）负责线程切换与业务聚合。
 */

#ifndef IHWCONTROLLER_H
#define IHWCONTROLLER_H

#include <cstdint>
#include <functional>
#include <string>

namespace HWController
{

/**
 * @brief 控制器连接状态
 */
enum class EHWControllerState
{
    Idle = 0,
    Connecting = 1,
    Connected = 2,
    Disconnecting = 3,
    Error = 4,
};

/**
 * @brief 控制器错误码
 */
enum class EHWControllerError
{
    Ok = 0,
    NotOpen = 1,
    AlreadyOpen = 2,
    DeviceNotFound = 3,
    BackendFailure = 4,
    InvalidParam = 5,
};

/**
 * @brief 控制器输入回调集合
 *
 * 数值合同：
 *  - fnJoystickXY: 统一逻辑范围 `[-15000, 15000]`
 *  - fnZoomSign: 仅方向语义 `-1 / 0 / +1`
 *  - fnButtonEvent: buttonId 从 0 开始
 *
 * @note 回调可能在非 UI 线程触发，调用方需自行完成线程切换。
 */
struct THWControllerCallbacks
{
    std::function<void(int16_t nX, int16_t nY)> fnJoystickXY;
    std::function<void(int8_t nSign)> fnZoomSign;
    std::function<void(std::uint8_t nButtonId0, bool bPressed)> fnButtonEvent;

    std::function<void(EHWControllerState eState)> fnStateChanged;
    std::function<void(EHWControllerError eError, const std::string& sMsg)> fnError;
};

/**
 * @brief PTZ 硬件控制器抽象接口
 */
class IHWController
{
public:
    virtual ~IHWController() = default;

    /**
     * @brief 打开控制器会话并注册回调
     * @param callbacks 回调集合
     * @return Ok 表示连接流程已启动或已建立；其他值表示失败。
     */
    virtual EHWControllerError Open(const THWControllerCallbacks& callbacks) = 0;

    /**
     * @brief 运行期替换回调集合
     */
    virtual void SetCallbacks(const THWControllerCallbacks& callbacks) = 0;

    /**
     * @brief 关闭控制器会话（幂等）
     */
    virtual void Close() = 0;

    /**
     * @brief 是否处于可用连接状态
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief 获取控制器状态
     */
    virtual EHWControllerState GetState() const = 0;

    /**
     * @brief 获取设备友好名
     */
    virtual std::string GetDeviceName() const = 0;
};

} // namespace HWController

#endif // IHWCONTROLLER_H
