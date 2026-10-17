#include "TextPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QProcess>
#include <QDateTime>

static void debugLog(const QString &msg)
{
    QString logPath = QDir::tempPath() + "/mmplayer_debug.log";
    QFile log(logPath);
    if (log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ls(&log);
        ls << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << " | " << msg << "\n";
        log.close();
    }
}

static bool isWordFormat(const QString &path)
{
    QString lower = path.toLower();
    bool r = lower.endsWith(".doc") || lower.endsWith(".docx") || lower.endsWith(".rtf");
    debugLog(QString("isWordFormat(%1) = %2").arg(path).arg(r));
    return r;
}

static bool isPptFormat(const QString &path)
{
    QString lower = path.toLower();
    bool r = lower.endsWith(".ppt") || lower.endsWith(".pptx")
        || lower.endsWith(".pptm") || lower.endsWith(".ppsm");
    debugLog(QString("isPptFormat(%1) = %2").arg(path).arg(r));
    return r;
}

static QString writePs1(const QString &scriptBody)
{
    QString ps1Path = QDir::tempPath() + "/mmplayer_office_helper.ps1";
    QFile f(ps1Path);
    if (!f.open(QIODevice::WriteOnly)) {
        debugLog(QString("writePs1 FAILED to open %1").arg(ps1Path));
        return QString();
    }
    f.write("\xEF\xBB\xBF");
    f.write(scriptBody.toUtf8());
    f.close();
    debugLog(QString("writePs1 OK -> %1 (%2 bytes)").arg(ps1Path).arg(f.size()));

    QFile check(ps1Path);
    if (check.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&check);
        ts.setEncoding(QStringConverter::Utf8);
        QString content = ts.readAll();
        check.close();
        debugLog(QString("ps1 content:\n%1").arg(content));
    }
    return ps1Path;
}

static int runPs1(const QString &ps1Path)
{
    debugLog(QString("runPs1 START: %1").arg(ps1Path));
    QProcess proc;
    proc.start("powershell.exe", {
        "-NoProfile", "-NonInteractive",
        "-ExecutionPolicy", "Bypass",
        "-File", ps1Path
    });
    proc.waitForFinished(45000);
    int code = proc.exitCode();
    QByteArray out = proc.readAllStandardOutput();
    QByteArray err = proc.readAllStandardError();

    debugLog(QString("runPs1 DONE: exitCode=%1, stdout=%2, stderr=%3, procErr=%4")
        .arg(code).arg(QString::fromUtf8(out)).arg(QString::fromUtf8(err)).arg(proc.errorString()));
    return code;
}

static QString readDocxViaWord(const QString &path)
{
    debugLog(QString("readDocxViaWord CALLED: %1").arg(path));
    QString tmpOut = QDir::tempPath() + "/mmplayer_word_read.txt";
    QString nativePath = QDir::toNativeSeparators(path);
    QString nativeOut  = QDir::toNativeSeparators(tmpOut);

    QString scriptBody = QStringLiteral(
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
        "} catch {\n"
        "  Write-Host \"OPEN_ERROR: $($_.Exception.Message)\"\n"
        "}\n"
        "if ($text) {\n"
        "  [System.IO.File]::WriteAllText('%2', $text, [System.Text.Encoding]::UTF8)\n"
        "  Write-Host 'WRITE_OK'\n"
        "} else {\n"
        "  Write-Host 'NO_TEXT'\n"
        "}\n"
        "try { if ($doc) { $doc.Close($false) } } catch {}\n"
        "try { if ($word) { $word.Quit() } } catch {}\n"
        "try { if ($word) { [System.Runtime.Interopservices.Marshal]::ReleaseComObject($word) | Out-Null } } catch {}\n"
        "exit 0\n"
    ).arg(nativePath, nativeOut);

    QString ps1 = writePs1(scriptBody);
    if (ps1.isEmpty()) return QString();
    runPs1(ps1);
    QFile::remove(ps1);

    QFile f(tmpOut);
    if (!f.exists()) return QString();
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    QString content = ts.readAll();
    f.close();
    QFile::remove(tmpOut);
    return content;
}

static QString readPptxViaPowerPoint(const QString &path)
{
    QString tmpOut = QDir::tempPath() + "/mmplayer_ppt_read.txt";
    QString nativePath = QDir::toNativeSeparators(path);
    QString nativeOut  = QDir::toNativeSeparators(tmpOut);

    QString scriptBody = QStringLiteral(
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
        "} catch {\n"
        "  Write-Host \"OPEN_ERROR: $($_.Exception.Message)\"\n"
        "}\n"
        "$text = $sb.ToString()\n"
        "if ($text) {\n"
        "  [System.IO.File]::WriteAllText('%2', $text, [System.Text.Encoding]::UTF8)\n"
        "  Write-Host 'WRITE_OK'\n"
        "} else {\n"
        "  Write-Host 'NO_TEXT'\n"
        "}\n"
        "try { if ($pres) { $pres.Close() } } catch {}\n"
        "try { if ($ppt) { $ppt.Quit() } } catch {}\n"
        "try { if ($ppt) { [System.Runtime.Interopservices.Marshal]::ReleaseComObject($ppt) | Out-Null } } catch {}\n"
        "exit 0\n"
    ).arg(nativePath, nativeOut);

    QString ps1 = writePs1(scriptBody);
    if (ps1.isEmpty()) return QString();
    runPs1(ps1);
    QFile::remove(ps1);

    QFile f(tmpOut);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    QString content = ts.readAll();
    f.close();
    QFile::remove(tmpOut);
    return content;
}

