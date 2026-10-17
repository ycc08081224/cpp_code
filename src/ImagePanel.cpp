#include "ImagePanel.h"
#include <QVBoxLayout>

ImagePanel::ImagePanel(const QString &path, QWidget *parent)
    : MediaPanel(Image, path, parent)
{
    m_imageLabel = new QLabel(m_mediaArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background: #111118;");
    m_imageLabel->setScaledContents(false);
    QVBoxLayout *lay = new QVBoxLayout(m_mediaArea);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_imageLabel);

    m_originalPixmap = QPixmap(path);
    if (m_originalPixmap.isNull()) {
        m_imageLabel->setText(QString::fromUtf8("❌ 无法加载图片"));
        m_infoLabel->setText("");
    } else {
        m_infoLabel->setText(QString("%1x%2")
            .arg(m_originalPixmap.width())
            .arg(m_originalPixmap.height()));
        m_lastRenderSize = QSize(-1, -1);
        renderImage();
    }

    m_playBtn->hide();
    m_stopBtn->hide();
    m_progress->hide();
    m_timeLabel->hide();
    m_volume->hide();
}

void ImagePanel::renderImage()
{
    if (m_originalPixmap.isNull()) return;
    QSize areaSize = m_mediaArea->size();
    if (areaSize.width() < 30 || areaSize.height() < 30) return;

    if (areaSize == m_lastRenderSize) return;

    QPixmap scaled = m_originalPixmap.scaled(areaSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_imageLabel->setPixmap(scaled);
    m_lastRenderSize = areaSize;
}

void ImagePanel::resizeEvent(QResizeEvent *event)
{
    MediaPanel::resizeEvent(event);
    renderImage();
}