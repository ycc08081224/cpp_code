#include "MainWindow.h"
#include "VideoPanel.h"
#include "AudioPanel.h"
#include "ImagePanel.h"
#include "PdfPanel.h"
#include "TextPanel.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QStatusBar>
#include <QSplitter>
#include <QDir>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <QMessageBox>
#include <QDialog>
#include <QScrollArea>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QAbstractItemView>
#include <QStyle>
#include <QIcon>
#include <QApplication>
#include <QCursor>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QAbstractSpinBox>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QComboBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QSet>
#include <QList>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>

const double MainWindow::AMP_RATIOS[AMP_LEVELS] = { 0.0, 0.25, 0.3333, 0.5 };

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("全能媒体播放器");
    setMinimumSize(800, 550);
    setAcceptDrops(true);

    QMenuBar *menuBar = this->menuBar();

    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    auto *openAct = fileMenu->addAction("添加文件...");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::addFiles);

    auto *openDirAct = fileMenu->addAction("添加文件夹...");
    openDirAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openDirAct, &QAction::triggered, this, &MainWindow::addFolder);

    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu("最近打开(&R)");
    fileMenu->addAction("清除历史", this, &MainWindow::clearRecentFiles);
    fileMenu->addSeparator();
    fileMenu->addAction("全部清除", this, &MainWindow::clearAll);
    fileMenu->addSeparator();
    auto *quitAct = fileMenu->addAction("退出");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    QMenu *playMenu = menuBar->addMenu("播放(&P)");
    playMenu->addAction("全部播放", this, &MainWindow::playAll);
    playMenu->addAction("全部暂停", this, &MainWindow::pauseAll);
    playMenu->addAction("全部停止", this, &MainWindow::stopAll);
    playMenu->addSeparator();
    m_loopAction = playMenu->addAction("循环播放");
    m_loopAction->setCheckable(true);
    m_loopAction->setChecked(m_loop);
    connect(m_loopAction, &QAction::toggled, this, [this](bool on) {
        m_loop = on;
        for (auto *pp : m_panels) pp->setLoop(on);
        if (m_audioPanel) m_audioPanel->setLoop(on);
    });

    m_muteAllAction = playMenu->addAction(QString::fromUtf8("全部静音"));
    m_muteAllAction->setCheckable(true);
    m_muteAllAction->setChecked(false);
    connect(m_muteAllAction, &QAction::toggled, this, [this](bool on) {
        applyMuteAll(on);
    });

    playMenu->addSeparator();
    QMenu *speedMenu = playMenu->addMenu("播放速度");
    static const struct { const char *label; qreal rate; } kRates[] = {
        { "0.5x", 0.5 }, { "0.75x", 0.75 }, { "1.0x", 1.0 },
        { "1.25x", 1.25 }, { "1.5x", 1.5 }, { "2.0x", 2.0 }
    };
    auto *speedGroup = new QActionGroup(this);
    speedGroup->setExclusive(true);
    for (const auto &r : kRates) {
        QAction *act = speedMenu->addAction(QString::fromUtf8(r.label));
        act->setCheckable(true);
        act->setChecked(qFuzzyCompare(r.rate, 1.0));
        act->setData(r.rate);
        speedGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, rate = r.rate]() { applyPlaybackRate(rate); });
    }
    m_speedGroup = speedGroup;

    QMenu *viewMenu = menuBar->addMenu("视图(&V)");
    viewMenu->addAction("全屏模式", this, &MainWindow::toggleFullScreen, QKeySequence(Qt::Key_F11));
    viewMenu->addAction("显示/隐藏播放列表", this, &MainWindow::togglePlaylist, QKeySequence(Qt::CTRL | Qt::Key_L));
    viewMenu->addAction("还原面板尺寸", this, &MainWindow::resetAmplify, QKeySequence(Qt::CTRL | Qt::Key_0));
    viewMenu->addSeparator();
    m_statusAction = viewMenu->addAction("显示状态栏");
    m_statusAction->setCheckable(true);
    m_statusAction->setChecked(true);
    connect(m_statusAction, &QAction::toggled, this, &MainWindow::toggleStatusBar);

    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    helpMenu->addAction("使用说明", this, &MainWindow::showHelp, QKeySequence::HelpContents);
    helpMenu->addAction("关于", this, &MainWindow::showAbout);

    m_container = new QWidget(this);
    m_container->setStyleSheet("background: #0f0f23;");
    setCentralWidget(m_container);
    qApp->installEventFilter(this);

    m_innerSplitter = new QSplitter(Qt::Horizontal, m_container);
    m_innerSplitter->setChildrenCollapsible(false);
    m_innerSplitter->setHandleWidth(4);
    m_innerSplitter->setStyleSheet("QSplitter::handle { background: #2a2a40; }");

    m_hSplitter = new QSplitter(Qt::Horizontal, m_innerSplitter);
    m_hSplitter->setChildrenCollapsible(false);
    m_hSplitter->setHandleWidth(4);
    m_hSplitter->setStyleSheet("QSplitter::handle { background: #2a2a40; }");

    buildPlaylist();
    m_innerSplitter->addWidget(m_hSplitter);
    m_innerSplitter->addWidget(m_playlistPanel);
    m_innerSplitter->setStretchFactor(0, 1);
    m_innerSplitter->setStretchFactor(1, 0);
    m_innerSplitter->setSizes(QList<int>() << 800 << 260);

    QVBoxLayout *root = new QVBoxLayout(m_container);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(0);
    root->addWidget(m_innerSplitter);

    // 之前这里直接 hide()，导致全程序所有 showMessage 提示（加载成功/截断/已达上限…）
    // 用户一条都看不到。改成深色主题常驻，并在视图菜单里保留开关。
    statusBar()->setStyleSheet(
        "QStatusBar { background: #1c1c32; color: #a8a8d0; font-size: 12px;"
        "             border-top: 1px solid #2a2a44; padding-left: 6px; }"
        "QStatusBar::item { border: none; }"
    );
    statusBar()->setSizeGripEnabled(false);
    statusBar()->showMessage(QString::fromUtf8("就绪 · 拖入文件或按 Ctrl+O 添加 · F1 查看帮助"));

    // 临时提示（2~3 秒）过期后回到常驻文案，否则状态栏会变成一条空白带
    connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString &msg) {
        if (!msg.isEmpty()) return;
        QTimer::singleShot(0, this, [this]() {
            if (statusBar()->currentMessage().isEmpty())
                statusBar()->showMessage(QString::fromUtf8("就绪 · 拖入文件或按 Ctrl+O 添加 · F1 查看帮助"));
        });
    });

    // Ctrl+1~9 直接设定放大比例（0=原始 1=1/4 2=1/3 3=1/2）
    for (int i = 1; i <= 9; ++i) {
        auto *sc = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + i)), this);
        connect(sc, &QShortcut::activated, this, [this, i]() { setAmplifyLevel(i); });
    }
    auto *closeSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this);
    connect(closeSc, &QShortcut::activated, this, &MainWindow::closePanelUnderCursor);

    auto *audioSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_M), this);
    connect(audioSc, &QShortcut::activated, this, &MainWindow::toggleAudio);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect screenRect = screen->availableGeometry();
        int w = qMax(900, static_cast<int>(screenRect.width() * 0.75));
        int h = qMax(600, static_cast<int>(screenRect.height() * 0.75));
        resize(w, h);
        move(screenRect.center() - rect().center());
    }

    loadSettings();
    updateRecentFilesMenu();
    refreshPlaylist();
    m_playlistPanel->hide();
}

