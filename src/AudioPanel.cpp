#include "AudioPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QWidget>
#include <QScreen>
#include <QGuiApplication>

AudioPanel::AudioPanel(const QString &path, QWidget *parent)
    : MediaPanel(Audio, path, parent, true)
{
    setObjectName("audioFloatBar");
    setFixedHeight(42);
    setFixedWidth(560);

    buildFloatingLayout();

    // 悬浮条本身就是全部 UI，不需要自动隐藏
    enableAutoHide(false);

    m_audioOut = new QAudioOutput(this);
    m_audioOut->setVolume(0.7);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOut);
    m_player->setSource(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &AudioPanel::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &AudioPanel::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &AudioPanel::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &AudioPanel::onDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &AudioPanel::onErrorOccurred);
}

void AudioPanel::buildFloatingLayout()
{
    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    QWidget *bar = new QWidget(this);
    bar->setStyleSheet(
        "QWidget { background: #16213e; border-radius: 6px; border: 1px solid #e94560; }"
        "QLabel#atitle { color: #e94560; font-family: Consolas; font-size: 12px; font-weight: bold; }"
        "QLabel#atime { color: #7f8c8d; font-family: Consolas; font-size: 11px; }"
        "QPushButton#aclose { background: transparent; color: #e94560; border: none; font-size: 14px; font-weight: bold; padding: 0 6px; }"
        "QPushButton#aclose:hover { background: #e94560; color: white; border-radius: 3px; }"
        "QPushButton#actrl { background: transparent; color: #e94560; border: none; font-size: 14px; padding: 0 4px; }"
        "QPushButton#actrl:hover { background: #0f3460; border-radius: 3px; }"
        "QPushButton#avolBtn { background: transparent; color: #e94560; border: none; font-size: 13px; padding: 0 4px; }"
        "QPushButton#avolBtn:hover { background: #0f3460; border-radius: 3px; }"
        "QSlider::groove:horizontal { height: 3px; background: #2a2a4a; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #e94560; width: 10px; height: 10px; margin: -3px 0; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: #e94560; border-radius: 2px; }"
    );

    QHBoxLayout *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(6, 4, 4, 4);
    lay->setSpacing(4);

    QString fileName = QFileInfo(m_path).fileName();
    if (fileName.length() > 16) fileName = fileName.left(16) + "...";
    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x8e\xb5 ") + fileName, bar);
    title->setObjectName("atitle");
    title->setToolTip(QFileInfo(m_path).absoluteFilePath());
    lay->addWidget(title);

    m_playBtn = new QPushButton(QString::fromUtf8("▶"), bar);
    m_playBtn->setObjectName("actrl");
    m_playBtn->setFixedSize(26, 26);
    m_playBtn->setToolTip(QString::fromUtf8("播放/暂停 (Space)"));
    connect(m_playBtn, &QPushButton::clicked, this, [this]() { m_playing ? pause() : play(); });
    lay->addWidget(m_playBtn);

    m_stopBtn = new QPushButton(QString::fromUtf8("■"), bar);
    m_stopBtn->setObjectName("actrl");
    m_stopBtn->setFixedSize(22, 22);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() { stop(); });
    lay->addWidget(m_stopBtn);

    m_progress = new QSlider(Qt::Horizontal, bar);
    m_progress->setRange(0, 1000);
    m_progress->setFixedHeight(18);
    m_progress->installEventFilter(this);
    connect(m_progress, &QSlider::sliderMoved, this, [this](int v) {
        if (m_duration > 0) {
            m_position = qint64(v) * m_duration / 1000;
            updateTimeLabel();
        }
    });
    connect(m_progress, &QSlider::sliderReleased, this, [this]() {
        if (m_duration > 0) seek(qint64(m_progress->value()) * m_duration / 1000);
    });
    lay->addWidget(m_progress, 1);

    m_timeLabel = new QLabel("0:00 / 0:00", bar);
    m_timeLabel->setObjectName("atime");
    m_timeLabel->setMinimumWidth(80);   // 长音频 "1:02:33 / 2:14:08" 不能被截断
    m_timeLabel->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_timeLabel);

    m_muteBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8a"), bar);
    m_muteBtn->setObjectName("avolBtn");
    m_muteBtn->setFixedSize(20, 20);
    connect(m_muteBtn, &QPushButton::clicked, this, [this]() {
        const bool muted = !isMuted();
        setMuted(muted);
        updateMuteIcon();
        // 只改本面板；通知主窗口更新菜单「全部静音」的勾选状态
        emit muteToggled(muted);
    });
    lay->addWidget(m_muteBtn);

    m_volume = new QSlider(Qt::Horizontal, bar);
    m_volume->setRange(0, 100);
    m_volume->setValue(70);
    m_volume->setFixedWidth(50);
    m_volume->setFixedHeight(14);
    m_volume->installEventFilter(this);
    connect(m_volume, &QSlider::valueChanged, this, [this](int v) {
        setVolume(v);
        updateMuteIcon();
    });
    lay->addWidget(m_volume);

    m_closeBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"), bar);
    m_closeBtn->setObjectName("aclose");
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setToolTip(QString::fromUtf8("关闭音频面板"));
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() { emit closed(this); });
    lay->addWidget(m_closeBtn);

    mainLay->addWidget(bar);
}

