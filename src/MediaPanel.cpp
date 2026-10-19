#include "MediaPanel.h"
#include <QFontMetrics>
#include <QFrame>
#include <QMouseEvent>
#include <QStyle>
#include <QDebug>
#include <QSignalBlocker>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QPainterPath>
#include <QRegion>
#include <QEnterEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>

// 只有会「播放」的面板才需要轮询鼠标来自动收起控制条；
// 图片/PDF/文本面板的控制条常驻，否则用户连标题栏和关闭按钮都看不到
static bool isPlayableType(MediaPanel::MediaType t)
{
    return t == MediaPanel::Video || t == MediaPanel::Audio;
}

MediaPanel::MediaPanel(MediaType type, const QString &path, QWidget *parent, bool skipBuild)
    : QWidget(parent), m_type(type), m_path(path)
{
    // 100ms + CoarseTimer：Windows 默认时钟精度即可，避免强制 1ms 精度造成调度压力。
    // 用轮询而不是 leaveEvent 判断鼠标离开：鼠标移到子控件（按钮）上时 Qt 也会给父控件发 Leave，
    // 直接依赖它会导致「鼠标一碰按钮，控制条就消失」。
    m_pollTimer.setInterval(100);
    m_pollTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_pollTimer, &QTimer::timeout, this, &MediaPanel::pollMousePosition);

    if (!skipBuild) {
        setAttribute(Qt::WA_Hover, true);
        setStyleSheet(
            "MediaPanel { background: #000000; border-radius: 14px; }"
            "QLabel#title { color: white; font-family: Consolas; font-size: 12px; font-weight: bold; }"
            "QLabel#info { color: rgba(255,255,255,200); font-family: Consolas; font-size: 11px; }"
            "QLabel#time { color: white; font-family: Consolas; font-size: 11px; }"
            "QPushButton#closeBtn { background: transparent; color: white; border: none; font-size: 14px; font-weight: bold; padding: 2px 6px; }"
            "QPushButton#closeBtn:hover { background: rgba(233,69,96,200); color: white; border-radius: 3px; }"
            "QPushButton#ctrlBtn { background: transparent; color: white; border: none; font-size: 14px; padding: 4px 8px; border-radius: 3px; }"
            "QPushButton#ctrlBtn:hover { background: rgba(255,255,255,40); }"
            "QSlider::groove:horizontal { height: 3px; background: rgba(255,255,255,60); border-radius: 1px; }"
            "QSlider::handle:horizontal { background: #e94560; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
            "QSlider::sub-page:horizontal { background: #e94560; border-radius: 1px; }"
            "QSlider::groove:vertical { width: 4px; background: rgba(255,255,255,60); border-radius: 2px; }"
            "QSlider::handle:vertical { background: #e94560; width: 14px; height: 14px; margin: -5px -5px -5px -5px; border-radius: 7px; }"
            "QSlider::sub-page:vertical { background: #e94560; border-radius: 2px; }"
        );
        buildControls();
    }

    // 所有面板统一启用自动隐藏：
    //   - 播放中（视频/音频）鼠标静止 2.5 秒才收起控制条
    //   - 非播放类面板（图片/PDF/文本）永远保持可见，
    //     否则用户看不到标题栏，也就没法关闭/放大面板
    enableAutoHide(true);
    setBarsVisible(true);
}

MediaPanel::~MediaPanel()
{
    // 音量浮层是无父顶层窗口，不随本控件销毁，必须手动释放
    if (m_volumePanel) {
        m_volumePanel->hide();
        m_volumePanel->deleteLater();
        m_volumePanel = nullptr;
    }
}

void MediaPanel::pollMousePosition()
{
    if (!m_autoHideEnabled) return;

    const bool inside = rect().contains(mapFromGlobal(QCursor::pos()));
    if (inside == m_cursorInside) return;
    m_cursorInside = inside;
    updateBarsForCursor();
}

