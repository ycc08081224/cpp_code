#include "MediaPanel.h"
#include <QFontMetrics>
#include <QFrame>
#include <QMouseEvent>
#include <QStyle>
#include <QDebug>
#include <QStyleOptionSlider>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QPainterPath>
#include <QRegion>
#include <QEnterEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QDebug>

MediaPanel::MediaPanel(MediaType type, const QString &path, QWidget *parent, bool skipBuild)
    : QWidget(parent), m_type(type), m_path(path)
{
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(2500);
    connect(&m_hideTimer, &QTimer::timeout, this, [this]() {
        if (m_autoHideEnabled && m_playing && m_barsVisible) setBarsVisible(false);
    });

    m_pollTimer.setInterval(150);
    connect(&m_pollTimer, &QTimer::timeout, this, &MediaPanel::pollMousePosition);

    m_volumeHideTimer.setSingleShot(true);
    m_volumeHideTimer.setInterval(600);
    connect(&m_volumeHideTimer, &QTimer::timeout, this, [this]() {
        if (m_volumePanel && !m_volumePanel->underMouse()) {
            m_volumePanel->hide();
            m_volumePanelVisible = false;
        }
    });

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
}

void MediaPanel::pollMousePosition()
{
    QPoint pos = QCursor::pos();

    if (pos != m_lastMousePos) {
        m_lastMousePos = pos;

        QPoint localPos = mapFromGlobal(pos);
        bool insidePanel = rect().contains(localPos);

        if (insidePanel) {
            if (!m_barsVisible) setBarsVisible(true);
            if (m_playing) m_hideTimer.start(2500);
        }
    }
}

void MediaPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_lastMousePos = QCursor::pos();
    if (m_autoHideEnabled) m_pollTimer.start();
}

void MediaPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_pollTimer.stop();
    m_hideTimer.stop();
}

