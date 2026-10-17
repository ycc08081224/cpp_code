#pragma once
#include "MediaPanel.h"
#include <QLabel>
#include <QScrollArea>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>

class ImagePanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit ImagePanel(const QString &path, QWidget *parent = nullptr);
    ~ImagePanel() override = default;

    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QLabel *m_imageLabel = nullptr;
    QPixmap m_originalPixmap;
    QSize m_lastRenderSize{-1, -1};
    void renderImage();
};