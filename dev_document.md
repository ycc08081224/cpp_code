# 全能媒体播放器 — 开发文档

## 1. 项目概述

一个基于 **Qt 6.7 (MinGW)** 的 Windows 桌面多媒播放器，核心特点是**多面板并排**——可以同时加载视频、音频、图片、PDF、文本等多种媒体，每个媒体占据一个独立 Panel，通过网格布局同时呈现在一个窗口里。

### 构建环境
- Qt 6.7.3 (MinGW 13.1.0 64-bit)
- CMake 3.x
- 运行时依赖：ffmpeg plugin（Qt Multimedia 默认自带）

### 构建命令
```powershell
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
cmake -B build -G "MinGW Makefiles"
cmake --build build -j
```

---

## 2. 源码结构

```
src/
├── main.cpp            程序入口，创建 MainWindow
├── MainWindow.h/cpp    主窗口，管理所有 Panel 的创建、布局、销毁
├── MediaPanel.h/cpp    抽象基类，所有 Panel 共有：标题栏 + 控制栏 + 自动隐藏
├── VideoPanel.h/cpp    视频播放（QVideoWidget + QMediaPlayer）
├── AudioPanel.h/cpp    音频播放（QMediaPlayer，带独立悬浮窗口）
├── ImagePanel.h/cpp    图片显示（QLabel + QPixmap 缩放）
├── PdfPanel.h/cpp      PDF 显示（QPdfView）
├── TextPanel.h/cpp     文本显示（QPlainTextEdit）
└── (无其他文件)
```

### 类继承关系
```
QWidget
└── MediaPanel (抽象基类)
    ├── VideoPanel
    ├── AudioPanel
    ├── ImagePanel
    ├── PdfPanel
    └── TextPanel
```

---

## 3. 核心架构

### 3.1 MediaPanel — 所有面板的骨架

MediaPanel 是抽象基类，只负责**UI 框架**和**通用控制逻辑**，不包含实际的媒体解码/渲染代码。子类只需实现 `play/pause/stop/seek/setVolume` 六个纯虚函数。

#### 内部布局

```
MediaPanel (QWidget, QVBoxLayout)
│
├── m_topBar (QWidget, 固定高 30)   ← 顶部标题栏
│   ├── m_titleLabel                ← 🎬 filename.mp4
│   ├── m_infoLabel                 ← 1920x1080 / loading...
│   ├── m_amplifyBtn                ← ⛶ 放大/还原
│   └── m_closeBtn                  ← ✕ 关闭
│
├── m_mediaArea (QWidget, stretch=1) ← 中间媒体区，子类把自己的媒体 widget 放这里
│   └── (子类填充：QVideoWidget / QLabel / QPdfView / ...)
│
└── m_ctrl (QWidget, 固定高 40)      ← 底部控制栏
    ├── m_playBtn                   ← ▶ / ⏸
    ├── m_stopBtn                   ← ■
    ├── m_backBtn                   ← ⏪ -5s
    ├── m_fwdBtn                    ← ⏩ +5s
    ├── m_progress (QSlider)        ← 进度条（支持点击跳转）
    ├── m_timeLabel                 ← 0:13 / 3:52
    ├── m_muteBtn                   ← 🔊 / 🔇
    └── m_volume (QSlider)          ← 音量 0-100
```

#### 为什么用 QVBoxLayout 而不是浮动叠加？

1.0 版用的是 QVideoWidget（Windows 上创建独立原生 HWND）。如果 topBar/ctrl 也 `setGeometry` 浮动叠在上面，QVideoWidget 的 HWND 会盖在所有 Qt widget 之上。Qt 的 `raise()` 只能管 Qt 逻辑 z-order，管不了 Windows 原生 HWND。

1.1 版的"修复"方案是换成 QGraphicsVideoItem（不走 HWND），但代价是：**所有视频帧在 CPU 上做 YUV→RGB 颜色空间转换 + QPainter 绘制到一个共享的 QGraphicsView 上**，12 路视频 = 每秒 360 次主线程阻塞 → 必卡。

