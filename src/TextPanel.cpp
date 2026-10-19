#include "TextPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QTextStream>
#include <QStringDecoder>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QProcess>
#include <QTimer>
#include <QUuid>
#include <QDateTime>

static const qint64 kBigFileWarnBytes = 8LL * 1024 * 1024;   // 8MB
static const qint64 kHugeFileBytes    = 64LL * 1024 * 1024;  // 64MB 直接拒绝

static bool isWordFormat(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(".doc") || lower.endsWith(".docx") || lower.endsWith(".rtf");
}

static bool isPptFormat(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(".ppt") || lower.endsWith(".pptx")
        || lower.endsWith(".pptm") || lower.endsWith(".ppsm");
}

static QString psQuote(const QString &s)
{
    // PowerShell 单引号字符串：内部单引号需要写成两个
    QString out = s;
    out.replace('\'', "''");
    return out;
}

static QString writePs1(const QString &scriptBody, const QString &ps1Path)
{
    QFile f(ps1Path);
    if (!f.open(QIODevice::WriteOnly)) return QString();
    f.write("\xEF\xBB\xBF");                    // BOM，避免 PowerShell 按 ANSI 解读中文
    f.write(scriptBody.toUtf8());
    f.close();
    return ps1Path;
}

// ---------------------------------------------------------------- 脚本模板

static QString buildWordReadScript(const QString &srcPath, const QString &outPath)
{
    return QStringLiteral(
        "$word = $null; $doc = $null\n"
        "foreach ($name in @('Word.Application','Kwps.Application')) {\n"
        "  try { $word = New-Object -ComObject $name; break } catch { $word = $null }\n"
        "}\n"
        "if (-not $word) { Write-Host 'NO_COM'; exit 2 }\n"
        "$text = ''\n"
        "try {\n"
        "  $word.Visible = $false\n"
        "  $word.DisplayAlerts = 0\n"
        "  $doc = $word.Documents.Open('%1', $false, $true)\n"
        "  $text = $doc.Content.Text\n"
        "} catch { Write-Host \"OPEN_ERROR: $($_.Exception.Message)\" }\n"
        "if ($text) {\n"
        "  [System.IO.File]::WriteAllText('%2', $text, [System.Text.Encoding]::UTF8)\n"
        "  Write-Host 'WRITE_OK'\n"
        "} else { Write-Host 'NO_TEXT' }\n"
        "try { if ($doc) { $doc.Close($false) } } catch {}\n"
        "try { if ($word) { $word.Quit() } } catch {}\n"
        "try { if ($word) { [System.Runtime.Interopservices.Marshal]::ReleaseComObject($word) | Out-Null } } catch {}\n"
        "exit 0\n"
    ).arg(psQuote(QDir::toNativeSeparators(srcPath)),
          psQuote(QDir::toNativeSeparators(outPath)));
}

static QString buildPptReadScript(const QString &srcPath, const QString &outPath)
{
    return QStringLiteral(
        "$ppt = $null; $pres = $null\n"
        "foreach ($name in @('PowerPoint.Application','Kwpp.Application')) {\n"
        "  try { $ppt = New-Object -ComObject $name; break } catch { $ppt = $null }\n"
        "}\n"
        "if (-not $ppt) { Write-Host 'NO_COM'; exit 2 }\n"
        "$sb = New-Object System.Text.StringBuilder\n"
        "try {\n"
        "  $ppt.Visible = -1\n"
        "  $ppt.DisplayAlerts = 0\n"
        "  $pres = $ppt.Presentations.Open('%1', $false, $false, $false)\n"
        "  $idx = 1\n"
        "  foreach ($slide in $pres.Slides) {\n"
        "    [void]$sb.AppendLine(\"===== Slide $idx =====\")\n"
        "    foreach ($shape in $slide.Shapes) {\n"
        "      try {\n"
        "        if ($shape.HasTextFrame -and $shape.TextFrame.HasText) {\n"
        "          [void]$sb.AppendLine($shape.TextFrame.TextRange.Text)\n"
        "        }\n"
        "      } catch {}\n"
        "    }\n"
        "    [void]$sb.AppendLine()\n"
        "    $idx++\n"
        "  }\n"
        "} catch { Write-Host \"OPEN_ERROR: $($_.Exception.Message)\" }\n"
        "$text = $sb.ToString()\n"
        "if ($text) {\n"
        "  [System.IO.File]::WriteAllText('%2', $text, [System.Text.Encoding]::UTF8)\n"
        "  Write-Host 'WRITE_OK'\n"
        "} else { Write-Host 'NO_TEXT' }\n"
        "try { if ($pres) { $pres.Close() } } catch {}\n"
        "try { if ($ppt) { $ppt.Quit() } } catch {}\n"
        "try { if ($ppt) { [System.Runtime.Interopservices.Marshal]::ReleaseComObject($ppt) | Out-Null } } catch {}\n"
        "exit 0\n"
    ).arg(psQuote(QDir::toNativeSeparators(srcPath)),
          psQuote(QDir::toNativeSeparators(outPath)));
}

