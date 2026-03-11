/**
 * @file    IGimbalDev.h
 * @brief   三轴云台设备抽象接口（ADR-006）
 *          轻依赖型头文件. Lite header.
 *
 * 设计原则：
 *  - 接口头文件本身不依赖 Qt 或任何具体 SDK。
 *  - 所有异步数据（位置、状态、错误）通过 TGimbalDevCallbacks 回调推送。
 *  - 指令方法均为 fire-and-forget（void 返回），不阻塞调用线程。
 *  - FocalLengthHandler 不纳入本接口，由具体实现类按需持有（见 ADR-006）。
 */

#ifndef IGIMBALDEV_H
#define IGIMBALDEV_H

#include <cstdint>
#include <functional>
#include <string>

namespace GimbalDev
{

// ============================================================
//  枚举：设备状态
// ============================================================

/**
 * @brief 云台设备连接状态枚举
 */
enum class EGimbalDevState
{
    Idle        = 0,    ///< 未连接 / 初始状态
    Connecting  = 1,    ///< 正在建立连接
    Connected   = 2,    ///< 已连接，可以收发指令
    Disconnecting = 3,  ///< 正在断开连接
    Error       = 4,    ///< 错误状态，需要重新 Open
};

// ============================================================
//  枚举：错误码
// ============================================================

/**
 * @brief 云台设备操作错误码
 */
enum class EGimbalDevError
{
    Ok              = 0,    ///< 操作成功
    NotOpen         = 1,    ///< 设备未连接，无法执行操作
    AlreadyOpen     = 2,    ///< 设备已连接，不可重复 Open
    DeviceNotFound  = 3,    ///< 指定索引的设备不存在
    BackendFailure  = 4,    ///< 底层 SDK / 驱动返回错误
    InvalidParam    = 5,    ///< 参数非法（如超出范围）
};

// ============================================================
//  回调结构体
// ============================================================

/**
 * @brief   云台设备异步回调集合
 *
 * 所有字段均为 std::function，未设置时保持默认（空）。
 * 实现层在调用前应检查回调是否有效（operator bool()）。
 *
 * @note    回调可能在非 UI 线程触发，调用方负责线程安全（如通过 Qt::QueuedConnection 转发）。
 */
struct TGimbalDevCallbacks
{
    /**
     * @brief   云台位置更新回调
     * @param   nYaw    偏航角，单位：0.1°
     * @param   nRoll   横滚角，单位：0.1°
     * @param   nPitch  俯仰角，单位：0.1°
     */
    std::function<void(int16_t nYaw, int16_t nRoll, int16_t nPitch)> fnPositionUpdate;

    /**
     * @brief   设备状态变化回调
     * @param   eState  新状态
     */
    std::function<void(EGimbalDevState eState)> fnStateChanged;

    /**
     * @brief   错误通知回调
     * @param   eError  错误码
     * @param   sMsg    可选的错误说明字符串
     */
    std::function<void(EGimbalDevError eError, const std::string& sMsg)> fnError;

    /**
     * @brief   对焦电机自动标定通知回调（可选）
     *          由 CmdFocusMotorAutoCal() 触发。对于不提供完成状态回报的设备，
     *          实现层可按 best-effort 策略在"命令成功发送"时触发该回调。
     */
    std::function<void()> fnFocusMotorCalComplete;
};

// ============================================================
//  接口：IGimbalDev
// ============================================================

/**
 * @brief   三轴云台设备抽象接口
 *
 * 使用流程：
 *  1. 构造具体实现（如 DJIRoninDev）并设置回调：Open(devIdx, canIdx, callbacks)
 *  2. 发送运动指令：CmdMoveTo / CmdJoystickMove
 *  3. 通过 fnPositionUpdate 回调接收位置更新
 *  4. 完成后调用 Close() 断开连接
 */
class IGimbalDev
{
public:
    virtual ~IGimbalDev() = default;

    // --------------------------------------------------------
    //  生命周期
    // --------------------------------------------------------