void MediaPanel::updateBarsForCursor()
{
    if (!m_autoHideEnabled) return;

    // 图片/PDF/文本没有播放概念，控制条必须常驻，否则连关闭按钮都看不到
    if (!isPlayableType(m_type)) {
        setBarsVisible(true);
        return;
    }
    setBarsVisible(m_cursorInside);
}

void MediaPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_autoHideEnabled && isPlayableType(m_type)) m_pollTimer.start();
}

void MediaPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_pollTimer.stop();
    m_volumeHideTimer.stop();
    if (m_volumePanel) {
        m_volumePanel->hide();
        m_volumePanelVisible = false;
    }
}

void MediaPanel::buildControls()
{
    m_mediaArea = new QWidget(this);
    m_mediaArea->setStyleSheet("background: transparent;");

    m_topBar = new QWidget(this);
    m_topBar->setStyleSheet(
        "QWidget { background: rgba(26,26,26,51); border-top-left-radius: 14px; border-top-right-radius: 14px; }"
    );
    QHBoxLayout *topLay = new QHBoxLayout(m_topBar);
    topLay->setContentsMargins(8, 4, 6, 4);

    QString prefix;
    switch (m_type) {
    case Video:  prefix = QString::fromUtf8("🎬"); break;
    case Audio:  prefix = QString::fromUtf8("🎵"); break;
    case Image:  prefix = QString::fromUtf8("🖼️"); break;
    case Pdf:    prefix = QString::fromUtf8("📄"); break;
    case Text:   prefix = QString::fromUtf8("📝"); break;
    }

    QString fileName = QFileInfo(m_path).fileName();
    QFontMetrics fm(font());
    int maxChars = 32;
    if (fm.horizontalAdvance(fileName) > fm.horizontalAdvance(QString(maxChars, 'W'))) {
        fileName = fileName.left(maxChars) + "...";
    }

    m_titleLabel = new QLabel(prefix + " " + fileName, m_topBar);
    m_titleLabel->setObjectName("title");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    topLay->addWidget(m_titleLabel, 1);

    m_infoLabel = new QLabel("", m_topBar);
    m_infoLabel->setObjectName("info");
    m_infoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    topLay->addWidget(m_infoLabel);

    m_amplifyBtn = new QPushButton(QString::fromUtf8("⛶"), m_topBar);
    m_amplifyBtn->setObjectName("closeBtn");
    m_amplifyBtn->setFixedSize(24, 24);
    m_amplifyBtn->setToolTip("放大/还原");
    connect(m_amplifyBtn, &QPushButton::clicked, this, &MediaPanel::onAmplifyClicked);
    topLay->addWidget(m_amplifyBtn);

    m_closeBtn = new QPushButton(QString::fromUtf8("✕"), m_topBar);
    m_closeBtn->setObjectName("closeBtn");
    m_closeBtn->setFixedSize(24, 24);
    connect(m_closeBtn, &QPushButton::clicked, this, &MediaPanel::onCloseClicked);
    topLay->addWidget(m_closeBtn);

    m_ctrl = new QWidget(this);
    m_ctrl->setStyleSheet(
        "QWidget { background: rgba(26,26,26,51); border-bottom-left-radius: 14px; border-bottom-right-radius: 14px; }"
    );
    QHBoxLayout *ctrlLay = new QHBoxLayout(m_ctrl);
    ctrlLay->setContentsMargins(6, 2, 6, 4);
    ctrlLay->setSpacing(4);

    m_playBtn = new QPushButton(QString::fromUtf8("▶"), m_ctrl);
    m_playBtn->setObjectName("ctrlBtn");
    m_playBtn->setFixedSize(32, 28);
    connect(m_playBtn, &QPushButton::clicked, this, &MediaPanel::onPlayPauseClicked);
    ctrlLay->addWidget(m_playBtn);

    m_stopBtn = new QPushButton(QString::fromUtf8("■"), m_ctrl);
    m_stopBtn->setObjectName("ctrlBtn");
    m_stopBtn->setFixedSize(32, 28);
    connect(m_stopBtn, &QPushButton::clicked, this, &MediaPanel::onStopClicked);
    ctrlLay->addWidget(m_stopBtn);

    m_progress = new QSlider(Qt::Horizontal, m_ctrl);
    m_progress->setRange(0, 1000);
    m_progress->setTracking(true);
    m_progress->installEventFilter(this);
    connect(m_progress, &QSlider::sliderPressed, this, &MediaPanel::onProgressPressed);
    connect(m_progress, &QSlider::sliderReleased, this, &MediaPanel::onProgressReleased);
    connect(m_progress, &QSlider::sliderMoved, this, &MediaPanel::onProgressMoved);
    ctrlLay->addWidget(m_progress, 1);

    m_timeLabel = new QLabel("0:00 / 0:00", m_ctrl);
    m_timeLabel->setObjectName("time");
    // 不能用 setFixedWidth：长视频显示 "2:09:17 / 2:24:00" 会被截断成 "2:24"
    m_timeLabel->setMinimumWidth(84);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    ctrlLay->addWidget(m_timeLabel);

    ctrlLay->addSpacing(4);

    m_muteBtn = new QPushButton(QString::fromUtf8("🔊"), m_ctrl);
    m_muteBtn->setObjectName("ctrlBtn");
    m_muteBtn->setFixedSize(26, 26);
    m_muteBtn->setStyleSheet("QPushButton { background: transparent; color: white; border: none; font-size: 14px; padding: 0px; border-radius: 3px; } QPushButton:hover { background: rgba(255,255,255,40); }");
    m_muteBtn->setToolTip(QString::fromUtf8("点击静音/取消静音 | 悬停弹出音量条 | 滚轮调节"));
    m_muteBtn->installEventFilter(this);
    m_muteBtn->setAttribute(Qt::WA_Hover, true);
    connect(m_muteBtn, &QPushButton::clicked, this, &MediaPanel::onMuteClicked);
    ctrlLay->addWidget(m_muteBtn);

    // 竖式音量浮层：必须是**无父的顶层窗口**。
    // QVideoWidget 在 Windows 上是原生 HWND，任何画在它上面的 Qt 子控件都会被盖住；
    // 加 WA_NativeWindow 也不行（同级子窗口的 z 序不受 Qt 控制）。
    // 只有顶层 Tool 窗口永远浮在所有普通窗口之上，悬停时才能真正显示在视频画面之上。
    m_volumePanel = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint
                                         | Qt::WindowDoesNotAcceptFocus
                                         | Qt::NoDropShadowWindowHint);
    m_volumePanel->setObjectName("volPanel");
    m_volumePanel->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_volumePanel->setStyleSheet(
        "QWidget#volPanel { background: #000000; border: 1px solid rgba(255,255,255,80); border-radius: 6px; }"
        "QLabel#volLabel { color: #ffffff; font-family: Consolas; font-size: 11px; font-weight: bold;"
        "                  background: transparent; border: none; }"
        "QSlider#volSlider::groove:vertical { width: 5px; background: rgba(255,255,255,45); border-radius: 3px; }"
        "QSlider#volSlider::handle:vertical { background: #e94560; width: 13px; height: 13px;"
        "                                     margin: -5px -4px -5px -4px; border-radius: 7px; }"
        // 竖直滑条的 sub-page 画在滑块“上方”，增长方向其实是 add-page（滑块到最小值/底部），
        // 所以红色要给 add-page：音量越大红色越长、灰色越短
        "QSlider#volSlider::sub-page:vertical { background: rgba(255,255,255,45); border-radius: 3px; }"
        "QSlider#volSlider::add-page:vertical { background: #e94560; border-radius: 3px; }"
    );
    m_volumePanel->setFixedSize(36, 148);
    m_volumePanel->installEventFilter(this);
    m_volumePanel->setAttribute(Qt::WA_Hover, true);
    m_volumePanel->hide();

    m_volumeLabel = new QLabel("70", m_volumePanel);
    m_volumeLabel->setObjectName("volLabel");
    m_volumeLabel->setGeometry(0, 6, 36, 16);
    m_volumeLabel->setAlignment(Qt::AlignCenter);

    m_volume = new QSlider(Qt::Vertical, m_volumePanel);
    m_volume->setObjectName("volSlider");
    m_volume->setGeometry(12, 28, 13, 106);
    m_volume->setRange(0, 100);
    m_volume->setValue(70);
    m_volume->installEventFilter(this);
    connect(m_volume, &QSlider::valueChanged, this, &MediaPanel::onVolumeChanged);

    m_volumeHideTimer.setSingleShot(true);
    m_volumeHideTimer.setInterval(500);
    connect(&m_volumeHideTimer, &QTimer::timeout, this, [this]() {
        if (m_volumePanel && !m_volumePanel->underMouse() && !m_muteBtn->underMouse()) {
            m_volumePanel->hide();
            m_volumePanelVisible = false;
        }
    });

    ctrlLay->addSpacing(4);

    m_playlistBtn = new QPushButton(QString::fromUtf8("📋"), m_ctrl);
    m_playlistBtn->setObjectName("ctrlBtn");
    m_playlistBtn->setFixedSize(26, 26);
    m_playlistBtn->setToolTip("显示/隐藏播放列表");
    connect(m_playlistBtn, &QPushButton::clicked, this, &MediaPanel::playlistToggleClicked);
    ctrlLay->addWidget(m_playlistBtn);

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(m_topBar);
    mainLay->addWidget(m_mediaArea, 1);
    mainLay->addWidget(m_ctrl);

    m_topBar->setFixedHeight(30);
    m_ctrl->setFixedHeight(40);
}

