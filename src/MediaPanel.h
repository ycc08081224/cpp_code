#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QTimer>
#include <QCursor>
#include <QRect>

class MediaPanel : public QWidget
{
    Q_OBJECT
public:
    enum MediaType { Video, Audio, Image, Pdf, Text };

    explicit MediaPanel(MediaType type, const QString &path, QWidget *parent = nullptr, bool skipBuild = false);
    virtual ~MediaPanel();

    MediaType type() const { return m_type; }
    QString path() const { return m_path; }

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 ms) = 0;
    virtual void setVolume(int percent) = 0;
    virtual void setMuted(bool muted) { Q_UNUSED(muted); }
    virtual void setLoop(bool loop) { m_loop = loop; }
    virtual void setPlaybackRate(qreal rate) { m_rate = rate; }
    virtual bool isMuted() const { return false; }
    virtual void setPlayingState(bool playing);
    virtual bool isPlaying() const { return m_playing; }
    virtual qint64 duration() const { return m_duration; }
    virtual qint64 position() const { return m_position; }

    qreal playbackRate() const { return m_rate; }

    void setBarsVisible(bool visible);
    void enableAutoHide(bool enable);
    static QString formatTime(qint64 ms);

    /// 请求关闭（默认直接发 closed；TextPanel 用它弹「未保存」确认）
    virtual void requestClose();

signals:
    void closed(MediaPanel *panel);
    void amplifyClicked(MediaPanel *panel);
    void playlistToggleClicked();
    /// 单面板的静音按钮被点击：静音是全局状态，交给 MainWindow 统一同步所有面板
    void muteToggled(bool muted);

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
    /// 彻底移除底部控制条（图片/PDF/文本这类没有播放概念的媒体用）
    void removeControlBar();
    /// 在底部追加自定义工具条（图片缩放条等），须在 removeControlBar() 之后调用
    void addCustomBottomBar(QWidget *bar);
    /// 在音量按钮正上方弹出竖式音量条（无父顶层窗口，才能盖住原生视频画面）
    void showVolumePanel();
    void updatePlayButton();
    void updateTimeLabel();
    void updateMuteIcon();
    void syncVolumeUi(int percent);
    /// 按「鼠标是否在面板内」刷新控制条可见性（使用说明：移入显示 / 移出隐藏）
    void updateBarsForCursor();

    MediaType m_type;
    QString m_path;
    bool m_playing = false;
    bool m_loop = true;
    bool m_seeking = false;
    qint64 m_duration = 0;
    qint64 m_position = 0;
    qreal m_rate = 1.0;

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

    QTimer m_pollTimer;
    QTimer m_volumeHideTimer;
    bool m_autoHideEnabled = false;
    bool m_barsVisible = true;
    bool m_volumePanelVisible = false;
    bool m_cursorInside = false;
    QRect m_lastMaskRect;

    void onAmplifyClicked();
};