MainWindow::MediaType MainWindow::classify(const QString &path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    static const QSet<QString> video {
        "mp4","avi","mkv","mov","flv","wmv","webm","m4v","mpg","mpeg","ts","3gp","rmvb","rm","vob"
    };
    static const QSet<QString> audio { "mp3","wav","flac","aac","ogg","wma","m4a" };
    static const QSet<QString> image { "jpg","jpeg","png","gif","bmp","webp","tiff","ico" };
    if (video.contains(ext)) return Video;
    if (audio.contains(ext)) return Audio;
    if (image.contains(ext)) return Image;
    if (ext == "pdf") return Pdf;
    static const QSet<QString> text {
        "txt","md","csv","log","py","java","c","cpp","h","json","xml",
        "html","css","js","yml","yaml","ini","cfg","sql","sh","bat",
        "doc","docx","rtf","ppt","pptx","pptm","ppsm"
    };
    if (text.contains(ext)) return Text;
    return Other;
}

void MainWindow::addPanel(const QString &path)
{
    MediaType t = classify(path);
    if (t == Other) {
        statusBar()->showMessage("不支持的文件: " + QFileInfo(path).fileName(), 3000);
        return;
    }

    addToRecentFiles(QFileInfo(path).absoluteFilePath());

    if (t == Audio) {
        if (m_audioPanel) {
            m_audioPanel->close();
            m_audioPanel->deleteLater();
            m_audioPanel = nullptr;
        }
        AudioPanel *panel = new AudioPanel(path, m_container);
        connect(panel, &MediaPanel::closed, this, [this](MediaPanel *) {
            if (m_audioPanel) {
                m_audioPanel->stop();
                m_audioPanel->deleteLater();
                m_audioPanel = nullptr;
                m_audioPath.clear();
            }
        });
        connect(panel, &MediaPanel::playlistToggleClicked, this, &MainWindow::togglePlaylist);
        connect(panel, &MediaPanel::muteToggled, this, &MainWindow::onPanelMuteChanged);
        m_audioPanel = panel;
        m_audioPath = QFileInfo(path).absoluteFilePath();
        panel->setLoop(m_loop);
        panel->setPlaybackRate(m_rate);
        panel->setMuted(m_muteAll);
        panel->adjustSize();
        layoutAudioPanel();
        panel->show();
        panel->raise();
        statusBar()->showMessage(QString("已加载音频: %1 (Ctrl+M 切换)").arg(QFileInfo(path).fileName()), 3000);
        refreshPlaylist();
        return;
    }
    if (m_panels.size() >= m_maxPanels) {
        statusBar()->showMessage("已达最大面板数", 3000);
        return;
    }
    MediaPanel *panel = nullptr;
    switch (t) {
    case Video: panel = new VideoPanel(path, nullptr); break;
    case Image: panel = new ImagePanel(path, nullptr); break;
    case Pdf:   panel = new PdfPanel(path, nullptr);   break;
    case Text:  panel = new TextPanel(path, nullptr);  break;
    default: return;
    }
    connect(panel, &MediaPanel::closed, this, &MainWindow::onPanelClosed);
    connect(panel, &MediaPanel::amplifyClicked, this, &MainWindow::onPanelAmplified);
    connect(panel, &MediaPanel::playlistToggleClicked, this, &MainWindow::togglePlaylist);
    connect(panel, &MediaPanel::muteToggled, this, &MainWindow::onPanelMuteChanged);
    panel->setLoop(m_loop);
    panel->setPlaybackRate(m_rate);
    panel->setMuted(m_muteAll);
    m_panels.append(panel);
    m_ampLevels.append(0);
    relayout();
    restorePanelProgress(panel);
    statusBar()->showMessage(QString("已加载 %1 / %2").arg(m_panels.size()).arg(m_maxPanels));
    refreshPlaylist();
}

void MainWindow::detachPanels()
{
    if (!m_hSplitter) return;

    for (int c = m_hSplitter->count() - 1; c >= 0; --c) {
        QSplitter *col = qobject_cast<QSplitter*>(m_hSplitter->widget(c));
        if (!col) continue;
        for (int i = col->count() - 1; i >= 0; --i) {
            if (QWidget *w = col->widget(i))
                w->setParent(nullptr);
        }
    }

    QList<QWidget*> colSplitters;
    for (int i = m_hSplitter->count() - 1; i >= 0; --i)
        colSplitters.append(m_hSplitter->widget(i));
    for (QWidget *w : colSplitters)
        delete w;
    m_colSplitters.clear();
}

void MainWindow::relayout()
{
    detachPanels();

    int n = m_panels.size();
    if (n == 0 || !m_hSplitter) return;

    int cols;
    if (n == 1)        cols = 1;
    else if (n <= 3)   cols = n;
    else if (n <= 4)   cols = 2;
    else if (n <= 6)   cols = 3;
    else if (n <= 9)   cols = 3;
    else               cols = 4;

    m_colSplitters.resize(cols);
    for (int c = 0; c < cols; ++c) {
        m_colSplitters[c] = new QSplitter(Qt::Vertical, m_hSplitter);
        m_colSplitters[c]->setChildrenCollapsible(false);
        m_colSplitters[c]->setHandleWidth(4);
        m_colSplitters[c]->setStyleSheet("QSplitter::handle { background: #2a2a40; }");
        m_hSplitter->addWidget(m_colSplitters[c]);
    }

    for (int i = 0; i < n; ++i) {
        int c = i % cols;
        m_colSplitters[c]->addWidget(m_panels[i]);
    }

    int totalW = m_hSplitter->width();
    if (totalW < 50) totalW = width() - 20;
    int minW = qMax(20, totalW / 24);

    QVector<double> colRatios(cols, 0.0);
    QVector<bool> colHasAmp(cols, false);

    for (int i = 0; i < n; ++i) {
        int c = i % cols;
        int lvl = m_ampLevels[i];
        if (lvl > 0 && lvl < AMP_LEVELS) {
            colRatios[c] = qMax(colRatios[c], AMP_RATIOS[lvl]);
            colHasAmp[c] = true;
        }
    }

    double used = 0.0;
    int dynCols = 0;
    for (int c = 0; c < cols; ++c) {
        if (colHasAmp[c]) used += colRatios[c];
        else ++dynCols;
    }
    double remaining = qMax(0.0, 1.0 - used);
    double eachDynamic = dynCols > 0 ? remaining / dynCols : 0.0;

    QList<int> sizes;
    bool allOk = true;
    for (int c = 0; c < cols; ++c) {
        double r = colHasAmp[c] ? colRatios[c] : eachDynamic;
        int w = qMax(1, qRound(totalW * r));
        sizes.append(w);
        if (w < minW) allOk = false;
    }

    if (!allOk) {
        for (int i = 0; i < n; ++i) {
            if (m_ampLevels[i] > 0) {
                statusBar()->showMessage("空间不足，还原一个面板", 2000);
                m_ampLevels[i] = 0;
                relayout();
                return;
            }
        }
    }

    m_hSplitter->setSizes(sizes);

    for (int c = 0; c < cols; ++c) {
        int rc = m_colSplitters[c]->count();
        if (rc > 0) {
            QList<int> rowHeights;
            for (int j = 0; j < rc; ++j) rowHeights.append(1000);
            m_colSplitters[c]->setSizes(rowHeights);
        }
    }
}