1.1.2 的最终方案：**用 QVBoxLayout 把三个区域垂直排列**，完全不重叠。QVideoWidget 的 HWND 只覆盖中间 m_mediaArea 那块区域，topBar 和 ctrl 在它上下方，天然不会被盖住。布局逻辑简单清晰，无需任何 Win32 API hack。

### 3.2 MainWindow — 网格布局管理器

MainWindow 使用 `QSplitter` 嵌套实现自适应网格：

```
MainWindow
└── m_container (QWidget)
    └── m_hSplitter (QSplitter, Qt::Horizontal)
        ├── colSplitter[0] (QSplitter, Qt::Vertical) → MediaPanel, MediaPanel, ...
        ├── colSplitter[1] (QSplitter, Qt::Vertical) → MediaPanel, ...
        └── colSplitter[N] ...
```

- 最多 12 个 Panel（`m_maxPanels = 12`）
- `relayout()` 根据 Panel 数量自动决定行数/列数
- 支持拖拽添加：整个 MainWindow 接收 `dragEnterEvent/dropEvent`
- 支持单 Panel 放大：点 ⛶ 按钮，该 Panel 独占窗口；再点还原（4 级放大：半宽→全宽→半屏→全屏）

### 3.3 AudioPanel — 特殊的悬浮面板

AudioPanel 不走 MediaPanel 的默认布局。它的媒体内容区域是空的（音频没有画面），而是额外创建了一个**独立的 QWidget 悬浮窗口**来显示封面/可视化，跟随 MainWindow 移动。

---

## 4. 已实现的功能清单

| 功能 | 说明 |
|------|------|
| 多格式支持 | 视频 (mp4/avi/mkv/mov/wmv/flv/webm)、音频 (mp3/wav/flac/aac/ogg/m4a)、图片 (jpg/png/bmp/gif/webp)、PDF、文本 |
| 多面板并排 | 最多 12 个 Panel 同时显示，自动网格布局 |
| 文件拖拽 | 支持从资源管理器拖拽文件/文件夹到窗口 |
| 菜单/工具栏 | 文件、播放、视图、帮助四个菜单 |
| 快捷键 | Ctrl+O 添加文件、Ctrl+Shift+O 添加文件夹、Ctrl+1~9 切换放大、Esc 全屏/退出、Space 播放暂停等 |
| 单 Panel 放大 | ⛶ 按钮 4 级放大（半宽→全宽→半屏→全屏→还原） |
| Panel 关闭 | ✕ 按钮关闭单个 Panel |
| 播放控制 | 每个 Panel 独立的 play/pause/stop/seek±5s |
| 进度条 | 点击任意位置跳转（通过 eventFilter 计算 slider groove） |
| 音量/静音 | 每个 Panel 独立音量滑块 + 静音按钮 |
| 自动循环 | 播放完自动回到开头（`m_loop = true`） |
| 全选控制 | 菜单栏"播放全部/暂停全部/停止全部" |
| 音频悬浮窗 | AudioPanel 额外的独立窗口，显示音频信息 |
| 圆角外观 | MediaPanel 整体圆角 14px（通过 `setMask` + QPainterPath） |
| 状态栏 | 底部显示加载/跳过/上限提示 |
| 持久化状态 | 关闭后重开恢复：窗口位置/大小/最大化、上次打开的文件列表、循环偏好 |
| 进度记忆 | 关闭面板时保存播放位置，下次打开同一文件时自动恢复（位置已过 95% 时长的不保存） |
| 历史记录 | 记录最近打开的文件（上限 20 条），菜单 [文件 → 最近打开] 快速加载；自动清理已删除文件；关闭程序时将当前打开文件合并到历史；旧版本的 `files/lastFiles` 自动迁移 |
| 高性能图片渲染 | 图片缩放用 `Qt::FastTransformation` + 尺寸变化缓存，避免平滑缩放的 CPU 黑洞 |
| 降低定时器开销 | pollMousePosition 用 `CoarseTimer`（Windows 默认 15.6ms 时钟）替代 `PreciseTimer`，间隔 100ms；且仅视频/音频面板启动 |
| **倍速播放** | 播放菜单 0.5x / 0.75x / 1.0x / 1.25x / 1.5x / 2.0x，已开面板立即生效，新面板继承，随 `preferences/rate` 持久化 |
| **图片缩放/平移** | 图片面板改为 `QGraphicsView`：滚轮缩放、拖拽平移、双击还原；底部缩放条提供「− 缩小 / 百分比 / + 放大 / 适应窗口 / 原始大小」；超大图先降采样到 3840 |
| **GIF 动画** | `.gif` 用 `QMovie` 逐帧驱动，不再只显示第一帧 |
| **PDF 适应模式** | 「适合宽度 / 整页」一键切换（`QPdfView::ZoomMode`） |
| **异步文档解析** | docx / pptx 文本提取走 `QProcess` 异步 + 60 秒兜底，加载期间界面不再卡死 |
| **状态栏** | 深色主题常驻，可在视图菜单关闭；所有操作提示可见 |