void MediaPanel::removeControlBar()
{
    if (!m_ctrl) return;
    // 先把指针置空，避免 setBarsVisible / 事件处理再碰到它
    QWidget *old = m_ctrl;
    QWidget *oldVolumePanel = m_volumePanel;

    m_ctrl = nullptr;
    m_progress = nullptr;
    m_timeLabel = nullptr;
    m_volume = nullptr;
    m_volumeLabel = nullptr;
    m_volumePanel = nullptr;
    m_muteBtn = nullptr;
    m_playlistBtn = nullptr;
    m_playBtn = nullptr;
    m_stopBtn = nullptr;

    old->hide();
    old->deleteLater();
    if (oldVolumePanel) {
        oldVolumePanel->hide();
        oldVolumePanel->deleteLater();
    }
}

void MediaPanel::showVolumePanel()
{
    if (!m_volumePanel || !m_muteBtn || !m_volume) return;

    if (m_volumeLabel) m_volumeLabel->setText(QString::number(m_volume->value()));

    // 顶层窗口用屏幕坐标定位：贴在音量按钮正上方，与按钮水平居中对齐
    const QPoint anchor = m_muteBtn->mapToGlobal(QPoint(m_muteBtn->width() / 2, 0));
    int x = anchor.x() - m_volumePanel->width() / 2;
    int y = anchor.y() - m_volumePanel->height() - 6;

    QScreen *scr = QGuiApplication::screenAt(anchor);
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (scr) {
        const QRect g = scr->availableGeometry();
        x = qBound(g.left() + 2, x, g.right() - m_volumePanel->width() - 2);
        y = qBound(g.top() + 2, y, g.bottom());
    }

    m_volumePanel->move(x, y);
    m_volumePanel->show();
    m_volumePanel->raise();
    m_volumePanelVisible = true;
}