void MainWindow::onPanelAmplified(MediaPanel *panel)
{
    int idx = m_panels.indexOf(panel);
    if (idx < 0) return;
    m_ampLevels[idx] = (m_ampLevels[idx] + 1) % AMP_LEVELS;
    relayout();

    static const char *labels[] = { "原始", "1/4", "1/3", "1/2" };
    statusBar()->showMessage(QString("尺寸: %1").arg(labels[m_ampLevels[idx]]), 1500);
}

void MainWindow::onPanelClosed(MediaPanel *panel)
{
    QElapsedTimer t; t.start();
    qDebug() << "[Close] === onPanelClosed start ===";

    savePanelProgress(panel);
    qDebug() << "[Close] savePanelProgress:" << t.elapsed() << "ms";

    int idx = m_panels.indexOf(panel);
    if (idx >= 0) {
        m_panels.removeAt(idx);
        m_ampLevels.removeAt(idx);
    }
    qDebug() << "[Close] removeFromList:" << t.elapsed() << "ms" << "remaining:" << m_panels.size();

    panel->stop();
    qDebug() << "[Close] panel->stop():" << t.elapsed() << "ms";

    panel->setParent(nullptr);
    qDebug() << "[Close] detach:" << t.elapsed() << "ms";

    // 即使关掉的是最后一个面板也要 relayout：detachPanels 会销毁残留的空列 splitter，
    // 否则 N 次关闭后窗口里会堆一排空的拖拽条
    relayout();
    qDebug() << "[Close] relayout:" << t.elapsed() << "ms";

    panel->deleteLater();
    qDebug() << "[Close] deleteLater:" << t.elapsed() << "ms";
    qDebug() << "[Close] === done ===";
}

void MainWindow::addFiles()
{
    QString filter =
        "所有文件 (*);;"
        "媒体文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv *.webm *.m4v *.mpg *.mpeg *.ts *.3gp *.rmvb *.rm *.vob "
        "*.mp3 *.wav *.flac *.aac *.ogg *.wma *.m4a "
        "*.jpg *.jpeg *.png *.gif *.bmp *.webp *.tiff *.ico "
        "*.pdf *.txt *.md *.csv *.log *.json *.xml *.html *.css *.js *.py *.java *.c *.cpp *.h *.yml *.yaml *.ini *.cfg *.sql *.sh *.bat)";
    QStringList files = QFileDialog::getOpenFileNames(this, "选择文件", QString(), filter);
    if (files.isEmpty()) return;
    if (files.size() > m_maxPanels) {
        files = files.mid(0, m_maxPanels);
        statusBar()->showMessage(QString("已截断到最多 %1 个文件").arg(m_maxPanels), 2500);
    }
    for (const QString &f : files) addPanel(f);
}

void MainWindow::addFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹");
    if (dir.isEmpty()) return;

    QDir d(dir);
    QStringList allFiles = d.entryList(QDir::Files);
    QStringList supported;
    for (const QString &f : allFiles) {
        if (classify(d.absoluteFilePath(f)) != Other)
            supported.append(f);
    }

    int found = supported.size();
    for (const QString &f : supported) {
        if (m_panels.size() >= m_maxPanels) break;
        addPanel(d.absoluteFilePath(f));
    }

    if (found == 0) {
        statusBar()->showMessage("该文件夹内没有找到支持的媒体文件", 3000);
    } else if (m_panels.size() >= m_maxPanels && found > m_maxPanels) {
        statusBar()->showMessage(QString("找到 %1 个文件，但最多同时加载 %2 个").arg(found).arg(m_maxPanels), 4000);
    }
}

