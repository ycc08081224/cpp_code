#pragma once

#include "MediaPanel.h"
#include <QPlainTextEdit>
#include <QString>

class QPushButton;
class QProcess;

class TextPanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit TextPanel(const QString &path, QWidget *parent = nullptr);
    ~TextPanel() override;

    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}

    void requestClose() override;
    bool hasUnsavedChanges() const { return m_dirty; }

private slots:
    void onTextChanged();
    void onSave();
    void onSaveAs();
    void onCloseRequested();
    void onExtractFinished(int exitCode, int exitStatus);
    void onExtractError(int error);

private:
    void setDirty(bool dirty);
    void saveToFile(const QString &path);
    void updateTitle();
    void applyTheme(bool dark);

    void startLoad();
    void loadPlainText();
    void startOfficeExtract(bool ppt);
    void applyContent(const QString &content);
    void cleanupTempFiles();

    QPlainTextEdit *m_textEdit = nullptr;
    QPushButton *m_themeBtn = nullptr;
    QString m_currentPath;

    QProcess *m_proc = nullptr;
    QString m_tmpOut;
    QString m_tmpPs1;
    bool m_officeLoad = false;   // 当前是否在异步提取 docx/pptx

    bool m_dirty = false;
    bool m_blockTextSignal = false;
    bool m_darkMode = true;
};