static QString buildWordSaveScript(const QString &dstPath, const QString &inPath, int fmt)
{
    return QStringLiteral(
        "$word = $null; $doc = $null\n"
        "foreach ($name in @('Word.Application','Kwps.Application')) {\n"
        "  try { $word = New-Object -ComObject $name; break } catch { $word = $null }\n"
        "}\n"
        "if (-not $word) { Write-Host 'NO_COM'; exit 2 }\n"
        "$ok = $false\n"
        "try {\n"
        "  $word.Visible = $false\n"
        "  $word.DisplayAlerts = 0\n"
        "  $doc = $word.Documents.Add()\n"
        "  $content = [System.IO.File]::ReadAllText('%1', [System.Text.Encoding]::UTF8)\n"
        "  $doc.Content.Text = $content\n"
        "  $doc.SaveAs2('%2', %3)\n"
        "  $ok = $true\n"
        "  Write-Host 'SAVE_OK'\n"
        "} catch { Write-Host \"SAVE_ERROR: $($_.Exception.Message)\" }\n"
        "try { if ($doc) { $doc.Close($false) } } catch {}\n"
        "try { if ($word) { $word.Quit() } } catch {}\n"
        "try { if ($word) { [System.Runtime.Interopservices.Marshal]::ReleaseComObject($word) | Out-Null } } catch {}\n"
        "if ($ok) { exit 0 } else { exit 1 }\n"
    ).arg(psQuote(QDir::toNativeSeparators(inPath)),
          psQuote(QDir::toNativeSeparators(dstPath)))
     .arg(fmt);
}

// Word 保存仍为同步调用（用户主动触发、通常 1~2 秒），但不再无脑等 45 秒
static bool runPs1Sync(const QString &ps1Path, int timeoutMs = 20000)
{
    QProcess proc;
    proc.start("powershell.exe", {
        "-NoProfile", "-NonInteractive",
        "-ExecutionPolicy", "Bypass",
        "-File", ps1Path
    });
    if (!proc.waitForStarted(timeoutMs)) return false;
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        return false;
    }
    return proc.exitCode() == 0;
}

static bool saveDocxViaWord(const QString &path, const QString &text)
{
    const QString tmpIn  = QDir::tempPath() + "/mmplayer_write_" + QUuid::createUuid().toString(QUuid::Id128) + ".txt";
    const QString ps1    = QDir::tempPath() + "/mmplayer_save_"  + QUuid::createUuid().toString(QUuid::Id128) + ".ps1";

    {
        QFile f(tmpIn);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << text;
        f.close();
    }

    const QString lower = path.toLower();
    int fmt = 16;                              // wdFormatDocumentDefault (docx)
    if (lower.endsWith(".doc"))      fmt = 0;  // wdFormatDocument
    else if (lower.endsWith(".rtf")) fmt = 6;  // wdFormatRTF

    bool ok = false;
    if (!writePs1(buildWordSaveScript(path, tmpIn, fmt), ps1).isEmpty()) {
        ok = runPs1Sync(ps1);
        QFile::remove(ps1);
    }
    QFile::remove(tmpIn);
    return ok;
}