void MainWindow::clearAll()
{
    if (m_panels.isEmpty() && !m_audioPanel) return;

    // 误点一次就把十几个面板全清掉太狠，先确认
    if (!m_panels.isEmpty()) {
        auto ret = QMessageBox::question(this, QString::fromUtf8("全部清除"),
            QString::fromUtf8("确定关闭全部 %1 个面板吗？").arg(m_panels.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    for (auto *p : m_panels) {
        p->stop();
        savePanelProgress(p);
        p->setParent(nullptr);
        p->deleteLater();
    }
    m_panels.clear();
    m_ampLevels.clear();

    if (m_audioPanel) {
        m_audioPanel->stop();
        m_audioPanel->hide();
        m_audioPanel->deleteLater();
        m_audioPanel = nullptr;
        m_audioPath.clear();
    }

    detachPanels();
    statusBar()->showMessage(QString::fromUtf8("已清空全部面板"), 2000);
}

void MainWindow::playAll()
{
    // 静音状态完全归各面板自己管：这里只负责播放，
    // 既不搞「第一个视频有声、其余静音」，也不把某个面板的静音状态同步给别人
    for (auto *p : m_panels) {
        if (!p->isPlaying()) p->play();
    }
    if (m_audioPanel) m_audioPanel->play();
}

void MainWindow::onPanelMuteChanged()
{
    // 单面板的静音按钮只影响自己，这里仅把菜单「全部静音」的勾选状态跟实际情况对齐
    bool allMuted = true;
    bool any = false;

    for (auto *p : m_panels) {
        if (p->type() == MediaPanel::Video || p->type() == MediaPanel::Audio) {
            any = true;
            if (!p->isMuted()) { allMuted = false; break; }
        }
    }
    if (allMuted && m_audioPanel) {
        any = true;
        if (!m_audioPanel->isMuted()) allMuted = false;
    }

    m_muteAll = any && allMuted;
    if (m_muteAllAction) {
        QSignalBlocker blocker(m_muteAllAction);
        m_muteAllAction->setChecked(m_muteAll);
        m_muteAllAction->setText(m_muteAll ? QString::fromUtf8("取消全部静音")
                                           : QString::fromUtf8("全部静音"));
    }
}

void MainWindow::applyMuteAll(bool muted)
{
    m_muteAll = muted;

    for (auto *p : m_panels) {
        if (p->type() == MediaPanel::Video || p->type() == MediaPanel::Audio)
            p->setMuted(muted);
    }
    if (m_audioPanel) m_audioPanel->setMuted(muted);

    // 菜单勾选/文字与实际静音状态保持一致（此处回写 setChecked 会再次触发 toggled）
    if (m_muteAllAction) {
        QSignalBlocker blocker(m_muteAllAction);
        m_muteAllAction->setChecked(muted);
        m_muteAllAction->setText(muted ? QString::fromUtf8("取消全部静音")
                                       : QString::fromUtf8("全部静音"));
    }
}
void MainWindow::pauseAll() { for (auto *p : m_panels) if (p->isPlaying())  p->pause(); if (m_audioPanel) m_audioPanel->pause(); }
void MainWindow::stopAll()  { for (auto *p : m_panels) p->stop(); if (m_audioPanel) m_audioPanel->stop(); }

void MainWindow::toggleAudio()
{
    if (!m_audioPanel) {
        statusBar()->showMessage(QString::fromUtf8("当前没有音频面板（拖入 mp3 / wav / flac 等音频文件即可）"), 2500);
        return;
    }
    m_audioPanel->toggleVisible();
}

void MainWindow::layoutAudioPanel()
{
    if (m_audioPanel) m_audioPanel->anchorToParent();
}

MediaPanel *MainWindow::panelUnderCursor() const
{
    QWidget *w = QApplication::widgetAt(QCursor::pos());
    while (w) {
        if (auto *mp = qobject_cast<MediaPanel*>(w)) return mp;
        w = w->parentWidget();
    }
    return nullptr;
}

MediaPanel *MainWindow::activePanel() const
{
    return m_panels.isEmpty() ? nullptr : m_panels.last();
}

void MainWindow::setAmplifyLevel(int level)
{
    if (m_panels.isEmpty()) return;
    MediaPanel *target = panelUnderCursor();
    if (!target) target = activePanel();

    const int idx = m_panels.indexOf(target);
    if (idx < 0) return;

    m_ampLevels[idx] = qBound(0, level, AMP_LEVELS - 1);
    relayout();

    static const char *labels[] = { "原始", "1/4", "1/3", "1/2" };
    statusBar()->showMessage(QString::fromUtf8("尺寸: %1").arg(QString::fromUtf8(labels[m_ampLevels[idx]])), 1500);
}

void MainWindow::resetAmplify()
{
    bool changed = false;
    for (int i = 0; i < m_ampLevels.size(); ++i) {
        if (m_ampLevels[i] != 0) { m_ampLevels[i] = 0; changed = true; }
    }
    if (changed) relayout();
    statusBar()->showMessage(QString::fromUtf8("已还原为原始尺寸"), 1500);
}

void MainWindow::closePanelUnderCursor()
{
    // 只关鼠标指着的那一个，避免误关最后一个面板
    MediaPanel *target = panelUnderCursor();
    if (target) {
        target->requestClose();
        return;
    }
    if (m_audioPanel && m_audioPanel->geometry().contains(
            m_audioPanel->mapFromGlobal(QCursor::pos()))) {
        m_audioPanel->requestClose();
        return;
    }
    statusBar()->showMessage(QString::fromUtf8("请把鼠标移到要关闭的面板上再按 Ctrl+W"), 2000);
}

void MainWindow::applyPlaybackRate(qreal rate)
{
    m_rate = rate;
    for (auto *p : m_panels) p->setPlaybackRate(rate);
    if (m_audioPanel) m_audioPanel->setPlaybackRate(rate);
    statusBar()->showMessage(QString::fromUtf8("播放速度: %1x").arg(rate), 1500);
}

void MainWindow::toggleStatusBar()
{
    const bool on = m_statusAction ? m_statusAction->isChecked() : true;
    statusBar()->setVisible(on);
}

void MainWindow::toggleFullScreen()
{
    if (m_isFullScreen) {
        showNormal();
        m_isFullScreen = false;
    } else {
        showFullScreen();
        m_isFullScreen = true;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F11) {
        toggleFullScreen();
        event->accept();
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_M) {
            toggleAudio();
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type t = event->type();

    // ---- 音频悬浮条跟随主窗口与容器尺寸变化 ----
    if ((watched == this || watched == m_container) && (t == QEvent::Resize || t == QEvent::Move)) {
        if (m_audioPanel && m_audioPanel->isVisible())
            QTimer::singleShot(0, this, [this]() { layoutAudioPanel(); });
        return QMainWindow::eventFilter(watched, event);
    }

    // ---- 键盘：Space / ←→ / Esc ----
    if (t == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape && m_isFullScreen) {
            toggleFullScreen();
            return true;
        }

        QWidget *focus = QApplication::focusWidget();
        // 文本输入 / 可键盘激活的控件下不要抢 Space（否则按钮、下拉框按不动）
        const bool ownKeys = qobject_cast<QLineEdit*>(focus)
                          || qobject_cast<QTextEdit*>(focus)
                          || qobject_cast<QPlainTextEdit*>(focus)
                          || qobject_cast<QAbstractSpinBox*>(focus)
                          || qobject_cast<QAbstractButton*>(focus)
                          || qobject_cast<QAbstractSlider*>(focus)
                          || qobject_cast<QComboBox*>(focus);

        if (!ownKeys && ke->modifiers() == Qt::NoModifier) {
            MediaPanel *target = panelUnderCursor();
            if (!target) target = activePanel();

            if (ke->key() == Qt::Key_Space) {
                if (target) {
                    target->isPlaying() ? target->pause() : target->play();
                    return true;
                }
                if (m_audioPanel) {
                    m_audioPanel->isPlaying() ? m_audioPanel->pause() : m_audioPanel->play();
                    return true;
                }
            }
            if (target && target->duration() > 0 && ke->key() == Qt::Key_Left) {
                target->seek(qMax<qint64>(0, target->position() - 5000));
                return true;
            }
            if (target && target->duration() > 0 && ke->key() == Qt::Key_Right) {
                target->seek(qMin(target->duration(), target->position() + 5000));
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    // ---- 拖放：只接管「本地文件」，其余放行 ----
    // 原来是全局无条件 return true，连文本编辑器拖入文字、文件对话框内拖拽都被吞掉
    if (t == QEvent::DragEnter || t == QEvent::DragMove || t == QEvent::Drop) {
        QWidget *w = qobject_cast<QWidget*>(watched);
        if (!w) return QMainWindow::eventFilter(watched, event);

        const bool inMainArea = (w == this || w == m_container
                                 || (m_container && m_container->isAncestorOf(w)));
        if (!inMainArea) return QMainWindow::eventFilter(watched, event);

        // 文本编辑控件自己处理自己的拖放（拖入纯文本）
        if (qobject_cast<QPlainTextEdit*>(w) || qobject_cast<QTextEdit*>(w) || qobject_cast<QLineEdit*>(w))
            return QMainWindow::eventFilter(watched, event);

        if (t == QEvent::Drop) {
            auto *de = static_cast<QDropEvent*>(event);
            if (de->mimeData() && de->mimeData()->hasUrls()) {
                dropEvent(de);
                return true;
            }
            return QMainWindow::eventFilter(watched, event);
        }

        auto *de = static_cast<QDragMoveEvent*>(event);
        if (!de->mimeData() || !de->mimeData()->hasUrls())
            return QMainWindow::eventFilter(watched, event);

        bool local = false;
        for (const QUrl &url : de->mimeData()->urls()) {
            if (url.isLocalFile()) { local = true; break; }
        }
        if (!local) return QMainWindow::eventFilter(watched, event);

        if (t == QEvent::DragEnter) dragEnterEvent(static_cast<QDragEnterEvent*>(event));
        de->acceptProposedAction();
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                event->acceptProposedAction();
                statusBar()->showMessage(QString("松开添加 %1 个文件...").arg(event->mimeData()->urls().size()));
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }

    for (const QString &path : paths) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            QDir d(path);
            QStringList allFiles = d.entryList(QDir::Files);
            for (const QString &f : allFiles) {
                if (classify(d.absoluteFilePath(f)) == Other) continue;
                if (m_panels.size() >= m_maxPanels) break;
                addPanel(d.absoluteFilePath(f));
            }
        } else {
            if (m_panels.size() >= m_maxPanels && classify(path) != Audio) continue;
            addPanel(path);
        }
    }
    event->acceptProposedAction();
}

void MainWindow::showHelp()
{
    QString text =
        "<h2 style='text-align:center;'>全能媒体播放器 - 使用说明</h2>"
        "<hr>"
        "<h3>&#128196; 支持的文件类型</h3>"
        "<ul>"
        "<li><b>视频</b>: MP4, AVI, MKV, MOV, FLV, WMV, WebM, M4V, MPG, TS, 3GP, RMVB 等</li>"
        "<li><b>音频</b>: MP3, WAV, FLAC, AAC, OGG, WMA, M4A</li>"
        "<li><b>图片</b>: JPG, PNG, GIF, BMP, WebP, TIFF, ICO</li>"
        "<li><b>文档</b>: PDF, TXT, MD, CSV, JSON, XML, HTML, CSS, JS, Python, Java, C/C++ 源码等</li>"
        "</ul>"
        "<h3>&#128221; 基本操作</h3>"
        "<ul>"
        "<li><b>添加文件</b>: 菜单 [文件 &rarr; 添加文件] 或从资源管理器直接拖入窗口</li>"
        "<li><b>添加文件夹</b>: 菜单 [文件 &rarr; 添加文件夹]，自动加载文件夹内所有支持的文件</li>"
        "<li><b>最近打开</b>: 菜单 [文件 &rarr; 最近打开]，快速加载之前播放过的文件（最多 20 条）</li>"
        "<li><b>清除历史</b>: 菜单 [文件 &rarr; 清除历史]，一键清空历史记录</li>"
        "<li><b>关闭面板</b>: 点击面板右上角的 &#10005; 按钮</li>"
        "<li><b>调整面板大小</b>: 拖动面板之间的分割线</li>"
        "<li><b>放大面板</b>: 点击面板上的 &#9744; 按钮，循环切换 原始 &rarr; 1/4 &rarr; 1/3 &rarr; 1/2 宽度</li>"
        "<li><b>批量控制</b>: 菜单 [播放] 中可全部播放 / 暂停 / 停止</li>"
        "<li><b>循环播放</b>: 菜单 [播放 &rarr; 循环播放] 勾选</li>"
        "<li><b>全部静音</b>: 菜单 [播放 &rarr; 全部静音] 勾选</li>"
        "</ul>"
        "<h3>&#8987; 快捷键</h3>"
        "<table border='1' cellpadding='6' cellspacing='0' style='border-collapse:collapse;'>"
        "<tr><th>快捷键</th><th>功能</th></tr>"
        "<tr><td><b>Ctrl + O</b></td><td>添加文件</td></tr>"
        "<tr><td><b>Ctrl + Shift + O</b></td><td>添加文件夹</td></tr>"
        "<tr><td><b>Space</b></td><td>播放 / 暂停鼠标所在面板</td></tr>"
        "<tr><td><b>&larr; / &rarr;</b></td><td>后退 / 前进 5 秒</td></tr>"
        "<tr><td><b>Ctrl + 1 ~ 9</b></td><td>切换鼠标所在面板的放大比例</td></tr>"
        "<tr><td><b>Ctrl + 0</b></td><td>还原全部面板尺寸</td></tr>"
        "<tr><td><b>Ctrl + W</b></td><td>关闭鼠标所在面板</td></tr>"
        "<tr><td><b>Ctrl + L</b></td><td>显示 / 隐藏播放列表</td></tr>"
        "<tr><td><b>Ctrl + M</b></td><td>显示 / 隐藏音频悬浮窗</td></tr>"
        "<tr><td><b>Ctrl + S</b></td><td>文本面板保存（Ctrl+Shift+S 另存为）</td></tr>"
        "<tr><td><b>F11 / Esc</b></td><td>全屏 / 退出全屏</td></tr>"
        "<tr><td><b>F1</b></td><td>打开本帮助</td></tr>"
        "<tr><td><b>Ctrl + Q</b></td><td>退出程序</td></tr>"
        "</table>"
        "<h3>&#128161; 小贴士</h3>"
        "<ul>"
        "<li>最多同时加载 12 个媒体面板，支持视频/音频/图片/PDF/文本混合播放</li>"
        "<li>关闭面板时会自动保存播放进度，下次打开同一文件时恢复</li>"
        "<li>鼠标移入视频/音频面板时，标题栏和底部控制条自动显示；移出则自动隐藏</li>"
        "<li>图片、PDF、文本面板的控制条始终可见，随时能关闭或放大</li>"
        "<li>图片面板底部有 <b>− 缩小 / 百分比 / + 放大 / 适应窗口 / 原始大小</b>，也支持鼠标滚轮缩放、"
        "按住拖动平移、双击还原到适应窗口；GIF 会播放动画</li>"
        "<li>PDF 面板支持翻页、A+/A- 缩放和「适合宽度 / 整页」切换</li>"
        "<li>Word / PPT 文件在后台提取文本，不会再让界面卡死；保存 docx 需要本机装有 Word 或 WPS</li>"
        "<li>音量条常驻在底部控制栏，可直接拖动；在音量图标上滚动滚轮也能调</li>"
        "<li>同时播放多个视频时，仅第一个有声音，其余自动静音以避免音频混乱；"
        "想让所有视频都出声，用 [播放 → 全部静音] 勾选一次再取消即可</li>"
        "<li>播放菜单里可以把速度调到 0.5x ~ 2.0x，新打开的面板会继承该速度</li>"
        "<li>按 F11 进入全屏获得最大播放区域</li>"
        "<li>支持从资源管理器直接拖拽文件或文件夹到窗口添加</li>"
        "</ul>";

        QDialog dlg(this);
        dlg.setWindowTitle("使用说明");
        dlg.resize(560, 600);

        auto *main = new QVBoxLayout(&dlg);

        auto *scroll = new QScrollArea(&dlg);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *content = new QLabel(scroll);
        content->setText(text);
        content->setTextFormat(Qt::RichText);
        content->setWordWrap(true);
        content->setOpenExternalLinks(true);
        scroll->setWidget(content);
        main->addWidget(scroll, 1);

        auto *btn = new QPushButton("确定", &dlg);
        btn->setDefault(true);
        main->addWidget(btn, 0, Qt::AlignCenter);
        connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);

        dlg.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "关于全能媒体播放器",
        "<h2 style='text-align:center;'>全能媒体播放器</h2>"
        "<p style='text-align:center; font-size:14px;'>版本 1.2.1</p>"
        "<hr>"
        "<p>一个支持同时播放多种媒体的播放器，</p>"
        "<p>支持视频、音频、图片、PDF 和文本文件。</p>"
        "<p><br>本次更新：<br>"
        "&nbsp;&nbsp;&bull; 音量条改为控制栏内联滑条，悬停/拖动都能用（原浮动面板被视频原生窗口遮挡）<br>"
        "&nbsp;&nbsp;&bull; 修复「取消全部静音」救不回被自动静音的视频<br>"
        "&nbsp;&nbsp;&bull; 修复图片 / PDF / 文本面板控制条永久隐藏、无法关闭的问题<br>"
        "&nbsp;&nbsp;&bull; 重做自动隐藏：播放中静止 2.5 秒才收起，暂停常驻<br>"
        "&nbsp;&nbsp;&bull; 修复音频面板播完不循环、不跟随主窗口、静音图标错乱<br>"
        "&nbsp;&nbsp;&bull; 修复拖动进度条时滑块回弹、视频循环失效<br>"
        "&nbsp;&nbsp;&bull; 实装 Ctrl+O / Ctrl+Shift+O / Space / ←→ / Ctrl+1~9 / Ctrl+W 等快捷键<br>"
        "&nbsp;&nbsp;&bull; 图片面板支持滚轮缩放、拖拽平移、GIF 动画<br>"
        "&nbsp;&nbsp;&bull; Word / PPT 文本改为后台异步提取，不再卡死界面<br>"
        "&nbsp;&nbsp;&bull; 新增 0.5x ~ 2.0x 倍速播放<br>"
        "&nbsp;&nbsp;&bull; 修复窗口位置/大小、音频面板、循环与倍速设置的持久化</p>"
        "<p style='color:#888; font-size:12px; margin-top:20px;'>基于 Qt 6.7 开发</p>");
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_geometryClamped) return;
    m_geometryClamped = true;

    // restoreGeometry 不会校正越界：换显示器 / 改分辨率 / 任务栏变化后，
    // 上一次保存的几何可能让窗口底部藏到任务栏后面（状态栏就看不见了）
    if (m_isFullScreen || isMaximized()) return;
    QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!scr) return;

    const QRect avail = scr->availableGeometry();

    // 注意：要按「外框」而不是客户区来限制，否则标题栏会顶到屏幕外、
    // 客户区底部会压到任务栏底下（状态栏正好被挡住）
    QRect frame = frameGeometry();
    const int dw = qMax(0, frame.width()  - width());
    const int dh = qMax(0, frame.height() - height());

    QSize target = size();
    target.setWidth(qMin(target.width(),  qMax(200, avail.width()  - dw)));
    target.setHeight(qMin(target.height(), qMax(200, avail.height() - dh)));
    if (target != size()) resize(target);

    frame = frameGeometry();
    const int dx = frame.left() - geometry().left();
    const int dy = frame.top()  - geometry().top();

    QPoint p = frame.topLeft();
    const int maxX = avail.right()  - frame.width()  + 1;
    const int maxY = avail.bottom() - frame.height() + 1;
    p.setX(qBound(avail.left(), p.x(), qMax(avail.left(), maxX)));
    p.setY(qBound(avail.top(),  p.y(), qMax(avail.top(),  maxY)));
    if (p != frame.topLeft()) move(p.x() - dx, p.y() - dy);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 退出前先处理未保存的文本面板（原来直接关，改过的内容全丢）
    const QVector<MediaPanel*> snapshot = m_panels;
    for (MediaPanel *p : snapshot) {
        auto *tp = qobject_cast<TextPanel*>(p);
        if (!tp || !tp->hasUnsavedChanges()) continue;
        tp->requestClose();
        if (m_panels.contains(p)) {   // 用户点了「取消」
            event->ignore();
            return;
        }
    }

    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.beginGroup("window");
    s.setValue("geometry", saveGeometry());
    s.setValue("state", saveState());
    s.setValue("maximized", isMaximized());
    s.setValue("fullScreen", m_isFullScreen);
    s.endGroup();

    s.beginGroup("preferences");
    s.setValue("loop", m_loop);
    s.setValue("rate", m_rate);
    s.endGroup();

    QStringList paths;
    for (auto *p : m_panels) {
        paths.append(p->path());
        savePanelProgress(p);
    }
    s.setValue("files/lastFiles", paths);

    if (!paths.isEmpty()) {
        QStringList recent = s.value("recentFiles").toStringList();
        for (const QString &path : paths) {
            QString absPath = QFileInfo(path).absoluteFilePath();
            recent.removeAll(absPath);
            recent.prepend(absPath);
        }
        while (recent.size() > MAX_RECENT_FILES)
            recent.removeLast();
        s.setValue("recentFiles", recent);
    }

    // 音频面板不占用 m_panels，单独保存路径，否则重开程序音频会彻底丢失
    s.setValue("audio/path", m_audioPath);
    s.setValue("audio/visible", m_audioPanel && m_audioPanel->isVisible());
}

