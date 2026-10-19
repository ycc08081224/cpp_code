#pragma once
#include <QMainWindow>
#include <QSplitter>
#include <QVector>
#include <QHash>
#include <QString>
#include <QSettings>
#include <QTabWidget>
#include <QListWidget>
#include <QIcon>

class MediaPanel;
class AudioPanel;
class QMenu;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    void addPanel(const QString &path);
    void toggleAudio();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void addFiles();
    void addFolder();
    void clearAll();
    void playAll();
    void pauseAll();
    void stopAll();
    void onPanelClosed(MediaPanel *panel);
    void onPanelAmplified(MediaPanel *panel);
    void toggleFullScreen();
    void showHelp();
    void showAbout();
    void openRecentFile();
    void clearRecentFiles();
    void onPlaylistItemActivated(QListWidgetItem *item);
    void togglePlaylist();
    void toggleStatusBar();
    void resetAmplify();
    void closePanelUnderCursor();

private:
    enum MediaType { Video, Audio, Image, Pdf, Text, Other };
    MediaType classify(const QString &path);

    void detachPanels();
    void relayout();
    void loadSettings();
    void saveSettings();
    void savePanelProgress(MediaPanel *panel);
    void restorePanelProgress(MediaPanel *panel);
    void addToRecentFiles(const QString &path);
    void updateRecentFilesMenu();
    void buildPlaylist();
    void refreshPlaylist();
    QIcon drawPlaylistIcon(MediaType t);

    MediaPanel *panelUnderCursor() const;
    MediaPanel *activePanel() const;
    void setAmplifyLevel(int level);
    void layoutAudioPanel();
    void applyPlaybackRate(qreal rate);
    /// 菜单「全部静音」：显式地对所有面板生效
    void applyMuteAll(bool muted);
    /// 单个面板的静音状态变化：只同步菜单勾选，不动其他面板
    void onPanelMuteChanged();

    QWidget *m_container = nullptr;
    QSplitter *m_hSplitter = nullptr;
    QWidget *m_playlistPanel = nullptr;
    QTabWidget *m_playlistTabs = nullptr;
    QListWidget *m_videoList = nullptr;
    QListWidget *m_audioList = nullptr;
    QListWidget *m_imageList = nullptr;
    QListWidget *m_docList = nullptr;
    QSplitter *m_innerSplitter = nullptr;
    QVector<QSplitter*> m_colSplitters;
    QVector<MediaPanel*> m_panels;
    QVector<int> m_ampLevels;
    AudioPanel *m_audioPanel = nullptr;
    QString m_audioPath;
    int m_maxPanels = 12;
    bool m_loop = true;
    bool m_muteAll = false;
    qreal m_rate = 1.0;
    bool m_isFullScreen = false;
    bool m_playlistVisible = false;
    bool m_geometryClamped = false;

    QMenu *m_recentMenu = nullptr;
    QAction *m_loopAction = nullptr;
    QAction *m_muteAllAction = nullptr;
    QAction *m_statusAction = nullptr;
    class QActionGroup *m_speedGroup = nullptr;

    static const int AMP_LEVELS = 4;
    static const double AMP_RATIOS[AMP_LEVELS];
    static const int MAX_RECENT_FILES = 20;
};
