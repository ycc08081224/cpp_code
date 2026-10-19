#pragma once
#include "MediaPanel.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMovie>
#include <QSize>

class QLabel;

class ImagePanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit ImagePanel(const QString &path, QWidget *parent = nullptr);
    ~ImagePanel() override;

    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildZoomBar();
    void fitToWindow();
    void actualSize();
    void applyZoom(qreal factor);
    void zoomByButton(qreal factor);
    void updateZoomLabel();
    void showMessage(const QString &text);

    QGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_item = nullptr;
    QMovie *m_movie = nullptr;
    QWidget *m_zoomBar = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QSize m_lastFrameSize;
    bool m_fitMode = true;
    qreal m_zoom = 1.0;
    bool m_empty = false;
};