---

## 5. 功能栏自动隐藏（当前状态：✅ 已完成）

### 行为规范（以界面内「使用说明」为准）

- **视频 / 音频面板**：鼠标移入 → topBar + ctrl 立即显示；**鼠标移出 → 立即隐藏**
- **图片 / PDF / 文本面板**：控制条**始终可见**（否则看不到标题栏，也就没法关闭/放大面板）
- 与播放状态**无关**，暂停时移出同样隐藏

> ⚠️ 历史遗留：早期版本曾设计过「播放中鼠标静止 2.5 秒才收起」，该方案**已废弃**，
> 本文档曾长期保留这段过期描述，导致 1.2.0 一度按错误需求实现，现已按界面「使用说明」改正。
> 修改本节前请先确认 `MainWindow::showHelp()` 里的文案。

### 实现原理
```cpp
// MediaPanel.h
QTimer m_pollTimer;        // 100ms + CoarseTimer，仅视频/音频面板启动
bool m_autoHideEnabled;    // 是否参与自动隐藏
bool m_barsVisible;
bool m_cursorInside;       // 鼠标是否在面板矩形内

// MediaPanel.cpp
void MediaPanel::pollMousePosition()
{
    if (!m_autoHideEnabled) return;
    const bool inside = rect().contains(mapFromGlobal(QCursor::pos()));
    if (inside == m_cursorInside) return;      // 状态没变就不折腾布局
    m_cursorInside = inside;
    updateBarsForCursor();
}

void MediaPanel::updateBarsForCursor()
{
    if (!m_autoHideEnabled) return;
    if (!isPlayableType(m_type)) { setBarsVisible(true); return; }  // 静态面板常驻
    setBarsVisible(m_cursorInside);
}

void MediaPanel::enterEvent(QEnterEvent *event)   // 移入立即响应，不等轮询
{
    QWidget::enterEvent(event);
    if (!m_autoHideEnabled || m_cursorInside) return;
    m_cursorInside = true;
    updateBarsForCursor();
}

void MediaPanel::leaveEvent(QEvent *event)        // 故意留空
{
    QWidget::leaveEvent(event);
}
```

### 为什么用轮询判断「移出」而不是 leaveEvent

Qt 在鼠标进入**子控件**（播放按钮、进度条、音量条）时，也会给父 Panel 发 `QEvent::Leave`。
如果直接在 `leaveEvent` 里隐藏控制条，就会出现"鼠标刚碰到播放键，整条控制条就消失"。
用 `rect().contains()` 判断坐标才是可靠的，代价是一个 100ms 的粗粒度定时器。

### 关键修复（1.2.0）

