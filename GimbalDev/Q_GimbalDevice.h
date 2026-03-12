// Deprecation.
/**
 * @file    Q_GimbalDevice.h
 * @brief   IGimbalDev 的 Qt Shim 层
 *
 * 职责：
 *  - 将来自 Qt UI 的信号（Slot 接入）转发给持有的 IGimbalDev 实例。
 *  - 通过 QTimer 周期性轮询设备状态，将结果以 Qt 信号形式发出。
 *  - 将 IGimbalDev 的 TGimbalDevCallbacks 回调桥接为 Qt 信号。
 *
 * 本类为 placeholder，不持有任何具体设备实现，仅持有 IGimbalDev* 指针。
 * 具体设备实例的构造与生命周期由外部管理。
 *
 * @note    本类依赖 Qt 5，不应被非 Qt 代码引用。
 * @note    所有 IGimbalDev callback 均通过 Qt::QueuedConnection 安全转发到 UI 线程。
 */

#ifndef Q_GIMBALDEVICE_H
#define Q_GIMBALDEVICE_H


#endif // Q_GIMBALDEVICE_H