// ---------------------------------------------------------------- TextPanel

TextPanel::TextPanel(const QString &path, QWidget *parent)
    : MediaPanel(Text, path, parent), m_currentPath(path)
{
    m_textEdit = new QPlainTextEdit(m_mediaArea);

    QVBoxLayout *lay = new QVBoxLayout(m_mediaArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_textEdit);

    // 文本面板没有播放/进度/音量概念，底部控制栏整条移除以腾出编辑区
    removeControlBar();

    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &TextPanel::onTextChanged);

    QObject::disconnect(m_closeBtn, &QPushButton::clicked, this, nullptr);
    connect(m_closeBtn, &QPushButton::clicked, this, &TextPanel::onCloseRequested);

    auto *saveShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), m_textEdit);
    saveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveShortcut, &QShortcut::activated, this, &TextPanel::onSave);

    auto *saveAsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), m_textEdit);
    saveAsShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveAsShortcut, &QShortcut::activated, this, &TextPanel::onSaveAs);

    // 主题按钮插入到放大按钮左边
    QWidget *topBar = m_titleLabel ? m_titleLabel->parentWidget() : nullptr;
    auto *topLay = topBar ? qobject_cast<QHBoxLayout*>(topBar->layout()) : nullptr;
    int idx = topLay ? topLay->indexOf(m_amplifyBtn) : -1;

    m_themeBtn = new QPushButton("\xe2\x98\x91", topBar);
    m_themeBtn->setFixedSize(24, 24);
    m_themeBtn->setToolTip(QString::fromUtf8("切换暗黑/明亮主题"));
    m_themeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #e94560; border: none; font-size: 13px; padding: 0; }"
        "QPushButton:hover { background: #0f3460; border-radius: 3px; }"
    );
    if (topLay && idx >= 0) topLay->insertWidget(idx, m_themeBtn);

    applyTheme(true);
    connect(m_themeBtn, &QPushButton::clicked, this, [this]() { applyTheme(!m_darkMode); });

    // 界面先画出来，再异步读内容，避免打开 docx 时整个窗口卡死
    startLoad();
}

TextPanel::~TextPanel()
{
    if (m_proc) {
        disconnect(m_proc, nullptr, this, nullptr);
        if (m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
            m_proc->waitForFinished(800);
        }
    }
    cleanupTempFiles();
}

void TextPanel::cleanupTempFiles()
{
    if (!m_tmpOut.isEmpty()) { QFile::remove(m_tmpOut); m_tmpOut.clear(); }
    if (!m_tmpPs1.isEmpty()) { QFile::remove(m_tmpPs1); m_tmpPs1.clear(); }
}

void TextPanel::startLoad()
{
    if (isWordFormat(m_currentPath)) {
        startOfficeExtract(false);
    } else if (isPptFormat(m_currentPath)) {
        startOfficeExtract(true);
    } else {
        loadPlainText();
    }
}

void TextPanel::loadPlainText()
{
    QFile file(m_currentPath);
    if (!file.open(QIODevice::ReadOnly)) {
        applyContent(QString::fromUtf8("(无法打开文件: %1)").arg(m_currentPath));
        m_infoLabel->setText(QString::fromUtf8("打开失败"));
        return;
    }

    const qint64 total = file.size();
    if (total > kHugeFileBytes) {
        file.close();
        applyContent(QString::fromUtf8("(文件过大，%1 MB，已跳过加载)").arg(total / 1024 / 1024));
        m_infoLabel->setText(QString::fromUtf8("文件过大"));
        return;
    }

    const QByteArray raw = file.readAll();
    file.close();

    // 先按 UTF-8 严格解码，失败再退回本地编码（中文 Windows 即 GBK）
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString content = utf8(raw);
    if (utf8.hasError()) {
        QStringDecoder local(QStringDecoder::System);
        content = local(raw);
    }

    applyContent(content);
    if (total > kBigFileWarnBytes)
        m_infoLabel->setText(m_infoLabel->text() + QString::fromUtf8(" · 大文件"));
}

