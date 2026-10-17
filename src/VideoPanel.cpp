#include "VideoPanel.h"
#include <QMediaMetaData>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QDebug>

VideoPanel::VideoPanel(const QString &path, QWidget *parent)
    : MediaPanel(Video, path, parent)
{
    enableAutoHide(true);

    m_graphicsView = new QGraphicsView(m_mediaArea);
    m_graphicsView->setFrameShape(QFrame::NoFrame);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphicsView->setDragMode(QGraphicsView::NoDrag);
    m_graphicsView->setRenderHints(QPainter::Antialiasing
                                   | QPainter::SmoothPixmapTransform);
    m_graphicsView->setStyleSheet("background: black; border: none;");
    m_graphicsView->setFocusPolicy(Qt::NoFocus);

    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(m_mediaArea->rect());
    m_graphicsView->setScene(m_scene);

    m_videoItem = new QGraphicsVideoItem();
    m_videoItem->setAcceptHoverEvents(false);
    m_videoItem->setFlags(QGraphicsItem::ItemIsSelectable
                          | QGraphicsItem::ItemIsFocusable);
    m_scene->addItem(m_videoItem);

    QVBoxLayout *lay = new QVBoxLayout(m_mediaArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_graphicsView);

    m_audioOut = new QAudioOutput(this);
    m_audioOut->setVolume(0.7);

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOut);
    m_player->setVideoOutput(m_videoItem);
    m_player->setSource(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));

    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &VideoPanel::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &VideoPanel::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &VideoPanel::onDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &VideoPanel::onErrorOccurred);
    connect(m_videoItem, &QGraphicsVideoItem::nativeSizeChanged,
            this, &VideoPanel::onVideoSizeChanged);
}

VideoPanel::~VideoPanel()
{
    if (m_player) {
        m_player->stop();
        m_player->setVideoOutput(nullptr);
        m_player->setAudioOutput(nullptr);
    }
}

void VideoPanel::resizeEvent(QResizeEvent *event)
{
    MediaPanel::resizeEvent(event);
    updateVideoItemSize();
}

void VideoPanel::updateVideoItemSize()
{
    if (!m_scene || !m_videoItem || !m_mediaArea) return;

    QRectF r = m_mediaArea->rect();
    m_scene->setSceneRect(r);

    QSizeF nativeSize = m_videoItem->nativeSize();
    if (!nativeSize.isEmpty() && nativeSize.width() > 0 && nativeSize.height() > 0) {
        QSizeF scaled = nativeSize;
        scaled.scale(r.size(), Qt::KeepAspectRatio);
        m_videoItem->setSize(scaled);
        m_videoItem->setPos((r.width() - scaled.width()) / 2.0,
                            (r.height() - scaled.height()) / 2.0);
    } else {
        m_videoItem->setSize(r.size());
        m_videoItem->setPos(0, 0);
    }
}

void VideoPanel::onVideoSizeChanged(const QSizeF &size)
{
    Q_UNUSED(size);
    updateVideoItemSize();

    if (m_videoItem && !m_videoItem->nativeSize().isEmpty()) {
        m_infoLabel->setText(QString("%1x%2")
            .arg(int(m_videoItem->nativeSize().width()))
            .arg(int(m_videoItem->nativeSize().height())));
    }
}

void VideoPanel::play()
{
    if (!m_player) return;
    m_player->play();
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
    m_progress->setValue(0);
    m_position = 0;
    updateTimeLabel();
}

void VideoPanel::seek(qint64 ms)
{
    qDebug() << "[DEBUG] VideoPanel::seek(" << ms << ") player=" << m_player << "duration=" << m_duration;
    if (!m_player || m_duration <= 0) return;
    ms = qBound(qint64(0), ms, m_duration);
    m_player->setPosition(ms);
    m_position = ms;
    m_progress->blockSignals(true);
    m_progress->setValue(int(ms * 1000 / m_duration));
    m_progress->blockSignals(false);
    updateTimeLabel();
}

void VideoPanel::setVolume(int percent)
{
    if (!m_audioOut) return;
    percent = qBound(0, percent, 100);
    m_audioOut->setVolume(percent / 100.0);
    if (m_volume) {
        m_volume->blockSignals(true);
        m_volume->setValue(percent);
        m_volume->blockSignals(false);
    }
}

void VideoPanel::setMuted(bool muted)
{
    if (m_audioOut) m_audioOut->setMuted(muted);
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
    case QMediaPlayer::LoadedMedia:
        if (m_videoItem && !m_videoItem->nativeSize().isEmpty()) {
            m_infoLabel->setText(QString("%1x%2")
                .arg(int(m_videoItem->nativeSize().width()))
                .arg(int(m_videoItem->nativeSize().height())));
        } else {
            m_infoLabel->setText("ready");
        }
        break;
    case QMediaPlayer::EndOfMedia:
        if (m_loop && m_duration > 0) { m_player->setPosition(0); m_player->play(); }
        break;
    case QMediaPlayer::InvalidMedia:
        m_infoLabel->setText("invalid media"); break;
    default: break;
    }
}

void VideoPanel::onPositionChanged(qint64 position)
{
    m_position = position;
    if (m_duration > 0) {
        m_progress->blockSignals(true);
        m_progress->setValue(int(position * 1000 / m_duration));
        m_progress->blockSignals(false);
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
}