void MainWindow::loadSettings()
{
    QSettings s;

    s.beginGroup("window");
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("state").toByteArray());
    bool maximized = s.value("maximized", false).toBool();
    if (maximized) showMaximized();
    m_isFullScreen = s.value("fullScreen", false).toBool();
    s.endGroup();

    s.beginGroup("preferences");
    m_loop = s.value("loop", true).toBool();
    m_rate = s.value("rate", 1.0).toDouble();
    if (m_rate < 0.1 || m_rate > 8.0) m_rate = 1.0;
    s.endGroup();

    QStringList paths = s.value("files/lastFiles").toStringList();

    if (!paths.isEmpty()) {
        QStringList recent = s.value("recentFiles").toStringList();
        bool changed = false;
        for (const QString &path : paths) {
            QString absPath = QFileInfo(path).absoluteFilePath();
            if (!recent.contains(absPath)) {
                recent.prepend(absPath);
                changed = true;
            }
        }
        if (changed) {
            while (recent.size() > MAX_RECENT_FILES)
                recent.removeLast();
            s.setValue("recentFiles", recent);
        }
    }

    for (const QString &p : paths) {
        QFileInfo fi(p);
        if (fi.exists()) addPanel(p);
    }

    s.beginGroup("audio");
    const QString audioPath = s.value("path").toString();
    const bool audioVisible = s.value("visible", false).toBool();
    s.endGroup();

    if (!audioPath.isEmpty() && QFileInfo::exists(audioPath)) {
        addPanel(audioPath);
        if (m_audioPanel) {
            m_audioPanel->setVisible(audioVisible);
            if (audioVisible) { layoutAudioPanel(); m_audioPanel->raise(); }
        }
    }

    // 让菜单勾选状态和实际设置一致（原来只读了 m_loop，菜单里永远是勾选 1.0x / 循环）
    if (m_loopAction) {
        QSignalBlocker blocker(m_loopAction);
        m_loopAction->setChecked(m_loop);
    }
    if (m_speedGroup) {
        for (QAction *a : m_speedGroup->actions())
            a->setChecked(qFuzzyCompare(a->data().toDouble(), m_rate));
    }
    for (auto *p : m_panels) {
        p->setLoop(m_loop);
        p->setPlaybackRate(m_rate);
    }
    if (m_audioPanel) {
        m_audioPanel->setLoop(m_loop);
        m_audioPanel->setPlaybackRate(m_rate);
    }

    if (m_isFullScreen) showFullScreen();
}