1.1.1 及之前，`MediaPanel` 构造函数末尾是无条件 `setBarsVisible(false)`，而 `m_pollTimer` 只在 `m_autoHideEnabled == true` 时启动，且**只有 `VideoPanel` 调用了 `enableAutoHide(true)`**。于是 `ImagePanel` / `PdfPanel` / `TextPanel` 的 topBar 和 ctrl **永远不可见**——用户既看不到文件名，也没有 ✕ 按钮，只能通过菜单"全部清除"退出。

修复：基类构造统一 `enableAutoHide(true)` + `setBarsVisible(true)`；静态面板不启动轮询定时器，控制条由 `updateBarsForCursor()` 强制常驻。

### 修复记录（Z-Order 战争）

**历史问题**：最早 VideoPanel 用的是 `QVideoWidget`，它在 Windows 上创建独立原生 HWND，视频画面永远盖在 topBar/ctrl 之上——Qt 的 `raise()` 只能影响 Qt widget 的逻辑 z-order，对原生 HWND 完全无效。

**第一版修复**（2026-10-16）：将 `QVideoWidget` 替换为 `QGraphicsView` + `QGraphicsScene` + `QGraphicsVideoItem`。`QGraphicsVideoItem` 不走独立 HWND，视频帧统一在 QGraphicsView 上用 QPainter 渲染，z-order 完全可控。

**问题暴露**：12 路视频同时播放时必卡到未响应。根因是 **QGraphicsVideoItem 是 CPU 软件渲染路径**——每帧 QMediaPlayer 解码 YUV → QGraphicsVideoItem 接收 → YUV→RGB 颜色空间转换 → QPainter::drawPixmap，全部阻塞在主线程。12 × 30fps = 每秒 360 次主线程重绘。而 QVideoWidget 走的是 **GPU D3D 硬件加速路径**，视频帧直接推到 GPU 显存，UI 线程零开销。

**最终方案**（2026-10-17）：恢复 `QVideoWidget`，用 **QVBoxLayout** 把 topBar / m_mediaArea / ctrl 垂直排列。三个区域**完全不重叠**——QVideoWidget 的 HWND 只覆盖中间 m_mediaArea，topBar 和 ctrl 在它上下方，天然不被盖住。无需任何 Win32 API hack。

| 方案 | 结论 |
|------|------|
| ~~纯 Qt Widget + raise()~~ | ❌ 视频 HWND 盖一切 |
| ~~topBar/ctrl 也加 WA_NativeWindow + SetWindowPos~~ | ❌ Qt stylesheet 在原生窗口上绘制异常 |
| ~~QGraphicsView + QGraphicsVideoItem~~ | ❌ CPU 软件渲染，12 路必卡 |
| ✅ **QVideoWidget + QVBoxLayout 垂直排列** | ✅ GPU 加速 + 无重叠区域，简单可靠 |

---

## 6. 修复记录 / 待改进项

### ✅ 已修复（2026-10-16 ~ 2026-10-17）