    /**
     * @brief       建立连接并注册回调（异步）
     * @param       nDevIndex   CAN 盒设备索引（从 0 起）
     * @param       nCanIndex   CAN 通道索引（从 0 起）
     * @param       callbacks   回调结构体，按需填充
     * @return      EGimbalDevError::Ok 表示连接流程已启动（异步）；
     *              真正连通以 fnStateChanged(Connected) 为准。
     *              EGimbalDevError::AlreadyOpen 若当前已处于
     *              Connected / Connecting / Error 状态。
     *              Error 状态须先调用 Close() 再 Open()。
     */
    virtual EGimbalDevError Open(
        int                         nDevIndex,
        int                         nCanIndex,
        const TGimbalDevCallbacks&  callbacks) = 0;

    /**
     * @brief   断开连接，释放资源。幂等：重复调用无副作用。
     */
    virtual void Close() = 0;

    /**
     * @brief   返回设备是否处于已连接状态（EGimbalDevState::Connected）
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief   返回完整的设备状态枚举
     */
    virtual EGimbalDevState GetState() const = 0;

    /**
     * @brief   返回用于向用户展示的设备友好名称字符串
     *
     * 典型返回值示例：
     *  - "DJI RS 3 Pro (CAN #0) [Serial]"
     *
     * @note    实现层应尽可能返回可读的型号与通道信息。
     *          在设备尚未连接时，允许返回基于配置信息的静态名称（如 "DJI Ronin [未连接]"）。
     */
    virtual std::string GetDeviceName() const = 0;

    // --------------------------------------------------------
    //  运动指令（fire-and-forget）
    // --------------------------------------------------------

    /**
     * @brief           绝对角度移动
     * @param   nYaw    目标偏航角，单位：0.1°
     * @param   nRoll   目标横滚角，单位：0.1°
     * @param   nPitch  目标俯仰角，单位：0.1°
     * @param   nTimeMs 运动时间，单位：毫秒
     *
     * @note    若已设置角度限位，实现层在发送指令前自动裁剪到限位范围内。
     * @note    实现层还应在用户限位裁剪之后强制执行设备协议硬范围裁剪，
     *          以防止超出硬件支持极限（如 DJI RS 系列：
     *          Yaw [-1800,+1800]，Roll [-300,+300]，Pitch [-560,+1460]，单位 0.1°）。
     * @note    nTimeMs 应裁剪至协议可表达范围（如 DJI RS 系列 [100, 25500] ms）。
     * @note    设备未连接时实现层应静默忽略或通过 fnError 通知，不抛出异常。
     */
    virtual void CmdMoveTo(
        int16_t     nYaw,
        int16_t     nRoll,
        int16_t     nPitch,
        uint32_t    nTimeMs) = 0;

    /**
     * @brief           摇杆相对增量移动
     * @param   nX      水平方向增量（映射至偏航轴）
     * @param   nY      垂直方向增量（映射至俯仰轴）
     *
     * @note    若已设置限位且当前位置已到达边界，超出方向的分量由实现层清零。
     * @note    实现层可对调用频率进行节流（如 DJI RS 系列建议最小间隔 50ms），
     *          以防止 CAN 总线过载或设备异常；超出节流的调用应被静默丢弃。
     */
    virtual void CmdJoystickMove(int16_t nX, int16_t nY) = 0;

    /**
     * @brief   主动触发一次位置查询
     *          结果通过 TGimbalDevCallbacks::fnPositionUpdate 回调返回，不阻塞。
     *          实现层可在内部做频率节流。
     */
    virtual void CmdQueryPosition() = 0;

    /**
     * @brief           启动或停止内部自动位置轮询
     * @param   bEnable true=启动轮询，false=停止轮询
     *
     * @note    本方法为 **可选实现**，不支持自动轮询的实现可不覆盖，默认为空操作。
     * @note    自动轮询启用时，实现层以固定内部周期主动查询设备位置并触发
     *          TGimbalDevCallbacks::fnPositionUpdate 回调；
     *          停用后仅响应 CmdQueryPosition() 的显式调用。
     */
    virtual void SetAutoPollPosition(bool /*bEnable*/) {}

    // --------------------------------------------------------
    //  对焦电机指令（fire-and-forget）
    // --------------------------------------------------------