void TextPanel::startOfficeExtract(bool ppt)
{
    const QString tag = QUuid::createUuid().toString(QUuid::Id128);
    m_tmpOut = QDir::tempPath() + "/mmplayer_read_" + tag + ".txt";
    m_tmpPs1 = QDir::tempPath() + "/mmplayer_read_" + tag + ".ps1";

    const QString script = ppt ? buildPptReadScript(m_currentPath, m_tmpOut)
                               : buildWordReadScript(m_currentPath, m_tmpOut);
    if (writePs1(script, m_tmpPs1).isEmpty()) {
        applyContent(QString::fromUtf8("(无法创建临时脚本)"));
        cleanupTempFiles();
        return;
    }

    m_officeLoad = true;
    m_infoLabel->setText(ppt ? QString::fromUtf8("正在提取 PPT 文本...")
                             : QString::fromUtf8("正在提取 Word 文本..."));
    m_textEdit->setReadOnly(true);
    m_textEdit->setPlaceholderText(QString::fromUtf8("正在读取，请稍候…"));

    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TextPanel::onExtractFinished);
    connect(m_proc, &QProcess::errorOccurred, this, &TextPanel::onExtractError);
    m_proc->start("powershell.exe", {
        "-NoProfile", "-NonInteractive",
        "-ExecutionPolicy", "Bypass",
        "-File", m_tmpPs1
    });

    // 60 秒兜底，避免 Word 弹窗等异常把进程挂死
    QTimer::singleShot(60000, this, [this]() {
        if (m_proc && m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
            if (m_officeLoad) {
                m_officeLoad = false;
                m_textEdit->setReadOnly(false);
                applyContent(QString::fromUtf8("(读取超时，请确认已安装 Word/WPS 且文件未被占用)"));
            }
        }
    });
}

void TextPanel::onExtractError(int error)
{
    if (!m_officeLoad) return;
    m_officeLoad = false;
    m_textEdit->setReadOnly(false);
    Q_UNUSED(error);
    applyContent(QString::fromUtf8("(无法启动 PowerShell，提取失败)"));
    cleanupTempFiles();
}

void TextPanel::onExtractFinished(int exitCode, int exitStatus)
{
    Q_UNUSED(exitStatus);
    if (!m_officeLoad) return;
    m_officeLoad = false;
    m_textEdit->setReadOnly(false);
    m_textEdit->setPlaceholderText(QString());

    QString content;
    QFile f(m_tmpOut);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        content = ts.readAll();
        f.close();
    }
    cleanupTempFiles();

    if (content.isEmpty()) {
        content = (exitCode == 2)
            ? QString::fromUtf8("(未检测到 Microsoft Word / WPS，无法提取文档文本)")
            : QString::fromUtf8("(未能从该文档提取到文本内容)");
    }
    applyContent(content);
}

void TextPanel::applyContent(const QString &content)
{
    m_blockTextSignal = true;
    m_textEdit->setPlainText(content);
    m_blockTextSignal = false;

    const int lines = content.isEmpty() ? 0 : content.count('\n') + 1;
    const int chars = content.size();
    if (content.isEmpty()) {
        m_infoLabel->setText(QString::fromUtf8("空文件"));
    } else {
        m_infoLabel->setText(QString("%1 %2 \xc2\xb7 %3 %4")
            .arg(lines).arg(QString::fromUtf8("行"))
            .arg(chars).arg(QString::fromUtf8("字符")));
    }
    setDirty(false);
}

void TextPanel::applyTheme(bool dark)
{
    m_darkMode = dark;
    QString bg = dark ? "#0d1117" : "#ffffff";
    QString fg = dark ? "#e6edf3" : "#1f2328";
    QString sel = dark ? "#264f78" : "#b6d4fe";

    QString ss = QString(
        "QPlainTextEdit {"
        "background-color: %1;"
        "color: %2;"
        "selection-background-color: %3;"
        "selection-color: %4;"
        "font-family: Consolas, 'Microsoft YaHei';"
        "font-size: 12px;"
        "border: none;"
        "padding: 8px;"
        "}"
    ).arg(bg, fg, sel, dark ? "#ffffff" : "#000000");

    m_textEdit->setStyleSheet(ss);
    m_textEdit->viewport()->setStyleSheet(QString("background-color: %1;").arg(bg));

    if (m_mediaArea) {
        m_mediaArea->setStyleSheet(QString("background-color: %1;").arg(bg));
    }

    if (m_themeBtn) {
        m_themeBtn->setText(dark ? "\xe2\x98\x91" : "\xe2\x98\x9e");
        m_themeBtn->setToolTip(dark ? QString::fromUtf8("切换为明亮模式")
                                    : QString::fromUtf8("切换为暗黑模式"));
    }
}