| # | 原问题 | 修复方案 | 改动文件 |
|---|--------|----------|----------|
| 1 | ~~🔴 视频 Panel 的 topBar/ctrl 被视频画面遮挡~~ | ~~临时方案~~ → **最终方案**：恢复 QVideoWidget（GPU 加速），改用 QVBoxLayout 垂直排列三个区域，完全不重叠 | VideoPanel.h / VideoPanel.cpp / MediaPanel.cpp |
| 2 | ~~🔴 自动隐藏逻辑无法验证~~ | 随 #1 自动修复：QVideoWidget HWND 只在 m_mediaArea 内，MediaPanel 正常接收事件 | 无额外改动 |
| 3 | ~~🟡 CustomSlider.h 存在但未使用~~ | 直接删除（未被任何代码 include） | CustomSlider.h（已删除） |
| 4 | ~~🟡 AudioPanel 重复的 eventFilter slider 逻辑~~ | 删除 AudioPanel 中 50+ 行重复实现，让继承链自动走 MediaPanel::eventFilter | AudioPanel.h / AudioPanel.cpp |
| 5 | ~~🔴 detachPanels splitter 正向迭代导致迭代器失效崩溃~~ | **所有嵌套 for 循环改为倒序** (`count()-1` 到 `0`)。`setParent(nullptr)` 会让 QSplitter 内部立刻移除该 widget，正向迭代时 `col->count()` 变化导致跳过部分 widget，下一行 `delete colSplitter` 时内部残留游离子对象 → SIGSEGV | MainWindow.cpp |
| 6 | ~~🟡 视频画面圆角不精确~~ | MediaPanel 的 setMask + QPainterPath 直接对整个 Panel 生效，包括视频画面 | MediaPanel.cpp |
| 7 | ~~🟡 没有持久化状态~~ | 新增 QSettings：窗口 geometry/state、循环播放偏好、音频面板可见性、上次打开的文件列表 | MainWindow.h / MainWindow.cpp |
| 8 | ~~🟡 没有进度记忆~~ | 关闭面板时保存播放位置（绝对路径→毫秒数）；打开同一文件时延迟 1.8s 后自动 seek 并暂停 | MainWindow.h / MainWindow.cpp |
| 9 | ~~🔴 历史记录不显示~~ | **根因两个**：(1) `main.cpp` 缺 `setOrganizationName` 导致 Windows QSettings 注册表路径不稳定；(2) `QSettings::setValue` 是延迟写入，刚写入就 `value()` 读不到。修复：加 organizationName + `s.sync()` 强制落盘 | main.cpp / MainWindow.cpp |
| 10 | ~~🟢 formatTime 不处理超 1 小时~~ | 超过 1 小时的视频显示 `H:MM:SS`，短视频保持 `M:SS` 格式 | MediaPanel.cpp |
| 11 | ~~🟡 鼠标移动时明显卡顿~~ | **两个性能黑洞**：(1) `PreciseTimer` 强制 Windows 时钟到 1ms 精度，4 面板 × 50次/秒调度；(2) ImagePanel 用 `Qt::SmoothTransformation` 大图缩放开销巨大。修复：改 CoarseTimer + 150ms 间隔；改 FastTransformation + 尺寸变化缓存 | MediaPanel.cpp / ImagePanel.h / ImagePanel.cpp |
| 12 | ~~🔴 关闭视频面板时闪退~~ | 根因同 #5：`detachPanels` 正向迭代 + VideoPanel 析构时的 `m_player->stop()` 阻塞叠加。倒序迭代后彻底解决 | MainWindow.cpp |
| 13 | ~~🔴 进度条点击跳转位置错误~~ | `QStyle::subControlRect` 在自定义 stylesheet 下返回完全错误的 groove 矩形（实测 `QRect(220,0 4x15)`），导致 ratio 永远被 clamp 到 0 或 1。修复：放弃 subControlRect，直接用 `me->pos().x() / m_progress->width()` 计算 | MediaPanel.cpp |
| — | 🆕 历史记录功能 | 新增"最近打开"菜单（上限 20 条）+ "清除历史"；三路数据同步：addPanel 实时写入 / saveSettings 关闭兜底 / loadSettings 旧数据迁移 | MainWindow.h / MainWindow.cpp |
| — | 🆕 ImagePanel 重写 | 去掉 SmoothTransformation，加 m_lastRenderSize 缓存避免同尺寸重复缩放 | ImagePanel.h / ImagePanel.cpp |
| — | 🆕 关闭面板流程优化 | `onPanelClosed` 中先 `stop()` 再 `setParent(nullptr)` 再 `relayout()` 最后 `deleteLater()`；确保 detach 和 delete 的顺序安全 | MainWindow.cpp |
| — | 🆕 **1.1.2 版本**：恢复 QVideoWidget GPU 硬件渲染 | QGraphicsVideoItem CPU 软件渲染导致 12 路视频必卡。恢复 QVideoWidget + D3D 路径，12 路视频流畅如 1.0 | VideoPanel.h / VideoPanel.cpp |
| — | 🆕 **1.1.2 版本**：MediaPanel 改用 QVBoxLayout | topBar/m_mediaArea/ctrl 垂直排列完全不重叠，QVideoWidget HWND 只覆盖中间区域，彻底消除原生 HWND 层级问题 | MediaPanel.cpp |
| — | 🆕 **1.1.2 版本**：隐藏底部无用状态栏 | QMainWindow::statusBar() 默认显示白色底部区域，占用空间无实际用途，改为隐藏 | MainWindow.cpp |
| — | 🆕 **1.1.2 版本**：智能静音 | 同时播放多个视频时，仅第一个有声音，其余自动静音，避免多路音频混乱 | MainWindow.cpp |
| 14 | 🔴 ~~图片/PDF/文本面板顶栏与底栏永久隐藏~~ | 基类构造改为统一 `enableAutoHide(true)` + `setBarsVisible(true)`，静态面板控制条常驻 | MediaPanel.cpp |
| 15 | 🔴 ~~自动隐藏对静态面板完全失效~~ | 基类统一启用；按「使用说明」实现「移入显示 / 移出隐藏」，静态面板常驻；删除废弃的 2.5s 倒计时逻辑 | MediaPanel.cpp |
| 16 | 🔴 ~~音频播完不循环~~ | `AudioPanel::onMediaStatusChanged` 实现 `EndOfMedia` 循环 | AudioPanel.cpp |
| 17 | 🔴 ~~音频悬浮条不跟随主窗口~~ | 新增 `anchorToParent()` + 主窗口 Resize/Move 事件里 `layoutAudioPanel()` | AudioPanel.cpp / MainWindow.cpp |
| 18 | 🔴 ~~拖动进度条时滑块回弹~~ | `m_seeking` 期间跳过 `onPositionChanged` 的滑块刷新 | VideoPanel.cpp / AudioPanel.cpp |
| 19 | 🔴 ~~视频循环失效~~ | `EndOfMedia` 改为先 `play()` 再 `setPosition(0)` | VideoPanel.cpp |
| 20 | 🔴 ~~窗口几何每次启动被重置~~ | 删除 `main.cpp` 里覆盖 `resize/move` 的代码，交给 `loadSettings()` | main.cpp |
| 21 | 🔴 ~~音频面板路径未持久化，从不恢复~~ | 新增 `audio/path` 键，启动时按路径重建 | MainWindow.cpp |
| 22 | 🟡 ~~静音图标被音量值覆盖~~ | 新增 `updateMuteIcon()`，静音按 `isMuted() || volume==0` 统一判断 | MediaPanel.cpp |
| 23 | 🟡 ~~退出/全部清除丢失未保存文本~~ | `closeEvent` 逐个确认；`clearAll` 二次确认 + 保存进度 | MainWindow.cpp |
| 24 | 🟡 ~~关闭最后一个面板残留空 splitter~~ | `onPanelClosed` 无条件 `relayout()` | MainWindow.cpp |
| 25 | 🟡 ~~状态栏被 hide，所有提示不可见~~ | 改深色样式常驻 + 视图菜单开关 | MainWindow.cpp |
| 26 | 🟡 ~~docx/pptx 同步解析卡死界面~~ | `QProcess` 异步 + 60s 兜底 + 加载期间只读 | TextPanel.cpp |
| 27 | 🟡 ~~GBK 文本乱码~~ | `QStringDecoder::hasError()` 严格判定，失败回退 `Encoding::System` | TextPanel.cpp |
| 28 | 🟡 ~~全局拖放过滤器吞掉所有拖放~~ | 只接管主窗口区域内的本地文件拖放，文本控件与其它区域放行 | MainWindow.cpp |
| 29 | 🟢 ~~音量条用 `subControlRect` 定位~~ | 改用 `me->pos().y() / height()` | MediaPanel.cpp |
| 30 | 🟢 ~~日志/图标写硬编码路径~~ | 改 `QStandardPaths::AppDataLocation`，日志重定向需 `MM_DEBUG=1` | main.cpp |
| 31 | 🟢 ~~快捷键只在文档里，未实装~~ | 实装 Ctrl+O / Ctrl+Shift+O / Space / ←→ / Ctrl+1~9 / Ctrl+0 / Ctrl+W / Ctrl+L / Ctrl+M / Esc | MainWindow.cpp |
| — | 🆕 **1.2.0**：倍速播放 | 0.5x~2.0x，已开面板立即生效，新面板继承，持久化 | MediaPanel/VideoPanel/AudioPanel/MainWindow |
| — | 🆕 **1.2.0**：图片面板重做 | `QGraphicsView` 滚轮缩放 + 拖拽平移 + 双击还原 + 超大图降采样 | ImagePanel.cpp |
| — | 🆕 **1.2.0**：GIF 动画 | `QMovie` 逐帧驱动 `QGraphicsPixmapItem` | ImagePanel.cpp |
| — | 🆕 **1.2.0**：PDF 适应模式 | 「适合宽度 / 整页」切换按钮 | PdfPanel.cpp |
| — | 🆕 **1.2.0**：定时器瘦身 | 仅视频/音频面板启动 150ms 鼠标轮询 | MediaPanel.cpp |

