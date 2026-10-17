#pragma once

#include "MediaPanel.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>

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
    bool isMuted() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &message);
    void onVideoSizeChanged(const QSizeF &size);

    void updateVideoItemSize();

private:
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOut = nullptr;
    QGraphicsView *m_graphicsView = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsVideoItem *m_videoItem = nullptr;
};