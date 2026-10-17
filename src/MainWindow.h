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
    int m_maxPanels = 12;
    bool m_loop = true;
    bool m_isFullScreen = false;
    bool m_playlistVisible = false;

    QMenu *m_recentMenu = nullptr;

    static const int AMP_LEVELS = 4;
    static const double AMP_RATIOS[AMP_LEVELS];
    static const int MAX_RECENT_FILES = 20;
};