void MainWindow::savePanelProgress(MediaPanel *panel)
{
    if (!panel) return;
    qint64 dur = panel->duration();
    qint64 pos = panel->position();
    if (dur <= 0 || pos <= 0) return;
    if (pos >= dur * 0.95) return;

    QSettings s;
    s.beginGroup("progress");
    s.setValue(QFileInfo(panel->path()).absoluteFilePath(), pos);
    s.endGroup();
}

void MainWindow::restorePanelProgress(MediaPanel *panel)
{
    if (!panel) return;
    QSettings s;
    s.beginGroup("progress");
    qint64 savedPos = s.value(QFileInfo(panel->path()).absoluteFilePath(), 0).toLongLong();
    s.endGroup();
    if (savedPos <= 0) return;

    QTimer::singleShot(1800, panel, [this, panel, savedPos] {
        if (!panel || panel->duration() <= 0) return;
        if (savedPos <= panel->duration() * 0.9) {
            panel->seek(savedPos);
            panel->pause();
            statusBar()->showMessage(QString("已恢复进度: %1").arg(MediaPanel::formatTime(savedPos)), 2000);
        }
    });
}

void MainWindow::addToRecentFiles(const QString &path)
{
    QSettings s;
    QStringList recent = s.value("recentFiles").toStringList();

    QString absPath = QFileInfo(path).absoluteFilePath();
    recent.removeAll(absPath);
    recent.prepend(absPath);

    while (recent.size() > MAX_RECENT_FILES)
        recent.removeLast();

    s.setValue("recentFiles", recent);
    s.sync();
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();

    QSettings s;
    QStringList recent = s.value("recentFiles").toStringList();

    if (recent.isEmpty()) {
        QAction *emptyAct = m_recentMenu->addAction("（暂无）");
        emptyAct->setEnabled(false);
        return;
    }

    QList<QString> missing;
    for (const QString &path : recent) {
        QFileInfo fi(path);
        QAction *act = m_recentMenu->addAction(fi.fileName());
        act->setToolTip(path);
        act->setData(path);
        connect(act, &QAction::triggered, this, &MainWindow::openRecentFile);

        if (!fi.exists())
            missing.append(path);
    }

    if (!missing.isEmpty()) {
        for (const QString &p : missing)
            recent.removeAll(p);
        QSettings s2;
        s2.setValue("recentFiles", recent);
    }
}

