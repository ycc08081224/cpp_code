#include "ImagePanel.h"
#include "MediaPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>

static const int kMaxDecodeSize = 3840;

ImagePanel::ImagePanel(const QString &path, QWidget *parent)
    : MediaPanel(Image, path, parent)
{
    m_scene = new QGraphicsScene(this);
    m_item = new QGraphicsPixmapItem();
    m_scene->addItem(m_item);

    m_view = new QGraphicsView(m_scene, m_mediaArea);
    m_view->setStyleSheet("background: #111118; border: none;");
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, true);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setCursor(Qt::OpenHandCursor);

    QVBoxLayout *lay = new QVBoxLayout(m_mediaArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_view);

    // 图片没有播放/进度/音量概念，整条底部控制栏都不要，换成缩放条
    removeControlBar();
    buildZoomBar();

    // GIF 走 QMovie，静态图一次性解码
    if (QFileInfo(path).suffix().toLower() == "gif") {
        m_movie = new QMovie(path, QByteArray(), this);
        if (m_movie->isValid()) {
            m_movie->setCacheMode(QMovie::CacheAll);
            connect(m_movie, &QMovie::frameChanged, this, [this](int) {
                const QPixmap frame = m_movie->currentPixmap();
                const bool sizeChanged = frame.size() != m_lastFrameSize;
                m_item->setPixmap(frame);
                if (sizeChanged) {
                    m_lastFrameSize = frame.size();
                    if (m_fitMode) fitToWindow();
                }
            });
            m_movie->start();
            m_infoLabel->setText(QString("%1x%2 GIF")
                .arg(m_movie->currentPixmap().width())
                .arg(m_movie->currentPixmap().height()));
        }
    }

    if (m_item->pixmap().isNull() && !m_movie) {
        QPixmap pm(path);
        if (pm.isNull()) {
            m_empty = true;
            showMessage(QString::fromUtf8("❌ 无法加载图片"));
            m_infoLabel->setText("load failed");
        } else {
            // 超大图先降采样，避免每次缩放都在 8K 原图上做插值
            if (pm.width() > kMaxDecodeSize || pm.height() > kMaxDecodeSize)
                pm = pm.scaled(kMaxDecodeSize, kMaxDecodeSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_item->setPixmap(pm);
            m_infoLabel->setText(QString("%1x%2").arg(pm.width()).arg(pm.height()));
        }
    }

    if (!m_empty) {
        m_lastFrameSize = m_item->pixmap().size();
        fitToWindow();
    }
}

ImagePanel::~ImagePanel()
{
    if (m_movie) {
        m_movie->stop();
        disconnect(m_movie, nullptr, this, nullptr);
    }
    if (m_view) m_view->viewport()->removeEventFilter(this);
}

void ImagePanel::buildZoomBar()
{
    m_zoomBar = new QWidget(this);
    m_zoomBar->setFixedHeight(34);
    m_zoomBar->setStyleSheet(
        "QWidget#zoomBar { background: rgba(26,26,26,140); }"
        "QPushButton { background: transparent; color: #e8e8f0; border: 1px solid #3a3a55;"
        "              border-radius: 5px; font-size: 12px; padding: 1px 9px; }"
        "QPushButton:hover { background: rgba(255,255,255,35); }"
        "QPushButton:pressed { background: rgba(233,69,96,120); }"
        "QLabel#zoomPct { color: #c8c8d8; font-family: Consolas; font-size: 12px; }"
    );
    m_zoomBar->setObjectName("zoomBar");

    QHBoxLayout *lay = new QHBoxLayout(m_zoomBar);
    lay->setContentsMargins(8, 3, 8, 3);
    lay->setSpacing(6);

    auto *zoomOut = new QPushButton(QString::fromUtf8("− 缩小"), m_zoomBar);
    zoomOut->setToolTip(QString::fromUtf8("缩小（也可以用鼠标滚轮）"));
    connect(zoomOut, &QPushButton::clicked, this, [this]() { zoomByButton(1.0 / 1.25); });

    m_zoomLabel = new QLabel("100%", m_zoomBar);
    m_zoomLabel->setObjectName("zoomPct");
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setMinimumWidth(52);

    auto *zoomIn = new QPushButton(QString::fromUtf8("+ 放大"), m_zoomBar);
    zoomIn->setToolTip(QString::fromUtf8("放大（也可以用鼠标滚轮）"));
    connect(zoomIn, &QPushButton::clicked, this, [this]() { zoomByButton(1.25); });

    auto *fitBtn = new QPushButton(QString::fromUtf8("适应窗口"), m_zoomBar);
    fitBtn->setToolTip(QString::fromUtf8("缩放到正好铺满面板（双击图片同效）"));
    connect(fitBtn, &QPushButton::clicked, this, [this]() { fitToWindow(); });

    auto *oneToOne = new QPushButton(QString::fromUtf8("原始大小"), m_zoomBar);
    oneToOne->setToolTip(QString::fromUtf8("按图片像素 1:1 显示"));
    connect(oneToOne, &QPushButton::clicked, this, [this]() { actualSize(); });

    lay->addWidget(zoomOut);
    lay->addWidget(m_zoomLabel);
    lay->addWidget(zoomIn);
    lay->addSpacing(4);
    lay->addWidget(fitBtn);
    lay->addWidget(oneToOne);
    lay->addStretch(1);

    addCustomBottomBar(m_zoomBar);
}