void MediaPanel::addCustomBottomBar(QWidget *bar)
{
    auto *lay = qobject_cast<QVBoxLayout*>(layout());
    if (!lay || !bar) return;
    bar->setParent(this);
    lay->addWidget(bar);
}

void MediaPanel::setBarsVisible(bool visible)
{
    m_barsVisible = visible;
    if (m_topBar) m_topBar->setVisible(visible);
    if (m_ctrl) m_ctrl->setVisible(visible);
    if (visible) {
        if (m_topBar) m_topBar->raise();
        if (m_ctrl) m_ctrl->raise();
    } else if (m_volumePanel) {
        m_volumePanel->hide();
        m_volumePanelVisible = false;
    }
}

void MediaPanel::enableAutoHide(bool enable)
{
    m_autoHideEnabled = enable;
    if (enable) {
        if (isVisible() && isPlayableType(m_type)) m_pollTimer.start();
    } else {
        m_pollTimer.stop();
        setBarsVisible(true);
    }
}

void MediaPanel::setPlayingState(bool playing)
{
    m_playing = playing;
    updatePlayButton();

    // 控制条显隐只取决于鼠标在不在面板内，与播放状态无关
    updateBarsForCursor();
}

bool MediaPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_muteBtn) {
        if (event->type() == QEvent::Enter) {
            m_volumeHideTimer.stop();
            showVolumePanel();
            return false;
        }
        if (event->type() == QEvent::Leave) {
            m_volumeHideTimer.start();
            return false;
        }
        if (event->type() == QEvent::Wheel && m_volume) {
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            int delta = we->angleDelta().y() > 0 ? 5 : -5;
            m_volume->setValue(qBound(0, m_volume->value() + delta, 100));
            return true;
        }
    }

    if (watched == m_volumePanel) {
        if (event->type() == QEvent::Enter) {
            m_volumeHideTimer.stop();
            return false;
        }
        if (event->type() == QEvent::Leave) {
            m_volumeHideTimer.start();
            return false;
        }
    }

    if (watched == m_progress) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                double ratio = double(me->pos().x()) / double(m_progress->width());
                ratio = qBound(0.0, ratio, 1.0);
                int v = int(ratio * m_progress->maximum());
                m_progress->blockSignals(true);
                m_progress->setValue(v);
                m_progress->blockSignals(false);
                if (m_duration > 0) {
                    seek(qint64(v) * m_duration / 1000);
                    m_seeking = true;
                    m_position = qint64(v) * m_duration / 1000;
                    updateTimeLabel();
                }
                return true;
            }
        }
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && m_seeking) {
                double ratio = double(me->pos().x()) / double(m_progress->width());
                ratio = qBound(0.0, ratio, 1.0);
                int v = int(ratio * m_progress->maximum());
                m_progress->blockSignals(true);
                m_progress->setValue(v);
                m_progress->blockSignals(false);
                m_position = qint64(v) * m_duration / 1000;
                updateTimeLabel();
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && m_seeking) {
                m_seeking = false;
                if (m_duration > 0) {
                    seek(qint64(m_progress->value()) * m_duration / 1000);
                }
                return true;
            }
        }
        return false;
    }

    if (watched == m_volume) {
        // 不要用 QStyle::subControlRect —— 自定义 stylesheet 下返回的 groove 矩形不可信
        // （进度条那边踩过坑：实测返回过 QRect(220,0 4x15)）
        const int h = qMax(1, m_volume->height());
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            const bool pressed = (event->type() == QEvent::MouseButtonPress);
            if (pressed && me->button() != Qt::LeftButton) return false;
            if (!pressed && !(me->buttons() & Qt::LeftButton)) return false;

            double ratio = double(me->pos().y()) / double(h);
            ratio = qBound(0.0, ratio, 1.0);
            int v = int(m_volume->minimum() + (1.0 - ratio) * (m_volume->maximum() - m_volume->minimum()));
            m_volume->setValue(v);
            return true;
        }
        return false;
    }
    return QWidget::eventFilter(watched, event);
}

