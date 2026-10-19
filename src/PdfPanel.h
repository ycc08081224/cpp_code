#pragma once

#include "MediaPanel.h"
#include <QLabel>
#include <QPushButton>

#ifdef HAS_QTPDF
#include <QPdfDocument>
#include <QPdfView>
#endif

class PdfPanel : public MediaPanel
{
    Q_OBJECT
public:
    explicit PdfPanel(const QString &path, QWidget *parent = nullptr);
    ~PdfPanel() override = default;

    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}

private slots:
    void onPrevPage();
    void onNextPage();
    void onFirstPage();
    void onLastPage();
    void onZoomIn();
    void onZoomOut();
    void onToggleFit();
    void updatePageLabel(int page);

private:
#ifdef HAS_QTPDF
    QPdfDocument *m_doc = nullptr;
    QPdfView *m_pdfView = nullptr;
#endif
    QPushButton *m_prevPageBtn = nullptr;
    QPushButton *m_nextPageBtn = nullptr;
    QPushButton *m_firstPageBtn = nullptr;
    QPushButton *m_lastPageBtn = nullptr;
    QPushButton *m_zoomInBtn = nullptr;
    QPushButton *m_zoomOutBtn = nullptr;
    QPushButton *m_fitBtn = nullptr;
    QLabel *m_pageLabel = nullptr;
    QLabel *m_placeholderLabel = nullptr;
    int m_pageCount = 0;
};