### ⚠️ 待改进

| # | 问题 | 影响 | 建议 |
|---|------|------|------|
| 1 | `detachPanels` + 立即 delete col splitters 是"硬重建"，每次关闭都销毁并新建整个 Splitter 树 | 关闭/添加面板时可能有轻微闪烁 | 改为增量移除（只 remove 一个 widget，不 destroy 整个 Splitter 树） |
| 2 | 每次 `relayout()` 都会把所有列的行高重置为均分 | 用户拖动过的行高在增删面板后丢失 | 重建前保存行高比例，重建后恢复 |
| 3 | 历史记录菜单是无图标纯 QAction | 视觉不够区分 | 给历史记录 Action 加文件类型图标（video / audio / image / pdf / text） |
| 4 | 只支持单实例，重复启动会开第二个窗口争抢 QSettings | 用户可能同时开两个 | 加 `QLockFile` / `QLocalServer` 单实例限制 |
| 5 | 没有字幕（.srt/.ass）支持，也没有画面旋转/比例调整 | 视频功能不完整 | 用 `QMediaPlayer` 配套接口或自绘字幕层 |
| 6 | 音视频解码依赖 Qt Multimedia 默认后端 | 部分编码（如 HEVC/AV1）可能无法播放 | 可考虑接入 libmpv 作为可选后端 |