void TextPanel::onTextChanged()
{
    if (m_blockTextSignal) return;
    setDirty(true);
    int lines = m_textEdit->document()->blockCount();
    int chars = m_textEdit->toPlainText().size();
    m_infoLabel->setText(QString("%1 %2 \xc2\xb7 %3 %4")
        .arg(lines).arg(QString::fromUtf8("行"))
        .arg(chars).arg(QString::fromUtf8("字符")));
}

void TextPanel::setDirty(bool dirty)
{
    if (m_dirty == dirty) return;
    m_dirty = dirty;
    updateTitle();
}

void TextPanel::updateTitle()
{
    QString prefix = m_dirty ? QString::fromUtf8("\xf0\x9f\x93\x9d\xe2\x9c\x95 ") : QString::fromUtf8("\xf0\x9f\x93\x9d ");
    m_titleLabel->setText(prefix + QFileInfo(m_currentPath).fileName());
}

void TextPanel::saveToFile(const QString &path)
{
    QString content = m_textEdit->toPlainText();

    if (isWordFormat(path)) {
        m_infoLabel->setText(QString::fromUtf8("正在通过 Word 保存..."));
        QCoreApplication::processEvents();
        if (!saveDocxViaWord(path, content)) {
            m_infoLabel->setText(QString::fromUtf8("保存失败"));
            QMessageBox::warning(this, QString::fromUtf8("保存失败"),
                QString::fromUtf8("无法通过 Word 保存，请确保已安装 Microsoft Word 或 WPS。"));
            return;
        }
    } else {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QString::fromUtf8("保存失败"),
                QString::fromUtf8("无法写入文件:\n%1").arg(path));
            return;
        }
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << content;
        f.close();
    }

    m_currentPath = path;
    setDirty(false);
    m_infoLabel->setText(QString::fromUtf8("已保存 \xc2\xb7 ") + QFileInfo(path).fileName());
}

void TextPanel::onSave()
{
    if (m_currentPath.isEmpty()) { onSaveAs(); return; }
    saveToFile(m_currentPath);
}

void TextPanel::onSaveAs()
{
    QString filter = QString::fromUtf8(
        "Word 文档 (*.docx);;Word 97-2003 (*.doc);;富文本 (*.rtf);;"
        "文本文件 (*.txt);;Markdown (*.md);;所有文件 (*)"
    );
    QString dir = m_currentPath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentPath).absolutePath();
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("另存为"), dir, filter);
    if (path.isEmpty()) return;

    QString lower = path.toLower();
    if (!lower.endsWith(".docx") && !lower.endsWith(".doc") && !lower.endsWith(".rtf")
        && !lower.endsWith(".txt") && !lower.endsWith(".md")) {
        path += ".txt";
    }
    saveToFile(path);
}

void TextPanel::requestClose()
{
    onCloseRequested();
}

void TextPanel::onCloseRequested()
{
    if (m_dirty) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(QString::fromUtf8("未保存的修改"));
        box.setText(QString::fromUtf8("文件已修改但尚未保存，是否保存？"));
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setButtonText(QMessageBox::Save,    QString::fromUtf8("保存"));
        box.setButtonText(QMessageBox::Discard, QString::fromUtf8("不保存"));
        box.setButtonText(QMessageBox::Cancel,  QString::fromUtf8("取消"));
        box.setDefaultButton(QMessageBox::Save);
        auto ret = box.exec();
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Save) {
            onSave();
            if (m_dirty) return;
        }
    }
    emit closed(this);
}
