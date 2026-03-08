// QtPreviewOutput.cpp
#include "QtPreviewOutput.h"

#include <QMetaObject>
#include <QImage>
#include <QPainter>
#include <QVBoxLayout>

#include "../UniAVFrame/UniAVFrame.h"

// -----------------------------------------------------------------------
// FrameCanvas：直接以 QPainter::drawImage 绘制，无 QPixmap 中间拷贝。
// 保持对 UniAVFrame 的 shared_ptr 引用，使 RGBA 缓存在 paintEvent 期间保活。
// -----------------------------------------------------------------------
class FrameCanvas : public QWidget
{
public:
    explicit FrameCanvas(QWidget* pParent = nullptr)
        : QWidget(pParent)
    {
        setMinimumSize(1, 1);
    }

    /**
     * @brief 更新待绘制帧。
     * @param spFrame 用于保活 RGBA 缓存；与 imgView 同步替换，主线程调用。
     * @param imgView 指向 spFrame RGBA 缓存的非拥有 QImage 视图。
     */
    void setFrame(const TFrameRecvFramePtr& spFrame, const QImage& imgView)
    {
        // 两者在同一主线程内顺序赋值，paintEvent 不会在中间插入，无竞态。
        m_spPaintFrame = spFrame;
        m_imgView      = imgView;
    }

    void clearFrame()
    {
        m_spPaintFrame.reset();
        m_imgView = QImage{};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_imgView.isNull())
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        // 保持长宽比，居中绘制
        const QSize szWidget = size();
        const QSize szScaled = m_imgView.size().scaled(szWidget, Qt::KeepAspectRatio);
        const int   nX       = (szWidget.width()  - szScaled.width())  / 2;
        const int   nY       = (szWidget.height() - szScaled.height()) / 2;

        p.drawImage(QRect(nX, nY, szScaled.width(), szScaled.height()), m_imgView);
    }

private:
    TFrameRecvFramePtr m_spPaintFrame;  ///< 保活 UniAVFrame RGBA 缓存
    QImage             m_imgView;       ///< 非拥有视图，生命周期由 m_spPaintFrame 保证
};

// -----------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------

QtPreviewOutput::QtPreviewOutput(QObject* pParent)
    : QObject(pParent)
    , m_bUpdatePending(false)
    , m_eState(ELiveOutputState::Idle)
    , m_bFormatSet(false)
    , m_pCanvas(nullptr)
{
}

QtPreviewOutput::~QtPreviewOutput()
{
    Close();
}

// -----------------------------------------------------------------------
// ILiveOutput
// -----------------------------------------------------------------------

ELiveOutputError QtPreviewOutput::Open(const std::string& sConfig)
{
    if (m_eState == ELiveOutputState::Open)
        return ELiveOutputError::AlreadyOpen;

    // ---- 创建窗口与画布控件 ---------------------------------------------
    m_pWindow = new QWidget(nullptr);
    m_pWindow->setWindowTitle(QString::fromStdString(sConfig));

    m_pCanvas = new FrameCanvas(m_pWindow);

    QVBoxLayout* pLayout = new QVBoxLayout(m_pWindow);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(m_pCanvas);
    m_pWindow->setLayout(pLayout);

    // ---- 若格式已配置，预先设置窗口尺寸 ---------------------------------
    if (m_bFormatSet && m_stFormat.nWidth > 0 && m_stFormat.nHeight > 0)
        m_pWindow->resize(m_stFormat.nWidth, m_stFormat.nHeight);

    // ---- 用户手动关闭窗口时回调 -----------------------------------------
    connect(m_pWindow.data(), &QObject::destroyed,
            this, [this]() { onWindowDestroyed(); });

    m_pWindow->show();
    m_eState = ELiveOutputState::Open;
    return ELiveOutputError::Ok;
}

void QtPreviewOutput::Close()
{
    if (m_eState == ELiveOutputState::Idle)
        return;

    if (m_pWindow)
    {
        // 先断开信号，避免 destroyed 信号触发二次清理
        disconnect(m_pWindow.data(), &QObject::destroyed, this, nullptr);
        m_pWindow->close();
        delete m_pWindow;
        m_pWindow = nullptr;
    }

    m_pCanvas = nullptr;
    m_bUpdatePending.store(false);

    {
        std::lock_guard<std::mutex> lk(m_mtxFrame);
        m_spLastFrame.reset();
    }

    m_eState = ELiveOutputState::Idle;
}