### ✅ 1.2.0 已解决的前待改进项

| 原编号 | 问题 | 处理方式 |
|--------|------|----------|
| 旧 4 | AudioPanel 拖动逻辑与 MediaPanel 不一致 | 音频条按自身语义改为"拖标题区移动"，并新增 `mouseReleaseEvent` 结束拖拽 |
| 旧 9 | `main.cpp` 里 `QScreen` include 未使用 | 一并删除（连同不再需要的 `QGuiApplication`） |
| 旧 11 | 快捷键全局作用，多 Panel 时不知操作哪个 | 统一改为"作用于鼠标悬停的面板"（`QApplication::widgetAt` 向上找 `MediaPanel`），找不到才回退到最后一个面板 |

### 业务约束

| 参数 | 值 | 说明 |
|------|----|------|
| 最大同时加载 Panel | **12** | `m_maxPanels`，addFiles 超量时自动截断，状态栏提示 |
| 文件对话框默认 filter | **所有文件 (*)** | 用户自行选择要加载的文件；也可手动切换到"媒体文件"快捷过滤 |
| 文件夹扫描 | **单层** | `QDir::entryList(QDir::Files)`，不递归子目录 |
| 持久化存储路径 | QSettings 默认位置 | Windows 下为 `HKCU\Software\MultiMediaPlayer\MultiMediaPlayer\`（organizationName + applicationName） |
| 进度记忆过期条件 | 位置 ≥ 95% 时长不保存；恢复时位置 ≥ 90% 不恢复 | 避免卡在片尾 |
| 历史记录上限 | **20 条** | `MAX_RECENT_FILES`，超出自动淘汰最早的 |
| pollMousePosition 间隔 | **150ms + CoarseTimer** | 原来是 80ms + PreciseTimer，后者强制 Windows 时钟精度到 1ms 导致 CPU 调度压力大 |

---

## 7. 技术要点备忘

### 7.1 为什么 pollMousePosition 用 QCursor::pos() 而不是 eventFilter？

因为 **QVideoWidget 的原生 HWND 吃掉了所有 Qt 鼠标事件**。即使安装 eventFilter 到 MediaPanel 上，当鼠标在视频画面区域（QVideoWidget 的 HWND）里时，MediaPanel 完全收不到 `enterEvent/leaveEvent/mouseMoveEvent`。

`QCursor::pos()` 是操作系统级别的 API，直接读取当前屏幕坐标，完全绕过 Qt 事件系统。所以用 QTimer 每 150ms 主动读一次（CoarseTimer）。

### 7.2 QSlider 点击跳转的实现

QSlider 默认行为是：点击 groove 时把 thumb 移到最近的 tick 位置，而不是点击的精确位置。MediaPanel 通过 eventFilter 拦截 `MouseButtonPress`，手动计算点击比例。

**⚠️ 不要用 `QStyle::subControlRect`！** 在自定义 stylesheet（尤其是 QSS 里指定了 `::groove`、`::handle` 的 margin/padding 时），Qt 的 style 系统会返回错误的 groove 矩形（实测返回过 `QRect(220,0 4x15)` 这样完全离谱的值）。

**正确做法**：`QMouseEvent::pos()` 返回的坐标本身就是相对于 slider widget 的，直接除以 slider 自身宽度即可：

```cpp
// MediaPanel.cpp eventFilter() — 进度条点击跳转
if (watched == m_progress) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            double ratio = double(me->pos().x()) / double(m_progress->width());
            ratio = qBound(0.0, ratio, 1.0);
            int v = int(ratio * m_progress->maximum());
            m_progress->blockSignals(true);
            m_progress->setValue(v);
            m_progress->blockSignals(false);
            if (m_duration > 0) {
                seek(qint64(v) * m_duration / 1000);
                m_seeking = true;
                m_position = qint64(v) * m_duration / 1000;
                updateTimeLabel();
            }
            return true;  // 拦截事件，不让 QSlider 默认处理
        }
    }
    // MouseMove / MouseButtonRelease 同理...
}
```

### 7.3 分类逻辑

MainWindow::classify() 根据文件扩展名决定创建哪种 Panel：

```cpp
Video: .mp4 .avi .mkv .mov .wmv .flv .webm .m4v .ts
Audio: .mp3 .wav .flac .aac .ogg .m4a .wma
Image: .jpg .jpeg .png .bmp .gif .webp .svg
Pdf:   .pdf
Text:  .txt .md .log .xml .json .csv .ini .h .cpp .java ... (纯文本)
```

---

## 8. 第三方依赖

| 依赖 | 用途 |
|------|------|
| Qt 6.7 Multimedia | QMediaPlayer + QAudioOutput（视频/音频解码播放） |
| Qt 6.7 MultimediaWidgets | QVideoWidget（VideoPanel 视频渲染核心，Windows 上自动走 D3D GPU 加速路径） |
| Qt 6.7 Pdf | QPdfView（PDF 渲染） |
| Qt 6.7 Svg | SVG 图片支持 |
| FFmpeg (Qt Multimedia plugin) | Qt Multimedia 在 Windows 上默认用 FFmpeg 做后端 |