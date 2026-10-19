#pragma once

#include "MediaPanel.h"
#include <QMediaPlayer>
#include <QAudioOutput>

class QVideoWidget;

class VideoPanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit VideoPanel(const QString &path, QWidget *parent = nullptr);
    ~VideoPanel() override;

    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 ms) override;
    void setVolume(int percent) override;
    void setMuted(bool muted) override;
    void setPlaybackRate(qreal rate) override;
    bool isMuted() const override;

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &message);

private:
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOut = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
};