void MediaPanel::buildControls()
{
    m_mediaArea = new QWidget(this);
    m_mediaArea->setStyleSheet("background: transparent;");

    m_topBar = new QWidget(this);
    m_topBar->setStyleSheet(
        "QWidget { background: #1a1a1a; border-top-left-radius: 14px; border-top-right-radius: 14px; }"
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
        "QWidget { background: #1a1a1a; border-bottom-left-radius: 14px; border-bottom-right-radius: 14px; }"
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
    m_timeLabel->setFixedWidth(85);
    ctrlLay->addWidget(m_timeLabel);

    ctrlLay->addSpacing(4);

    m_muteBtn = new QPushButton(QString::fromUtf8("🔊"), m_ctrl);
    m_muteBtn->setObjectName("ctrlBtn");
    m_muteBtn->setFixedSize(26, 26);
    m_muteBtn->setStyleSheet("QPushButton { background: transparent; color: white; border: none; font-size: 14px; padding: 0px; border-radius: 3px; } QPushButton:hover { background: rgba(255,255,255,40); }");
    m_muteBtn->setToolTip("点击静音/取消静音 | 悬停弹出音量条 | 滚轮调节");
    m_muteBtn->installEventFilter(this);
    m_muteBtn->setAttribute(Qt::WA_Hover, true);
    connect(m_muteBtn, &QPushButton::clicked, this, &MediaPanel::onMuteClicked);
    ctrlLay->addWidget(m_muteBtn);

    m_volumePanel = new QWidget(this);
    m_volumePanel->setStyleSheet(
        "QWidget#volPanel { background: #1a1a2e; border: 1px solid #2a2a44; border-radius: 8px; }"
        "QLabel#volLabel { color: #e94560; font-family: Consolas; font-size: 11px; font-weight: bold; background: transparent; border: none; }"
        "QSlider#volSlider::groove:vertical { width: 4px; background: #e94560; border-radius: 2px; }"
        "QSlider#volSlider::handle:vertical { background: #ffffff; width: 14px; height: 14px; margin: -5px -5px -5px -5px; border-radius: 7px; }"
        "QSlider#volSlider::sub-page:vertical { background: rgba(255,255,255,60); border-radius: 2px; }"
    );
    m_volumePanel->setObjectName("volPanel");
    m_volumePanel->setFixedHeight(140);
    m_volumePanel->installEventFilter(this);
    m_volumePanel->setAttribute(Qt::WA_Hover, true);
    m_volumePanel->hide();

    m_volumeLabel = new QLabel("70", m_volumePanel);
    m_volumeLabel->setObjectName("volLabel");
    m_volumeLabel->setAlignment(Qt::AlignCenter);

    m_volume = new QSlider(Qt::Vertical, m_volumePanel);
    m_volume->setObjectName("volSlider");
    m_volume->setRange(0, 100);
    m_volume->setValue(70);
    m_volume->installEventFilter(this);
    connect(m_volume, &QSlider::valueChanged, this, &MediaPanel::onVolumeChanged);

    ctrlLay->addSpacing(4);

    m_playlistBtn = new QPushButton(QString::fromUtf8("📋"), m_ctrl);
    m_playlistBtn->setObjectName("ctrlBtn");
    m_playlistBtn->setFixedSize(26, 26);
    m_playlistBtn->setToolTip("显示/隐藏播放列表");
    connect(m_playlistBtn, &QPushButton::clicked, this, &MediaPanel::playlistToggleClicked);
    ctrlLay->addWidget(m_playlistBtn);
}

void MediaPanel::setBarsVisible(bool visible)
{
    m_barsVisible = visible;
    if (m_topBar) m_topBar->setVisible(visible);
    if (m_ctrl) m_ctrl->setVisible(visible);
    if (visible) {
        if (m_topBar) m_topBar->raise();
        if (m_ctrl) m_ctrl->raise();
    }
}

void MediaPanel::enableAutoHide(bool enable)
{
    m_autoHideEnabled = enable;
    if (enable) {
        if (isVisible()) m_pollTimer.start();
    } else {
        m_pollTimer.stop();
        m_hideTimer.stop();
        setBarsVisible(true);
    }
}

void MediaPanel::setPlayingState(bool playing)
{
    m_playing = playing;
    updatePlayButton();
    if (!playing) {
        m_hideTimer.stop();
        setBarsVisible(true);
    }
}

bool MediaPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_muteBtn) {
        if (event->type() == QEvent::Enter) {
            m_volumeHideTimer.stop();
            if (m_volumePanel) {
                int pw = m_muteBtn->width();
                m_volumePanel->setFixedWidth(pw);
                m_volumeLabel->setGeometry(0, 4, pw, 18);
                int sx = (pw - 12) / 2;
                m_volume->setGeometry(sx, 36, 12, 72);
                QPoint pos = m_muteBtn->mapTo(this, QPoint(0, m_muteBtn->height()));
                m_volumePanel->move(pos.x(), pos.y() - m_volumePanel->height());
                m_volumePanel->show();
                m_volumePanel->raise();
                if (m_ctrl) m_ctrl->raise();
                if (m_topBar) m_topBar->raise();
                m_volumePanelVisible = true;
            }
            return false;
        }
        if (event->type() == QEvent::Leave) {
            m_volumeHideTimer.start(500);
            return false;
        }
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            int delta = we->angleDelta().y() > 0 ? 5 : -5;
            int v = qBound(0, m_volume->value() + delta, 100);
            m_volume->setValue(v);
            return true;
        }
    }

    if (watched == m_volumePanel) {
        if (event->type() == QEvent::Enter) {
            m_volumeHideTimer.stop();
            return false;
        }
        if (event->type() == QEvent::Leave) {
            m_volumeHideTimer.start(500);
            return false;
        }
    }

    if (watched == m_progress) {
        QStyleOptionSlider opt;
        opt.initFrom(m_progress);
        opt.minimum = m_progress->minimum();
        opt.maximum = m_progress->maximum();
        opt.orientation = Qt::Horizontal;
        QRect groove = m_progress->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, m_progress);

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && groove.width() > 0) {
                double ratio = double(me->pos().x() - groove.left()) / double(groove.width());
                ratio = qBound(0.0, ratio, 1.0);
                int v = int(ratio * m_progress->maximum());
                qDebug() << "[DEBUG] m_progress Press pos=" << me->pos() << "groove=" << groove << "ratio=" << ratio << "v=" << v << "duration=" << m_duration;
                m_progress->blockSignals(true);
                m_progress->setValue(v);
                m_progress->blockSignals(false);
                if (m_duration > 0) {
                    qDebug() << "[DEBUG] calling seek(" << (qint64(v) * m_duration / 1000) << ")";
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
            if ((me->buttons() & Qt::LeftButton) && m_seeking && groove.width() > 0) {
                double ratio = double(me->pos().x() - groove.left()) / double(groove.width());
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
        QStyleOptionSlider opt;
        opt.initFrom(m_volume);
        opt.minimum = m_volume->minimum();
        opt.maximum = m_volume->maximum();
        opt.orientation = Qt::Vertical;
        QRect groove = m_volume->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, m_volume);

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && groove.height() > 0) {
                double ratio = double(me->pos().y() - groove.top()) / double(groove.height());
                ratio = qBound(0.0, ratio, 1.0);
                int v = int(m_volume->minimum() + (1.0 - ratio) * (m_volume->maximum() - m_volume->minimum()));
                m_volume->setValue(v);
                return true;
            }
        }
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) && groove.height() > 0) {
                double ratio = double(me->pos().y() - groove.top()) / double(groove.height());
                ratio = qBound(0.0, ratio, 1.0);
                int v = int(m_volume->minimum() + (1.0 - ratio) * (m_volume->maximum() - m_volume->minimum()));
                m_volume->setValue(v);
                return true;
            }
        }
        return false;
    }
    return QWidget::eventFilter(watched, event);
}

