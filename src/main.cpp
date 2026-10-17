#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QScreen>
#include <QGuiApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <csignal>
#include "MainWindow.h"

static const char *LOG_FILE = "D:/gongzuo/zuoye/huochairen/cpp_code/build/crash.log";

static void crashSignalHandler(int sig)
{
    QFile f(LOG_FILE);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "\n===== CRASH at " << QDateTime::currentDateTime().toString()
           << " signal=" << sig << " =====\n";
        f.close();
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void logMsg(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QFile f(LOG_FILE);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
           << " [" << (type == QtDebugMsg ? "DBG" : type == QtWarningMsg ? "WRN" : "ERR") << "] "
           << msg << Qt::endl;
        f.close();
    }
}

static QIcon buildAppIcon()
{
    QPixmap pm(256, 256);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Shadow
    p.setPen(Qt::NoPen);
    QRadialGradient shadowGrad(128, 140, 140);
    shadowGrad.setColorAt(0.0, QColor(0, 0, 0, 80));
    shadowGrad.setColorAt(0.6, QColor(0, 0, 0, 20));
    shadowGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(shadowGrad);
    p.drawEllipse(16, 24, 224, 224);

    // Background rounded rect with deep violet gradient
    QPainterPath bgPath;
    bgPath.addRoundedRect(20, 20, 216, 216, 44, 44);
    QLinearGradient bgGrad(20, 20, 236, 236);
    bgGrad.setColorAt(0.0, QColor("#0f0c29"));
    bgGrad.setColorAt(0.5, QColor("#302b63"));
    bgGrad.setColorAt(1.0, QColor("#24243e"));
    p.setBrush(bgGrad);
    p.drawPath(bgPath);

    // 2x2 panels
    const int panelGap = 10;
    const int panelMargin = 24;
    const int panelW = (216 - panelMargin * 2 - panelGap) / 2;
    const int panelH = panelW;
    const int radius = 20;

    struct PanelInfo {
        int x, y;
        QColor color;
        QColor accent;
        enum IconType { Play, Image, Music, Doc } icon;
    };

    PanelInfo panels[] = {
        { 24, 24,              QColor("#ec4899"), QColor("#f9a8d4"), PanelInfo::Play },
        { 24 + panelW + panelGap, 24,   QColor("#06b6d4"), QColor("#67e8f9"), PanelInfo::Image },
        { 24, 24 + panelH + panelGap,   QColor("#10b981"), QColor("#6ee7b7"), PanelInfo::Music },
        { 24 + panelW + panelGap, 24 + panelH + panelGap, QColor("#f59e0b"), QColor("#fcd34d"), PanelInfo::Doc },
    };

    for (const auto &pan : panels) {
        // Panel rounded rect with gradient
        QPainterPath pp;
        pp.addRoundedRect(pan.x, pan.y, panelW, panelH, radius, radius);
        QLinearGradient pg(pan.x, pan.y, pan.x, pan.y + panelH);
        pg.setColorAt(0.0, pan.accent);
        pg.setColorAt(0.3, pan.color);
        pg.setColorAt(1.0, pan.color.darker(120));
        p.setBrush(pg);
        p.drawPath(pp);

        // Inner icon
        p.setPen(Qt::white);
        p.setBrush(Qt::white);
        p.setOpacity(0.92);

        int cx = pan.x + panelW / 2;
        int cy = pan.y + panelH / 2;
        int sz = panelW / 3;

        if (pan.icon == PanelInfo::Play) {
            // Play triangle
            QPainterPath tri;
            tri.moveTo(cx - sz / 2, cy - sz / 2);
            tri.lineTo(cx - sz / 2, cy + sz / 2);
            tri.lineTo(cx + sz / 2 + 4, cy);
            tri.closeSubpath();
            p.drawPath(tri);
        } else if (pan.icon == PanelInfo::Image) {
            // Image: rectangle + mountain + sun
            int iw = sz + 8, ih = sz;
            int ix = cx - iw / 2, iy = cy - ih / 2;
            QPainterPath imgPath;
            imgPath.addRoundedRect(ix, iy, iw, ih, 4, 4);
            p.setBrush(QColor(255, 255, 255, 60));
            p.drawPath(imgPath);

            p.setBrush(Qt::white);
            QPainterPath mountain;
            mountain.moveTo(ix + 4, iy + ih - 4);
            mountain.lineTo(ix + iw / 3, iy + ih / 2 + 2);
            mountain.lineTo(ix + iw * 2 / 3, iy + ih / 2 + 6);
            mountain.lineTo(ix + iw - 4, iy + ih - 4);
            mountain.closeSubpath();
            p.drawPath(mountain);

            p.setBrush(Qt::white);
            p.drawEllipse(ix + iw - 12, iy + 4, 8, 8);
        } else if (pan.icon == PanelInfo::Music) {
            // Music note: vertical bar + flag
            int barW = 5, barH = sz + 6;
            int barX = cx - barW / 2 - 4;
            int barY = cy - barH / 2 + 2;
            p.drawRect(barX, barY, barW, barH);
            // Flag
            QPainterPath flag;
            flag.moveTo(barX + barW, barY);
            flag.lineTo(barX + barW + sz / 2, barY + sz / 4);
            flag.lineTo(barX + barW, barY + sz / 2);
            flag.closeSubpath();
            p.drawPath(flag);
            // Note head
            p.drawEllipse(barX - 4, barY + barH - 10, 12, 10);
        } else if (pan.icon == PanelInfo::Doc) {
            // Document: page with lines
            int dw = sz + 4, dh = sz + 10;
            int dx = cx - dw / 2, dy = cy - dh / 2;
            QPainterPath docPath;
            docPath.moveTo(dx + 8, dy);
            docPath.lineTo(dx + dw - 4, dy);
            docPath.lineTo(dx + dw - 4, dy + dh - 4);
            docPath.lineTo(dx, dy + dh - 4);
            docPath.lineTo(dx, dy + 4);
            docPath.closeSubpath();
            p.setBrush(QColor(255, 255, 255, 85));
            p.drawPath(docPath);

            p.setPen(Qt::white);
            p.setBrush(Qt::NoBrush);
            p.setOpacity(0.9);
            for (int i = 0; i < 4; ++i) {
                int ly = dy + 8 + i * 6;
                int lw = (i == 3) ? dw - 14 : dw - 10;
                p.drawLine(dx + 6, ly, dx + 6 + lw, ly);
            }
        }
        p.setOpacity(1.0);
    }

    // Center big play button overlay (semi-transparent dark circle + white play)
    int btnR = 48;
    int btnX = 128 - btnR, btnY = 128 - btnR;
    QPainterPath btnBg;
    btnBg.addEllipse(btnX, btnY, btnR * 2, btnR * 2);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawPath(btnBg);

    // Thick white ring
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, 4));
    p.drawEllipse(btnX + 2, btnY + 2, btnR * 2 - 4, btnR * 2 - 4);

    // Play triangle in center
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    QPainterPath bigPlay;
    int px = 128 - 16, py = 128 - 22;
    bigPlay.moveTo(px, py);
    bigPlay.lineTo(px, py + 44);
    bigPlay.lineTo(px + 38, py + 22);
    bigPlay.closeSubpath();
    p.drawPath(bigPlay);

    p.end();

    // Save PNG for preview
    pm.save("D:/gongzuo/zuoye/huochairen/cpp_code/build/app_icon.png", "PNG");

    QIcon icon(pm);
    return icon;
}

int main(int argc, char *argv[])
{
    signal(SIGSEGV, crashSignalHandler);
    signal(SIGABRT, crashSignalHandler);
    signal(SIGFPE, crashSignalHandler);
    signal(SIGILL, crashSignalHandler);
    qInstallMessageHandler(logMsg);

    QApplication app(argc, argv);
    app.setOrganizationName("MultiMediaPlayer");
    app.setApplicationName("MultiMediaPlayer");
    app.setApplicationVersion("1.1.0");
    app.setWindowIcon(buildAppIcon());

    QApplication::setStyle("Fusion");

    MainWindow w;
    w.setWindowIcon(app.windowIcon());

    QScreen *scr = QGuiApplication::primaryScreen();
    if (scr) {
        QRect sg = scr->availableGeometry();
        int ww = int(sg.width() * 0.9);
        int wh = int(sg.height() * 0.9);
        w.resize(ww, wh);
        w.move(sg.center().x() - ww / 2, sg.center().y() - wh / 2);
    } else {
        w.resize(1200, 800);
    }

    w.show();

    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        w.addPanel(args[i]);
    }

    return app.exec();
}