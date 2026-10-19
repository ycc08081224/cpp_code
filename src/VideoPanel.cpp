#include "VideoPanel.h"
#include <QMediaMetaData>
#include <QVideoWidget>
#include <QVBoxLayout>
#include <QSignalBlocker>

VideoPanel::VideoPanel(const QString &path, QWidget *parent)
    : MediaPanel(Video, path, parent)
{
    m_videoWidget = new QVideoWidget(m_mediaArea);
    m_videoWidget->setStyleSheet("background: black; border: none;");
    m_videoWidget->setFocusPolicy(Qt::NoFocus);

    QVBoxLayout *lay = new QVBoxLayout(m_mediaArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_videoWidget);

    m_audioOut = new QAudioOutput(this);
    m_audioOut->setVolume(0.7);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOut);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setSource(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoPanel::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &VideoPanel::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &VideoPanel::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &VideoPanel::onDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &VideoPanel::onErrorOccurred);
}

VideoPanel::~VideoPanel()
{
    if (m_player) {
        m_player->stop();
        m_player->setVideoOutput(nullptr);
        m_player->setAudioOutput(nullptr);
    }
}

void VideoPanel::play()
{
    if (!m_player) return;
    m_player->play();
    m_player->setPlaybackRate(m_rate);
    setPlayingState(true);
}

void VideoPanel::pause()
{
    if (!m_player) return;
    m_player->pause();
    setPlayingState(false);
}

void VideoPanel::stop()
{
    if (!m_player) return;
    m_player->stop();
    setPlayingState(false);
    if (m_progress) m_progress->setValue(0);
    m_position = 0;
    updateTimeLabel();
}

void VideoPanel::seek(qint64 ms)
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

void VideoPanel::setVolume(int percent)
{
    if (!m_audioOut) return;
    percent = qBound(0, percent, 100);
    m_audioOut->setVolume(percent / 100.0);
    syncVolumeUi(percent);
}

void VideoPanel::setMuted(bool muted)
{
    if (m_audioOut) m_audioOut->setMuted(muted);
    updateMuteIcon();
}

void VideoPanel::setPlaybackRate(qreal rate)
{
    m_rate = qBound(0.1, rate, 8.0);
    if (m_player && m_player->playbackState() != QMediaPlayer::StoppedState)
        m_player->setPlaybackRate(m_rate);
}

bool VideoPanel::isMuted() const
{
    return m_audioOut ? m_audioOut->isMuted() : false;
}

void VideoPanel::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadingMedia:
        m_infoLabel->setText("loading..."); break;
    case QMediaPlayer::BufferingMedia:
        m_infoLabel->setText("buffering..."); break;
    case QMediaPlayer::BufferedMedia:
    case QMediaPlayer::LoadedMedia: {
        QSize res = m_player->metaData().value(QMediaMetaData::Resolution).toSize();
        if (res.isValid()) {
            m_infoLabel->setText(QString("%1x%2").arg(res.width()).arg(res.height()));
        } else {
            m_infoLabel->setText("ready");
        }
        break;
    }
    case QMediaPlayer::EndOfMedia:
        if (m_loop) {
            // EndOfMedia 状态下 setPosition 在某些后端无效，
            // 先 play() 回到 StoppedState 再定位到 0 更稳
            m_player->play();
            m_player->setPosition(0);
            setPlayingState(true);
        } else {
            setPlayingState(false);
            if (m_progress) m_progress->setValue(m_progress->maximum());
        }
        break;
    case QMediaPlayer::InvalidMedia:
        m_infoLabel->setText("invalid media"); break;
    default: break;
    }
}

void VideoPanel::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    // 与播放器真实状态保持一致（出错自动停止、播完等情况）
    if (state == QMediaPlayer::PlayingState) {
        setPlayingState(true);
    } else if (state == QMediaPlayer::PausedState || state == QMediaPlayer::StoppedState) {
        setPlayingState(false);
    }
}

void VideoPanel::onPositionChanged(qint64 position)
{
    m_position = position;
    // 用户正在拖动进度条时不要抢滑块，否则会一直回弹
    if (m_seeking) return;
    if (m_duration > 0 && m_progress) {
        QSignalBlocker blocker(m_progress);
        m_progress->setValue(int(position * 1000 / m_duration));
    }
    updateTimeLabel();
}

void VideoPanel::onDurationChanged(qint64 duration)
{
    m_duration = duration;
    updateTimeLabel();
}

void VideoPanel::onErrorOccurred(QMediaPlayer::Error error, const QString &message)
{
    Q_UNUSED(error);
    m_infoLabel->setText("error: " + message.left(30));
    setPlayingState(false);
}