void MediaPanel::updatePlayButton()
{
    if (!m_playBtn) return;
    m_playBtn->setText(m_playing ? QString::fromUtf8("⏸") : QString::fromUtf8("▶"));
}

void MediaPanel::updateMuteIcon()
{
    if (!m_muteBtn) return;
    const bool muted = isMuted() || (m_volume && m_volume->value() == 0);
    m_muteBtn->setText(muted ? QString::fromUtf8("🔇") : QString::fromUtf8("🔊"));
    m_muteBtn->setToolTip(muted ? QString::fromUtf8("已静音 · 点击恢复")
                                : QString::fromUtf8("点击静音/取消静音 | 悬停弹出音量条 | 滚轮调节"));
}

void MediaPanel::syncVolumeUi(int percent)
{
    if (m_volume) {
        QSignalBlocker blocker(m_volume);
        m_volume->setValue(percent);
    }
    if (m_volumeLabel) m_volumeLabel->setText(QString::number(percent));
    updateMuteIcon();
}

void MediaPanel::updateTimeLabel()
{
    if (!m_timeLabel) return;
    m_timeLabel->setText(formatTime(m_position) + " / " + formatTime(m_duration));
}

QString MediaPanel::formatTime(qint64 ms)
{
    if (ms < 0) ms = 0;
    qint64 totalSec = ms / 1000;
    qint64 h = totalSec / 3600;
    qint64 m = (totalSec % 3600) / 60;
    qint64 s = totalSec % 60;
    if (h > 0) {
        return QString("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(m)
        .arg(s, 2, 10, QChar('0'));
}

void MediaPanel::requestClose() { emit closed(this); }

void MediaPanel::onCloseClicked() { emit closed(this); }
void MediaPanel::onPlayPauseClicked() { m_playing ? pause() : play(); }
void MediaPanel::onStopClicked() { stop(); }
void MediaPanel::onProgressPressed() { m_seeking = true; }
void MediaPanel::onProgressReleased()
{
    if (!m_seeking) return;
    m_seeking = false;
    if (m_duration > 0) {
        seek(qint64(m_progress->value()) * m_duration / 1000);
    }
}
void MediaPanel::onProgressMoved(int value)
{
    if (!m_seeking) return;
    m_position = qint64(value) * m_duration / 1000;
    updateTimeLabel();
}
void MediaPanel::onVolumeChanged(int value)
{
    setVolume(value);
    if (m_volumeLabel) m_volumeLabel->setText(QString::number(value));
    // 注意：这里不能再用 value == 0 推断静音状态，
    // 静音（setMuted）和音量为 0 是两件事，统一交给 updateMuteIcon 判断
    updateMuteIcon();
}
void MediaPanel::onMuteClicked()
{
    const bool muted = !isMuted();
    setMuted(muted);
    updateMuteIcon();
    // 只改本面板；通知主窗口把菜单「全部静音」的勾选状态跟实际情况对齐
    emit muteToggled(muted);
}
void MediaPanel::onAmplifyClicked() { emit amplifyClicked(this); }

void MediaPanel::dragEnterEvent(QDragEnterEvent *event)
{
    event->ignore();
}

void MediaPanel::dropEvent(QDropEvent *event)
{
    event->ignore();
}

void MediaPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    QRect r = rect();
    if (r.width() <= 0 || r.height() <= 0) return;

    if (r != m_lastMaskRect) {
        m_lastMaskRect = r;
        QPainterPath p;
        p.addRoundedRect(r, 14, 14);
        setMask(QRegion(p.toFillPolygon().toPolygon()));
    }
}

void MediaPanel::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    // 移入立即显示，不用等下一次轮询
    if (!m_autoHideEnabled || m_cursorInside) return;
    m_cursorInside = true;
    updateBarsForCursor();
}

void MediaPanel::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    // 注意：鼠标移到子控件（按钮/滑块）时 Qt 也会给父控件发 Leave，
    // 所以这里不做隐藏，交给 pollMousePosition() 用坐标判断真正移出
}

void MediaPanel::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
}