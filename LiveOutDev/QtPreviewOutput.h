// A concrete ILiveOutput implementation that previews frames in a Qt window.
// Frames can be pushed from any thread; display is marshalled to the Qt main
// thread via QueuedConnection -- no Refresh() loop required from the caller.
#pragma once

#include "ILiveOutput.h"

#include <atomic>
#include <mutex>

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QWidget>

class FrameCanvas;   ///< 内部用画布控件，forward declaration

/**
 * @brief 脚手架。基于 Qt 窗口的实时帧预览输出（`ILiveOutput` 具体实现）。
 *
 * ## 线程模型
 * - `ReceiveFrame()` 可在**任意线程**调用：仅以互斥锁更新内部帧缓冲，
 *   通过 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 通知主线程刷新显示，
 *   符合 `IFrameRecv` 快速返回契约。
 * - `Open()`、`Close()` 及构造/析构**必须在 Qt 主线程调用**。
 * - 帧速率超过主线程处理能力时，以原子 pending 标志合并 invoke 请求，
 *   始终展示最新帧，中间帧自然丢弃，事件队列不会无限增长。
 *
 * ## 格式配置
 * `SetVideoFormat()` 会将预览窗口 `resize()` 至指定分辨率并保存帧率供查询。
 * 若未调用 `SetVideoFormat()`，窗口尺寸由第一帧的实际尺寸决定。
 *
 * ## 窗口生命周期
 * 若用户手动关闭预览窗口，状态自动回到 `Idle`，内部帧缓冲被清空；
 * 再次调用 `Open()` 可重新打开新窗口。
 *
 * @note 必须在 Qt 主线程构造与析构。
 */
class QtPreviewOutput : public QObject, public ILiveOutput
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数。
     * @param pParent Qt 父对象（可为 nullptr）。
     * @note 必须在 Qt 主线程调用。
     */
    explicit QtPreviewOutput(QObject* pParent = nullptr);

    /**
     * @brief 析构函数；自动调用 `Close()`。
     * @note 必须在 Qt 主线程析构。
     */
    ~QtPreviewOutput() override;

    // ----------------------------------------------------------------
    // ILiveOutput 接口实现
    // ----------------------------------------------------------------

    /**
     * @brief 创建并显示预览窗口。
     * @param sConfig 窗口标题（UTF-8 字符串）。
     * @return `AlreadyOpen` 若已处于打开状态；`Ok` 表示成功。
     * @note 必须在 Qt 主线程调用。
     */
    ELiveOutputError Open(const std::string& sConfig) override;

    /**
     * @brief 关闭并销毁预览窗口，清空帧缓冲，状态回 `Idle`。幂等。
     * @note 必须在 Qt 主线程调用。
     */
    void Close() override;

    bool             IsOpen()   const override;
    ELiveOutputState GetState() const override;

    /**
     * @brief 配置目标分辨率与帧率。
     * @details 若窗口已打开，立即 `resize()` 到指定尺寸。
     *          帧率字段仅保存，不驱动内部定时器。
     * @return `InvalidConfig` 若宽/高为负或 nFpsDen <= 0；`Ok` 其余情况。
     */
    ELiveOutputError SetVideoFormat(const TLiveOutputVideoFormat& stFormat) override;
    bool             GetVideoFormat(TLiveOutputVideoFormat& stFormat) const override;

    // ----------------------------------------------------------------
    // IFrameRecv 接口实现
    // ----------------------------------------------------------------

    /**
     * @brief 接收一帧；可在任意线程调用，立即返回。
     * @param spFrame 若为 `nullptr`、状态非 `Open`、或为非视频帧，静默忽略。
     */
    void ReceiveFrame(const TFrameRecvFramePtr& spFrame) override;

private slots:
    /// @brief Qt 主线程槽：从缓冲取最新帧并刷新 QLabel 显示。
    void onUpdateDisplay();

private:
    /// @brief 用户手动关闭窗口时的清理（连接到 `QWidget::destroyed`）。
    void onWindowDestroyed();

private:
    mutable std::mutex     m_mtxFrame;       ///< 保护 m_spLastFrame
    TFrameRecvFramePtr     m_spLastFrame;    ///< 最新帧（任意线程写，主线程读）
    std::atomic<bool>      m_bUpdatePending; ///< 防止 invokeMethod 堆积

    ELiveOutputState       m_eState;         ///< 当前输出端状态
    TLiveOutputVideoFormat m_stFormat;       ///< 已配置的视频格式
    bool                   m_bFormatSet;     ///< SetVideoFormat 是否已调用过

    QPointer<QWidget>      m_pWindow;        ///< 预览窗口（主线程管理）
    FrameCanvas*           m_pCanvas;        ///< 帧画布控件（由 m_pWindow 拥有）
};