static bool saveDocxViaWord(const QString &path, const QString &text)
{
    QString tmpIn = QDir::tempPath() + "/mmplayer_word_write.txt";
    QString nativePath = QDir::toNativeSeparators(path);
    QString nativeIn   = QDir::toNativeSeparators(tmpIn);

    {
        QFile f(tmpIn);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << text;
        f.close();
    }

    QString lower = path.toLower();
    int fmt = 16;
    if (lower.endsWith(".docx")) fmt = 16;
    else if (lower.endsWith(".doc")) fmt = 0;
    else if (lower.endsWith(".rtf")) fmt = 6;

    QString scriptBody = QStringLiteral(
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
        "} catch {\n"
        "  Write-Host \"SAVE_ERROR: $($_.Exception.Message)\"\n"
        "}\n"
        "try { if ($doc) { $doc.Close($false) } } catch {}\n"
        "try { if ($word) { $word.Quit() } } catch {}\n"
        "try { if ($word) { [System.Runtime.Interopservices.Marshal]::ReleaseComObject($word) | Out-Null } } catch {}\n"
        "if ($ok) { exit 0 } else { exit 1 }\n"
    ).arg(nativeIn, nativePath).arg(fmt);

    QString ps1 = writePs1(scriptBody);
    if (ps1.isEmpty()) { QFile::remove(tmpIn); return false; }
    int code = runPs1(ps1);
    QFile::remove(ps1);
    QFile::remove(tmpIn);
    return code == 0;
}

TextPanel::TextPanel(const QString &path, QWidget *parent)
    : MediaPanel(Text, path, parent), m_currentPath(path)
{
    m_textEdit = new QPlainTextEdit(m_mediaArea);

    QVBoxLayout *lay = new QVBoxLayout(m_mediaArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_textEdit);

    QString content;
    if (isWordFormat(path)) {
        m_infoLabel->setText(QString::fromUtf8("正在通过 Word 加载..."));
        QCoreApplication::processEvents();
        content = readDocxViaWord(path);
        if (content.isEmpty()) {
            content = QString::fromUtf8("(无法读取此 Word 文件，请确保已安装 Microsoft Word 或 WPS)");
        }
    } else if (isPptFormat(path)) {
        m_infoLabel->setText(QString::fromUtf8("正在通过 PowerPoint 加载..."));
        QCoreApplication::processEvents();
        content = readPptxViaPowerPoint(path);
        if (content.isEmpty()) {
            content = QString::fromUtf8("(无法读取此 PPT 文件，请确保已安装 PowerPoint 或 WPS 演示)");
        }
    } else {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            content = stream.readAll();
            file.close();
            if (content.isEmpty()) {
                file.open(QIODevice::ReadOnly);
                content = QString::fromLocal8Bit(file.readAll());
                file.close();
            }
        }
    }

    m_blockTextSignal = true;
    m_textEdit->setPlainText(content);
    m_blockTextSignal = false;

    int lines = content.isEmpty() ? 0 : content.count('\n') + 1;
    int chars = content.size();
    if (lines == 1 && content.isEmpty()) {
        m_infoLabel->setText(QString::fromUtf8("新文件"));
    } else {
        m_infoLabel->setText(QString("%1 %2 \xc2\xb7 %3 %4")
            .arg(lines).arg(QString::fromUtf8("行"))
            .arg(chars).arg(QString::fromUtf8("字符")));
    }

    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &TextPanel::onTextChanged);

    QObject::disconnect(m_closeBtn, &QPushButton::clicked, this, nullptr);
    connect(m_closeBtn, &QPushButton::clicked, this, &TextPanel::onCloseRequested);

    auto *saveShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), m_textEdit);
    saveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveShortcut, &QShortcut::activated, this, &TextPanel::onSave);

    auto *saveAsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), m_textEdit);
    saveAsShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveAsShortcut, &QShortcut::activated, this, &TextPanel::onSaveAs);

    m_playBtn->hide();
    m_stopBtn->hide();
    m_progress->hide();
    m_timeLabel->hide();
    m_volume->hide();

    auto *topBar = m_titleLabel->parentWidget();
    auto *topLay = qobject_cast<QHBoxLayout*>(topBar->layout());
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
    connect(m_themeBtn, &QPushButton::clicked, this, [this]() {
        applyTheme(!m_darkMode);
    });
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
        "font-family: Consolas;"
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

    m_themeBtn->setText(dark ? "\xe2\x98\x91" : "\xe2\x98\x9e");
    m_themeBtn->setToolTip(dark ? QString::fromUtf8("切换为明亮模式")
                                : QString::fromUtf8("切换为暗黑模式"));
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
            QMessageBox::warning(this, QString::fromUtf8("保存失败"),
                QString::fromUtf8("无法通过 Word 保存，请确保已安装 Microsoft Word"));
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
    m_infoLabel->setText(QString::fromUtf8("已保存 · ") + QFileInfo(path).fileName());
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