void ImagePanel::showMessage(const QString &text)
{
    auto *label = new QLabel(text, m_mediaArea);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("background: #111118; color: #c8c8d8; font-size: 13px;");
    m_mediaArea->layout()->addWidget(label);
    if (m_view) m_view->hide();
}

void ImagePanel::fitToWindow()
{
    if (!m_view || !m_item || m_empty) return;
    if (m_item->pixmap().isNull()) return;
    m_view->resetTransform();
    m_view->fitInView(m_item, Qt::KeepAspectRatio);

    // 小图标（几十像素）铺满整个面板会被插值成一坨马赛克，放大到 400% 封顶
    const qreal kFitMaxZoom = 4.0;
    if (m_view->transform().m11() > kFitMaxZoom) {
        m_view->resetTransform();
        m_view->scale(kFitMaxZoom, kFitMaxZoom);
        m_view->centerOn(m_item);
    }

    m_fitMode = true;
    // m_zoom 统一用「相对 1:1 像素的绝对缩放比」，从 transform 里读回来，
    // 这样百分比标签和按钮缩放才是同一个口径
    m_zoom = m_view->transform().m11();
    updateZoomLabel();
}

void ImagePanel::actualSize()
{
    if (!m_view || m_empty) return;
    m_view->resetTransform();
    m_fitMode = false;
    m_zoom = 1.0;
    if (m_item) m_view->centerOn(m_item);
    updateZoomLabel();
}

void ImagePanel::applyZoom(qreal factor)
{
    if (!m_view || m_empty) return;
    const qreal target = qBound<qreal>(0.02, m_zoom * factor, 40.0);
    const qreal f = target / m_zoom;
    if (qFuzzyCompare(f, 1.0)) return;

    m_fitMode = false;
    m_view->scale(f, f);          // AnchorUnderMouse 生效
    m_zoom = target;
    updateZoomLabel();
}

void ImagePanel::zoomByButton(qreal factor)
{
    if (!m_view) return;
    // 点按钮时鼠标在底栏上，用「以视图中心为锚点」更符合直觉
    m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    applyZoom(factor);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void ImagePanel::updateZoomLabel()
{
    if (!m_zoomLabel) return;
    const int pct = qRound(m_view ? m_view->transform().m11() * 100.0 : m_zoom * 100.0);
    m_zoomLabel->setText(QString("%1%").arg(pct));
}

bool ImagePanel::eventFilter(QObject *watched, QEvent *event)
{
    if (m_view && watched == m_view->viewport()) {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            const int dy = we->angleDelta().y();
            if (dy != 0) {
                applyZoom(dy > 0 ? 1.15 : 1.0 / 1.15);
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonDblClick) {
            fitToWindow();
            return true;
        }
    }
    return MediaPanel::eventFilter(watched, event);
}

void ImagePanel::resizeEvent(QResizeEvent *event)
{
    MediaPanel::resizeEvent(event);
    if (m_fitMode) fitToWindow();
}
