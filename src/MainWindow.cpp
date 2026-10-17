#include "MainWindow.h"
#include "VideoPanel.h"
#include "AudioPanel.h"
#include "ImagePanel.h"
#include "PdfPanel.h"
#include "TextPanel.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
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
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include <QAbstractItemView>
#include <QStyle>
#include <QIcon>
#include <QApplication>
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
    fileMenu->addAction("添加文件...", this, &MainWindow::addFiles, QKeySequence::Open);
    fileMenu->addAction("添加文件夹...", this, &MainWindow::addFolder);
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu("最近打开(&R)");
    fileMenu->addAction("清除历史", this, &MainWindow::clearRecentFiles);
    fileMenu->addSeparator();
    fileMenu->addAction("全部清除", this, &MainWindow::clearAll);
    fileMenu->addSeparator();
    fileMenu->addAction("退出", this, &QWidget::close, QKeySequence::Quit);

    QMenu *playMenu = menuBar->addMenu("播放(&P)");
    playMenu->addAction("全部播放", this, &MainWindow::playAll);
    playMenu->addAction("全部暂停", this, &MainWindow::pauseAll);
    playMenu->addAction("全部停止", this, &MainWindow::stopAll);
    playMenu->addSeparator();
    auto *loopAction = playMenu->addAction("循环播放");
    loopAction->setCheckable(true);
    loopAction->setChecked(true);
    connect(loopAction, &QAction::toggled, this, [this](bool on) {
        m_loop = on;
        for (auto *pp : m_panels) pp->setLoop(on);
    });
    auto *muteAction = playMenu->addAction("全部静音");
    muteAction->setCheckable(true);
    muteAction->setChecked(false);
    connect(muteAction, &QAction::toggled, this, [this](bool on) {
        for (auto *pp : m_panels) {
            if (pp->type() == MediaPanel::Video || pp->type() == MediaPanel::Audio)
                pp->setVolume(on ? 0 : 70);
        }
        if (m_audioPanel) m_audioPanel->setVolume(on ? 0 : 70);
    });

    QMenu *viewMenu = menuBar->addMenu("视图(&V)");
    viewMenu->addAction("全屏模式", this, &MainWindow::toggleFullScreen, QKeySequence(Qt::Key_F11));

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

    statusBar()->showMessage("就绪 | Click \xe2\x9b\xb6 to resize | Min 1/24");

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
            }
        });
        connect(panel, &MediaPanel::playlistToggleClicked, this, &MainWindow::togglePlaylist);
        m_audioPanel = panel;
        panel->setLoop(m_loop);
        panel->adjustSize();
        QRect cr = m_container->rect();
        int pw = panel->width();
        int ph = panel->height();
        int x = cr.left() + (cr.width() - pw) / 2;
        int y = cr.bottom() - ph - 10;
        panel->move(x, y);
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
    panel->setLoop(m_loop);
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

    if (!m_panels.isEmpty()) {
        relayout();
        qDebug() << "[Close] relayout:" << t.elapsed() << "ms";
    }

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
    for (auto *p : m_panels) p->deleteLater();
    m_panels.clear();
    m_ampLevels.clear();
    if (m_audioPanel) {
        m_audioPanel->close();
        m_audioPanel->deleteLater();
        m_audioPanel = nullptr;
    }
    detachPanels();
}

void MainWindow::playAll()  { for (auto *p : m_panels) if (!p->isPlaying()) p->play(); if (m_audioPanel) m_audioPanel->play(); }
void MainWindow::pauseAll() { for (auto *p : m_panels) if (p->isPlaying())  p->pause(); if (m_audioPanel) m_audioPanel->pause(); }
void MainWindow::stopAll()  { for (auto *p : m_panels) p->stop(); if (m_audioPanel) m_audioPanel->stop(); }

void MainWindow::toggleAudio()
{
    if (m_audioPanel) {
        m_audioPanel->toggleVisible();
    }
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
    QWidget *w = qobject_cast<QWidget*>(watched);
    if (!w) return QMainWindow::eventFilter(watched, event);

    QEvent::Type t = event->type();
    if (t == QEvent::DragEnter) {
        QDragEnterEvent *de = static_cast<QDragEnterEvent*>(event);
        dragEnterEvent(de);
        return true;
    }
    if (t == QEvent::DragMove) {
        QDragMoveEvent *de = static_cast<QDragMoveEvent*>(event);
        if (de->mimeData()->hasUrls()) {
            for (const QUrl &url : de->mimeData()->urls()) {
                if (url.isLocalFile()) { de->acceptProposedAction(); return true; }
            }
        }
        return true;
    }
    if (t == QEvent::Drop) {
        QDropEvent *de = static_cast<QDropEvent*>(event);
        dropEvent(de);
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
        "<li><b>添加文件</b>: 菜单 [文件 &rarr; 添加文件] (Ctrl+O) 或从资源管理器直接拖入窗口</li>"
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
        "<tr><td><b>F11</b></td><td>全屏 / 退出全屏</td></tr>"
        "<tr><td><b>F1</b></td><td>打开本帮助</td></tr>"
        "<tr><td><b>Ctrl + O</b></td><td>添加文件</td></tr>"
        "<tr><td><b>Ctrl + Shift + O</b></td><td>添加文件夹</td></tr>"
        "<tr><td><b>Ctrl + Q</b></td><td>退出程序</td></tr>"
        "<tr><td><b>Space</b></td><td>播放 / 暂停当前面板</td></tr>"
        "<tr><td><b>Ctrl + 1 ~ 9</b></td><td>切换第 N 个面板的放大模式</td></tr>"
        "<tr><td><b>Ctrl + M</b></td><td>切换音频悬浮窗</td></tr>"
        "</table>"
        "<h3>&#128161; 小贴士</h3>"
        "<ul>"
        "<li>最多同时加载 12 个媒体面板</li>"
        "<li>关闭面板时会自动保存播放进度，下次打开同一文件时恢复</li>"
        "<li>鼠标在视频画面上静止 2.5 秒后，顶部标题栏和底部控制条会自动隐藏</li>"
        "<li>按 F11 进入全屏获得最大播放区域</li>"
        "<li>面板自动网格布局，文件越多排列越紧密</li>"
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
        "<p style='text-align:center; font-size:14px;'>版本 1.1.0</p>"
        "<hr>"
        "<p>一个支持同时播放多种媒体的播放器，</p>"
        "<p>支持视频、音频、图片、PDF 和文本文件。</p>"
        "<p><br>本次更新新增：<br>"
        "&nbsp;&nbsp;&bull; 最近打开历史记录<br>"
        "&nbsp;&nbsp;&bull; 大幅降低鼠标移动卡顿<br>"
        "&nbsp;&nbsp;&bull; 修复关闭面板闪退问题<br>"
        "&nbsp;&nbsp;&bull; 高性能图片渲染</p>"
        "<p style='color:#888; font-size:12px; margin-top:20px;'>基于 Qt 6.7 开发</p>");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
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

    if (m_audioPanel) {
        s.setValue("audio/visible", m_audioPanel->isVisible());
    }
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
    if (s.value("visible", false).toBool() && m_audioPanel)
        m_audioPanel->show();
    s.endGroup();
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