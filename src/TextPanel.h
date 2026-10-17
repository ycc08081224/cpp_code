#pragma once

#include "MediaPanel.h"
#include <QPlainTextEdit>

class QPushButton;

class TextPanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit TextPanel(const QString &path, QWidget *parent = nullptr);
    ~TextPanel() override = default;

    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}

private slots:
    void onTextChanged();
    void onSave();
    void onSaveAs();
    void onCloseRequested();

private:
    void setDirty(bool dirty);
    void saveToFile(const QString &path);
    void updateTitle();
    void applyTheme(bool dark);

    QPlainTextEdit *m_textEdit = nullptr;
    QPushButton *m_themeBtn = nullptr;
    QString m_currentPath;
    bool m_dirty = false;
    bool m_blockTextSignal = false;
    bool m_darkMode = true;
};