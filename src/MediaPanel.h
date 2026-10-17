#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QComboBox>
#include <QElapsedTimer>
#include <QVariant>
#include <QTimer>
#include <QPropertyAnimation>
#include <QCursor>
#include <QWheelEvent>

class MediaPanel : public QWidget
{
    Q_OBJECT
public:
    enum MediaType { Video, Audio, Image, Pdf, Text };

    explicit MediaPanel(MediaType type, const QString &path, QWidget *parent = nullptr, bool skipBuild = false);
    virtual ~MediaPanel() = default;

    MediaType type() const { return m_type; }
    QString path() const { return m_path; }

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 ms) = 0;
    virtual void setVolume(int percent) = 0;
    virtual void setMuted(bool muted) { Q_UNUSED(muted); }
    virtual void setLoop(bool loop) { m_loop = loop; }
    virtual bool isMuted() const { return false; }
    virtual void setPlayingState(bool playing);
    virtual bool isPlaying() const { return m_playing; }
    virtual qint64 duration() const { return m_duration; }
    virtual qint64 position() const { return m_position; }

    void setBarsVisible(bool visible);
    void enableAutoHide(bool enable);
    static QString formatTime(qint64 ms);

signals:
    void closed(MediaPanel *panel);
    void amplifyClicked(MediaPanel *panel);
    void playlistToggleClicked();

protected slots:
    void onCloseClicked();
    void onPlayPauseClicked();
    void onStopClicked();
    void onProgressPressed();
    void onProgressReleased();
    void onProgressMoved(int value);
    void onVolumeChanged(int value);
    void onMuteClicked();
    void pollMousePosition();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    virtual void buildControls();
    void updatePlayButton();
    void updateTimeLabel();

    MediaType m_type;
    QString m_path;
    bool m_playing = false;
    bool m_loop = true;
    bool m_seeking = false;
    qint64 m_duration = 0;
    qint64 m_position = 0;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_infoLabel = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_amplifyBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QSlider *m_progress = nullptr;
    QPushButton *m_muteBtn = nullptr;
    QSlider *m_volume = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_volumeLabel = nullptr;
    QWidget *m_volumePanel = nullptr;
    QPushButton *m_playlistBtn = nullptr;
    QWidget *m_mediaArea = nullptr;
    QWidget *m_topBar = nullptr;
    QWidget *m_ctrl = nullptr;

    QTimer m_hideTimer;
    QTimer m_pollTimer;
    QTimer m_volumeHideTimer;
    bool m_autoHideEnabled = false;
    bool m_barsVisible = true;
    bool m_volumePanelVisible = false;
    QPoint m_lastMousePos;

    void onAmplifyClicked();
};