void MainWindow::openRecentFile()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (!act) return;
    QString path = act->data().toString();
    if (path.isEmpty()) return;

    QFileInfo fi(path);
    if (!fi.exists()) {
        statusBar()->showMessage(QString("文件不存在: %1").arg(path), 3000);
        QSettings s;
        QStringList recent = s.value("recentFiles").toStringList();
        recent.removeAll(path);
        s.setValue("recentFiles", recent);
        updateRecentFilesMenu();
        return;
    }

    addPanel(path);
}

void MainWindow::clearRecentFiles()
{
    QSettings s;
    s.remove("recentFiles");
    updateRecentFilesMenu();
    refreshPlaylist();
    statusBar()->showMessage("历史记录已清空", 2000);
}

void MainWindow::buildPlaylist()
{
    m_playlistPanel = new QWidget(m_innerSplitter);
    m_playlistPanel->setStyleSheet("background: #16162a; border-left: none;");
    auto *lay = new QVBoxLayout(m_playlistPanel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *header = new QLabel("播放列表", m_playlistPanel);
    header->setStyleSheet("color: #cfcfe0; font-size: 13px; font-weight: bold; padding: 8px 10px; background: #1c1c32; border-bottom: 1px solid #2a2a44;");
    header->setFixedHeight(34);
    lay->addWidget(header);

    m_playlistTabs = new QTabWidget(m_playlistPanel);
    m_playlistTabs->setStyleSheet(
        "QTabWidget::pane { border: none; background: #16162a; }"
        "QTabBar::tab { background: #1c1c32; color: #8888aa; padding: 6px 8px; margin-right: 2px; font-size: 12px; }"
        "QTabBar::tab:selected { background: #252545; color: #ffffff; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:hover { color: #ccccff; }"
    );
    lay->addWidget(m_playlistTabs, 1);

    auto makeList = [this](const QString &emptyHint) -> QListWidget* {
        auto *lw = new QListWidget();
        lw->setStyleSheet(
            "QListWidget { background: #16162a; color: #e0e0f0; border: none; outline: none; font-size: 12px; }"
            "QListWidget::item { padding: 5px 10px; border-bottom: 1px solid #1e1e36; }"
            "QListWidget::item:hover { background: #1f1f38; color: #ffffff; }"
            "QListWidget::item:selected { background: #3b3b6a; color: #ffffff; }"
        );
        lw->setIconSize(QSize(16, 16));
        lw->setUniformItemSizes(true);
        lw->setSelectionMode(QAbstractItemView::SingleSelection);
        lw->setToolTip(emptyHint);
        connect(lw, &QListWidget::itemActivated, this, &MainWindow::onPlaylistItemActivated);
        connect(lw, &QListWidget::itemClicked, this, [this](QListWidgetItem *) {
            m_playlistTabs->currentWidget();
        });
        return lw;
    };

    m_videoList = makeList("双击加载视频");
    m_audioList = makeList("双击加载音频");
    m_imageList = makeList("双击加载图片");
    m_docList   = makeList("双击加载文档");

    m_playlistTabs->addTab(m_videoList, "视频");
    m_playlistTabs->addTab(m_audioList, "音频");
    m_playlistTabs->addTab(m_imageList, "图片");
    m_playlistTabs->addTab(m_docList,   "文档");
}

QIcon MainWindow::drawPlaylistIcon(MediaType t)
{
    const int S = 22;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg("#e94560");
    QColor fg("#ffffff");

    int pad = 3;
    QRect iconRect(pad, pad, S - pad * 2, S - pad * 2);

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(iconRect, 4, 4);

    p.setBrush(fg);
    p.setPen(Qt::NoPen);

    switch (t) {
    case Video: {
        QPoint tri[3] = {
            QPoint(8, 6),
            QPoint(8, S - 6),
            QPoint(S - 6, S / 2)
        };
        p.drawPolygon(tri, 3);
        break;
    }
    case Audio: {
        p.drawEllipse(QPointF(S * 0.62, S * 0.75), 2.8, 2.8);
        p.drawEllipse(QPointF(S * 0.38, S * 0.82), 2.8, 2.8);
        QPainterPath path;
        path.moveTo(S * 0.44, S * 0.75);
        path.lineTo(S * 0.44, S * 0.28);
        path.cubicTo(S * 0.58, S * 0.22, S * 0.75, S * 0.28, S * 0.58, S * 0.38);
        path.lineTo(S * 0.58, S * 0.78);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Image: {
        QPainterPath path;
        path.moveTo(4, S - 5);
        path.lineTo(9, 11);
        path.lineTo(13, 15);
        path.lineTo(17, 8);
        path.lineTo(S - 4, S - 5);
        path.closeSubpath();
        p.drawPath(path);
        p.setBrush(QColor("#e94560"));
        p.drawEllipse(QPointF(S - 7, 7), 2.2, 2.2);
        break;
    }
    case Pdf: {
        QRectF r(5, 4, S - 10, S - 8);
        p.drawRoundedRect(r, 2, 2);
        p.setBrush(QColor("#e94560"));
        p.drawRect(r.adjusted(0, 0, -5, 0));
        p.setBrush(fg);
        QFont f; f.setBold(true); f.setPointSize(5.5);
        p.setFont(f);
        p.drawText(r.adjusted(1, 1, -1, -1), Qt::AlignCenter, "P");
        break;
    }
    case Text: {
        QRectF r(5, 4, S - 10, S - 8);
        p.drawRoundedRect(r, 2, 2);
        p.setPen(fg);
        p.setBrush(Qt::NoBrush);
        int ly = 9;
        for (int i = 0; i < 4; ++i) {
            if (i == 3) p.drawLine(QPointF(9, ly), QPointF(15, ly));
            else p.drawLine(QPointF(8, ly), QPointF(S - 8, ly));
            ly += 3.2;
        }
        break;
    }
    default: break;
    }

    return QIcon(pm);
}

void MainWindow::refreshPlaylist()
{
    if (!m_videoList) return;

    QList<QListWidget*> lists = { m_videoList, m_audioList, m_imageList, m_docList };
    for (auto *lw : lists) lw->clear();

    QSettings s;
    QStringList recent = s.value("recentFiles").toStringList();
    QSet<QString> seen;

    for (const QString &path : recent) {
        QString absPath = QFileInfo(path).absoluteFilePath();
        if (!QFile::exists(absPath)) continue;
        if (seen.contains(absPath)) continue;
        seen.insert(absPath);

        MediaType t = classify(absPath);
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, absPath);
        item->setText(QFileInfo(absPath).fileName());
        item->setToolTip(absPath);

        QIcon ic;
        switch (t) {
        case Video:
            ic = drawPlaylistIcon(Video);
            m_videoList->addItem(item);
            break;
        case Audio:
            ic = drawPlaylistIcon(Audio);
            m_audioList->addItem(item);
            break;
        case Image:
            ic = drawPlaylistIcon(Image);
            m_imageList->addItem(item);
            break;
        case Pdf:
        case Text:
            ic = drawPlaylistIcon(t == Pdf ? Pdf : Text);
            m_docList->addItem(item);
            break;
        default:
            delete item;
            continue;
        }
        item->setIcon(ic);
    }

    auto updateTabLabel = [](QTabWidget *tw, int idx, QListWidget *lw, const QString &base) {
        tw->setTabText(idx, QString("%1 (%2)").arg(base).arg(lw->count()));
    };
    updateTabLabel(m_playlistTabs, 0, m_videoList, "视频");
    updateTabLabel(m_playlistTabs, 1, m_audioList, "音频");
    updateTabLabel(m_playlistTabs, 2, m_imageList, "图片");
    updateTabLabel(m_playlistTabs, 3, m_docList,   "文档");
}

void MainWindow::onPlaylistItemActivated(QListWidgetItem *item)
{
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (!QFile::exists(path)) {
        statusBar()->showMessage(QString("文件不存在: %1").arg(QFileInfo(path).fileName()), 3000);
        return;
    }
    addPanel(path);
}

void MainWindow::togglePlaylist()
{
    m_playlistVisible = !m_playlistVisible;
    if (!m_innerSplitter || !m_playlistPanel) return;

    if (m_playlistVisible) {
        m_playlistPanel->show();
        int total = m_innerSplitter->width();
        int playlistW = qMin(260, total / 4);
        m_innerSplitter->setSizes(QList<int>() << (total - playlistW) << playlistW);
    } else {
        m_innerSplitter->setSizes(QList<int>() << m_innerSplitter->width() << 0);
        m_playlistPanel->hide();
    }
}