bool QtPreviewOutput::IsOpen() const
{
    return m_eState == ELiveOutputState::Open;
}

ELiveOutputState QtPreviewOutput::GetState() const
{
    return m_eState;
}

ELiveOutputError QtPreviewOutput::SetVideoFormat(const TLiveOutputVideoFormat& stFormat)
{
    if (stFormat.nWidth < 0 || stFormat.nHeight < 0 || stFormat.nFpsDen <= 0)
        return ELiveOutputError::InvalidConfig;

    m_stFormat   = stFormat;
    m_bFormatSet = true;

    // 若窗口已打开且分辨率有效，立即 resize
    if (m_pWindow && m_stFormat.nWidth > 0 && m_stFormat.nHeight > 0)
        m_pWindow->resize(m_stFormat.nWidth, m_stFormat.nHeight);

    return ELiveOutputError::Ok;
}

bool QtPreviewOutput::GetVideoFormat(TLiveOutputVideoFormat& stFormat) const
{
    if (!m_bFormatSet)
        return false;

    stFormat = m_stFormat;
    return true;
}

// -----------------------------------------------------------------------
// IFrameRecv
// -----------------------------------------------------------------------

void QtPreviewOutput::ReceiveFrame(const TFrameRecvFramePtr& spFrame)
{
    // 状态防御与 nullptr 防御
    if (!spFrame || m_eState != ELiveOutputState::Open)
        return;

    // 仅处理视频帧
    if (!spFrame->hasVideo())
        return;

    {
        std::lock_guard<std::mutex> lk(m_mtxFrame);
        m_spLastFrame = spFrame;
    }

    // 以原子 pending 标志合并 invokeMethod 请求，避免事件队列堆积
    bool bExpected = false;
    if (m_bUpdatePending.compare_exchange_strong(bExpected, true))
    {
        QMetaObject::invokeMethod(this, "onUpdateDisplay", Qt::QueuedConnection);
    }
}

// -----------------------------------------------------------------------
// Private slots
// -----------------------------------------------------------------------

void QtPreviewOutput::onUpdateDisplay()
{
    m_bUpdatePending.store(false);

    if (!m_pCanvas || !m_pWindow)
        return;

    // 在锁内复制 shared_ptr，锁外执行渲染，避免持锁调用 Qt
    TFrameRecvFramePtr spFrame;
    {
        std::lock_guard<std::mutex> lk(m_mtxFrame);
        spFrame = m_spLastFrame;
    }

    if (!spFrame || !spFrame->hasVideo())
        return;

    UniAV::RGBAImageView stView = spFrame->rgbaCacheView();
    if (!stView.data || stView.width <= 0 || stView.height <= 0)
        return;

    // ---- 首帧自动 resize（SetVideoFormat 未指定尺寸时）-----------------
    if (!m_bFormatSet || m_stFormat.nWidth <= 0 || m_stFormat.nHeight <= 0)
        m_pWindow->resize(stView.width, stView.height);

    // ---- 构建 QImage 非拥有视图，交给 FrameCanvas 保活并触发重绘 -------
    // 无 QPixmap 中间拷贝；像素数据由 spFrame 持有，FrameCanvas 内部保存
    // m_spPaintFrame 确保 RGBA 缓存在 paintEvent 期间始终有效。
    QImage stImgView(
        stView.data,
        stView.width,
        stView.height,
        stView.strideBytes,
        QImage::Format_RGBA8888);

    m_pCanvas->setFrame(spFrame, stImgView);
    m_pCanvas->update();    // 调度 paintEvent，在当前事件循环迭代末尾执行
}

// -----------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------

void QtPreviewOutput::onWindowDestroyed()
{
    m_pWindow = nullptr;
    if (m_pCanvas)
    {
        m_pCanvas->clearFrame();
        m_pCanvas = nullptr;
    }
    m_bUpdatePending.store(false);

    {
        std::lock_guard<std::mutex> lk(m_mtxFrame);
        m_spLastFrame.reset();
    }

    m_eState = ELiveOutputState::Idle;
}