    /**
     * @brief   触发**跟焦电机**自动限位标定流程（协议 CmdSet=0x0E CmdID=0x12）
     *
     * @note    本方法触发的是"跟焦电机自动标定（focus motor auto calibration）"，
     *          而非"云台自动校准（gimbal auto calibration, CmdID=0x0F）"，两者不同。
     * @note    回调 TGimbalDevCallbacks::fnFocusMotorCalComplete 的触发语义由实现层定义，
     *          对于无完成状态回报的设备可采用 best-effort（命令发送成功即触发）。
     */
    virtual void CmdFocusMotorAutoCal() = 0;

    /**
     * @brief           直接设置对焦电机位置
     * @param   uiPos   目标位置，范围 0 ~ 4095
     */
    virtual void CmdSetFocusMotorPos(uint16_t uiPos) = 0;

    /**
     * @brief           控制对焦电机使用时域增量/相对速度移动
     * @param   nSpeed  对焦改变速度或每周期的增量步长。正负代表方向。
     * 
     * @note            虽然某些底层设备（如 DJI）仅接受绝对位置命令，
     *                  实现层须在内部维护当前位置并累加速度进行绝对位置的推算和下发。
     */
    virtual void CmdFocusMotorMoveRel(int16_t nSpeed) = 0;

    /**
     * @brief           添加或更新焦距-电机位置标定点
     * @param   uiFocalMm   焦距（毫米）
     * @param   uiMotorPos  跟焦电机位置（0~4095）
     */
    virtual void AddFocalCalPoint(uint16_t uiFocalMm, uint16_t uiMotorPos) = 0;

    /**
     * @brief           根据电机位置估算焦距（毫米）
     * @param   uiMotorPos  跟焦电机位置（0~4095）
     * @return          估算焦距；失败返回负值
     *
     * @note            焦距数据源存在潜在冲突：
     *                  1) 本方法基于“跟焦电机位置-焦距标定数据”进行插值估算；
     *                  2) 当系统同时接入相机控制链路时，相机状态上报可能给出另一份焦距值。
     *                  具体采用哪一路数据由上层聚合对象决策，本接口不做仲裁。
     */
    virtual int GetFocalLenFromMotorPos(uint16_t uiMotorPos) const = 0;

    /**
     * @brief   清除全部焦距标定数据
     */
    virtual void ClearFocalCalData() = 0;

    /**
     * @brief   将焦距标定数据持久化到文件
     */
    virtual void SaveFocalCalToFile() = 0;

    /**
     * @brief   从文件加载焦距标定数据
     */
    virtual void LoadFocalCalFromFile() = 0;

    // --------------------------------------------------------
    //  角度限位配置
    // --------------------------------------------------------

    /**
     * @brief   设置各轴角度限位
     *
     * 单位均为 0.1°（与 DJI Ronin SDK 原生单位一致）。  
     * A/B 同为 0 表示该轴无限位。  
     * 实现层应在内部保证 A <= B（若不满足则自动交换）。
     *
     * @param   nYawA   偏航轴下界
     * @param   nYawB   偏航轴上界
     * @param   nRolA   横滚轴下界
     * @param   nRolB   横滚轴上界
     * @param   nPitA   俯仰轴下界
     * @param   nPitB   俯仰轴上界
     */
    virtual void SetAxisLimits(
        int16_t nYawA, int16_t nYawB,
        int16_t nRolA, int16_t nRolB,
        int16_t nPitA, int16_t nPitB) = 0;

    /**
     * @brief   读取当前各轴角度限位设置
     *          参数语义与 SetAxisLimits 相同，通过引用输出。
     */
    virtual void GetAxisLimits(
        int16_t& nYawA, int16_t& nYawB,
        int16_t& nRolA, int16_t& nRolB,
        int16_t& nPitA, int16_t& nPitB) const = 0;

    /**
     * @brief   清除全部轴限位（所有值重置为 0，即无限位）
     */
    virtual void ClearAxisLimits() = 0;
};

} // namespace GimbalDev

#endif // IGIMBALDEV_H
