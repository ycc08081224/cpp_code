#include "PdfPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

#ifdef HAS_QTPDF
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfPageNavigator>
#endif

PdfPanel::PdfPanel(const QString &path, QWidget *parent)
    : MediaPanel(Pdf, path, parent)
{
    // PDF 没有播放/进度/音量，翻页和缩放工具栏在媒体区内部，底部控制栏直接移除
    removeControlBar();

    QVBoxLayout *outer = new QVBoxLayout(m_mediaArea);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

#ifdef HAS_QTPDF
    m_doc = new QPdfDocument(this);
    auto loadResult = m_doc->load(path);

    if (loadResult == QPdfDocument::Error::None) {
        m_pageCount = m_doc->pageCount();

        m_pdfView = new QPdfView(m_mediaArea);
        m_pdfView->setDocument(m_doc);
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        m_pdfView->setStyleSheet("background: #404040; border: none;");
        outer->addWidget(m_pdfView, 1);

        QHBoxLayout *toolbar = new QHBoxLayout();
        toolbar->setContentsMargins(6, 4, 6, 4);
        toolbar->setSpacing(4);
        toolbar->addStretch();

        // 用 ASCII 而不是 ⏪◀▶⏩：默认字体里这些符号常常缺字形，渲染成方块
        m_firstPageBtn = new QPushButton("|<", m_mediaArea);
        m_prevPageBtn  = new QPushButton("<",  m_mediaArea);
        m_nextPageBtn  = new QPushButton(">",  m_mediaArea);
        m_lastPageBtn  = new QPushButton(">|", m_mediaArea);
        m_firstPageBtn->setToolTip(QString::fromUtf8("第一页"));
        m_prevPageBtn->setToolTip(QString::fromUtf8("上一页"));
        m_nextPageBtn->setToolTip(QString::fromUtf8("下一页"));
        m_lastPageBtn->setToolTip(QString::fromUtf8("最后一页"));
        m_zoomOutBtn   = new QPushButton("A-", m_mediaArea);
        m_zoomInBtn    = new QPushButton("A+", m_mediaArea);
        m_fitBtn       = new QPushButton(QString::fromUtf8("适合"), m_mediaArea);
        m_fitBtn->setToolTip(QString::fromUtf8("在「适合宽度 / 适合整页」之间切换"));

        for (auto *b : {m_firstPageBtn, m_prevPageBtn, m_nextPageBtn, m_lastPageBtn, m_zoomOutBtn, m_zoomInBtn, m_fitBtn}) {
            b->setFixedHeight(22);
            b->setStyleSheet(
                "QPushButton { background: #3a3a55; color: #ddd; border: 1px solid #555; border-radius: 3px; padding: 0 8px; font-size: 11px; }"
                "QPushButton:hover { background: #4a4a6a; }"
            );
        }

        m_pageLabel = new QLabel(QString("1 / %1").arg(m_pageCount), m_mediaArea);
        m_pageLabel->setStyleSheet("color: #ddd; font-size: 11px; padding: 0 6px;");
        m_pageLabel->setAlignment(Qt::AlignCenter);

        toolbar->addWidget(m_firstPageBtn);
        toolbar->addWidget(m_prevPageBtn);
        toolbar->addWidget(m_pageLabel);
        toolbar->addWidget(m_nextPageBtn);
        toolbar->addWidget(m_lastPageBtn);
        toolbar->addSpacing(12);
        toolbar->addWidget(m_zoomOutBtn);
        toolbar->addWidget(m_zoomInBtn);
        toolbar->addWidget(m_fitBtn);
        toolbar->addStretch();

        outer->addLayout(toolbar);

        connect(m_firstPageBtn, &QPushButton::clicked, this, &PdfPanel::onFirstPage);
        connect(m_prevPageBtn,  &QPushButton::clicked, this, &PdfPanel::onPrevPage);
        connect(m_nextPageBtn,  &QPushButton::clicked, this, &PdfPanel::onNextPage);
        connect(m_lastPageBtn,  &QPushButton::clicked, this, &PdfPanel::onLastPage);
        connect(m_zoomInBtn,    &QPushButton::clicked, this, &PdfPanel::onZoomIn);
        connect(m_zoomOutBtn,   &QPushButton::clicked, this, &PdfPanel::onZoomOut);
        connect(m_fitBtn,       &QPushButton::clicked, this, &PdfPanel::onToggleFit);

        if (auto *nav = m_pdfView->pageNavigator()) {
            connect(nav, &QPdfPageNavigator::currentPageChanged,
                    this, &PdfPanel::updatePageLabel);
        }

        m_infoLabel->setText(QString("%1 页").arg(m_pageCount));
    } else {
        m_placeholderLabel = new QLabel(
            QString("PDF 加载失败\n错误代码: %1").arg(static_cast<int>(loadResult)), m_mediaArea);
        m_placeholderLabel->setAlignment(Qt::AlignCenter);
        m_placeholderLabel->setStyleSheet("background: #555; color: #ff8080; font-size: 13px;");
        m_placeholderLabel->setWordWrap(true);
        outer->addWidget(m_placeholderLabel);
        m_infoLabel->setText("加载失败");
    }
#else
    m_placeholderLabel = new QLabel(
        "当前 Qt 版本未包含 Pdf 模块\n安装 Qt 6.6+ (勾选 PDF 组件) 即可预览", m_mediaArea);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet("background: #555; color: #ccc; font-size: 13px;");
    m_placeholderLabel->setWordWrap(true);
    outer->addWidget(m_placeholderLabel);
#endif
}

#ifdef HAS_QTPDF
static QPdfPageNavigator *nav(QPdfView *view) { return view ? view->pageNavigator() : nullptr; }
#endif

void PdfPanel::onPrevPage()
{
#ifdef HAS_QTPDF
    if (auto *n = nav(m_pdfView)) n->jump(n->currentPage() - 1, QPointF(), 0);
#endif
}

void PdfPanel::onNextPage()
{
#ifdef HAS_QTPDF
    if (auto *n = nav(m_pdfView)) n->jump(n->currentPage() + 1, QPointF(), 0);
#endif
}

void PdfPanel::onFirstPage()
{
#ifdef HAS_QTPDF
    if (auto *n = nav(m_pdfView)) n->jump(0, QPointF(), 0);
#endif
}

void PdfPanel::onLastPage()
{
#ifdef HAS_QTPDF
    if (auto *n = nav(m_pdfView)) n->jump(m_pageCount - 1, QPointF(), 0);
#endif
}

void PdfPanel::onZoomIn()
{
#ifdef HAS_QTPDF
    if (m_pdfView) {
        qreal z = m_pdfView->zoomFactor();
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(qMin(z * 1.25, 8.0));
    }
#endif
}

void PdfPanel::onZoomOut()
{
#ifdef HAS_QTPDF
    if (m_pdfView) {
        qreal z = m_pdfView->zoomFactor();
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(qMax(z / 1.25, 0.1));
    }
#endif
}

void PdfPanel::onToggleFit()
{
#ifdef HAS_QTPDF
    if (!m_pdfView) return;
    if (m_pdfView->zoomMode() == QPdfView::ZoomMode::FitToWidth) {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
        if (m_fitBtn) m_fitBtn->setText(QString::fromUtf8("整页"));
    } else {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        if (m_fitBtn) m_fitBtn->setText(QString::fromUtf8("适合"));
    }
#endif
}

void PdfPanel::updatePageLabel(int page)
{
    if (m_pageLabel)
        m_pageLabel->setText(QString("%1 / %2").arg(page + 1).arg(m_pageCount));
}