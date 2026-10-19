#pragma once

#include "MediaPanel.h"
#include <QMediaPlayer>
#include <QAudioOutput>

class AudioPanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit AudioPanel(const QString &path, QWidget *parent = nullptr);
    ~AudioPanel() override;

    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 ms) override;
    void setVolume(int percent) override;
    void setMuted(bool muted) override;
    void setPlaybackRate(qreal rate) override;
    bool isMuted() const override;

    void toggleVisible();

    /// 让悬浮条贴回父窗口底部中央（主窗口 resize / move 时调用）
    void anchorToParent();

signals:
    void visibilityToggled();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &message);

private:
    void buildFloatingLayout();

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOut = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};