void MediaPanel::updatePlayButton()
{
    m_playBtn->setText(m_playing ? QString::fromUtf8("⏸") : QString::fromUtf8("▶"));
}

void MediaPanel::updateTimeLabel()
{
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
void MediaPanel::onVolumeChanged(int value) {
    setVolume(value);
    if (m_volumeLabel) {
        m_volumeLabel->setText(QString::number(value));
    }
    if (m_muteBtn) {
        bool muted = (value == 0);
        m_muteBtn->setText(muted ? QString::fromUtf8("🔇") : QString::fromUtf8("🔊"));
    }
}
void MediaPanel::onMuteClicked() {
    bool nowMuted = isMuted();
    setMuted(!nowMuted);
    if (m_muteBtn) m_muteBtn->setText(nowMuted ? QString::fromUtf8("🔊") : QString::fromUtf8("🔇"));
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

    if (m_mediaArea) m_mediaArea->setGeometry(0, 0, r.width(), r.height());

    QPainterPath p;
    p.addRoundedRect(r, 14, 14);
    setMask(QRegion(p.toFillPolygon().toPolygon()));

    if (m_topBar) {
        int h = 30;
        m_topBar->setGeometry(0, 0, r.width(), h);
        m_topBar->raise();
    }
    if (m_ctrl) {
        int h = 40;
        m_ctrl->setGeometry(0, r.height() - h, r.width(), h);
        m_ctrl->raise();
    }
    if (m_volumePanel) {
        m_volumePanel->setMask(QRegion(m_volumePanel->rect()));
        m_volumePanel->raise();
    }
}

void MediaPanel::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    if (m_autoHideEnabled) {
        if (!m_barsVisible) setBarsVisible(true);
        if (m_playing) m_hideTimer.start(2500);
    }
}

void MediaPanel::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hideTimer.stop();
}

void MediaPanel::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
    if (m_autoHideEnabled) {
        if (!m_barsVisible) setBarsVisible(true);
        if (m_playing) m_hideTimer.start(2500);
    }
}