AudioPanel::~AudioPanel()
{
    if (m_player) {
        m_player->stop();
        m_player->setAudioOutput(nullptr);
    }
}

void AudioPanel::anchorToParent()
{
    QWidget *p = parentWidget();
    if (!p) return;
    const QRect pr = p->rect();
    const int x = pr.left() + (pr.width() - width()) / 2;
    const int y = pr.bottom() - height() - 12;
    move(qMax(pr.left(), x), qMax(pr.top(), y));
}

void AudioPanel::toggleVisible()
{
    if (isVisible()) {
        hide();
    } else {
        anchorToParent();
        show();
        raise();
    }
    emit visibilityToggled();
}

void AudioPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPos = event->globalPosition().toPoint() - pos();
        event->accept();
        return;
    }
    MediaPanel::mousePressEvent(event);
}

void AudioPanel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
        return;
    }
    MediaPanel::mouseMoveEvent(event);
}

void AudioPanel::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    MediaPanel::mouseReleaseEvent(event);
}

void AudioPanel::play()
{
    if (!m_player) return;
    m_player->play();
    m_player->setPlaybackRate(m_rate);
    setPlayingState(true);
}

void AudioPanel::pause()
{
    if (!m_player) return;
    m_player->pause();
    setPlayingState(false);
}

void AudioPanel::stop()
{
    if (!m_player) return;
    m_player->stop();
    setPlayingState(false);
    if (m_progress) {
        QSignalBlocker blocker(m_progress);
        m_progress->setValue(0);
    }
    m_position = 0;
    updateTimeLabel();
}

void AudioPanel::seek(qint64 ms)
{
    if (!m_player || m_duration <= 0) return;
    ms = qBound(qint64(0), ms, m_duration);
    m_player->setPosition(ms);
    m_position = ms;
    if (m_progress) {
        QSignalBlocker blocker(m_progress);
        m_progress->setValue(int(ms * 1000 / m_duration));
    }
    updateTimeLabel();
}

void AudioPanel::setVolume(int percent)
{
    if (!m_audioOut) return;
    percent = qBound(0, percent, 100);
    m_audioOut->setVolume(percent / 100.0);
    syncVolumeUi(percent);
}

void AudioPanel::setMuted(bool muted)
{
    if (m_audioOut) m_audioOut->setMuted(muted);
    updateMuteIcon();
}

void AudioPanel::setPlaybackRate(qreal rate)
{
    m_rate = qBound(0.1, rate, 8.0);
    if (m_player) m_player->setPlaybackRate(m_rate);
}

bool AudioPanel::isMuted() const
{
    return m_audioOut ? m_audioOut->isMuted() : false;
}

void AudioPanel::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::EndOfMedia:
        if (m_loop) {
            m_player->play();
            m_player->setPosition(0);
            setPlayingState(true);
        } else {
            setPlayingState(false);
            if (m_progress) m_progress->setValue(m_progress->maximum());
        }
        break;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        if (m_timeLabel) updateTimeLabel();
        break;
    case QMediaPlayer::InvalidMedia:
        if (m_timeLabel) m_timeLabel->setText("invalid");
        break;
    default:
        break;
    }
}

void AudioPanel::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        setPlayingState(true);
    } else if (state == QMediaPlayer::PausedState || state == QMediaPlayer::StoppedState) {
        setPlayingState(false);
    }
}

void AudioPanel::onPositionChanged(qint64 position)
{
    m_position = position;
    if (m_seeking) return;
    if (m_duration > 0 && m_progress) {
        QSignalBlocker blocker(m_progress);
        m_progress->setValue(int(position * 1000 / m_duration));
    }
    updateTimeLabel();
}

void AudioPanel::onDurationChanged(qint64 duration)
{
    m_duration = duration;
    updateTimeLabel();
}

void AudioPanel::onErrorOccurred(QMediaPlayer::Error error, const QString &message)
{
    Q_UNUSED(error);
    if (m_timeLabel) m_timeLabel->setText(message.left(18));
    setPlayingState(false);
}
