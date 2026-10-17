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
├── VideoPanel.h/cpp    视频播放（QGraphicsView + QGraphicsVideoItem + QMediaPlayer）
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

#### 内部布局（关键！）

```
MediaPanel (QWidget)
│
├── m_mediaArea (QWidget)        ← 占满整个 panel，子类把自己的媒体 widget 放这里
│   └── (子类填充：QGraphicsView+VideoItem / QLabel / QPdfView / ...)
│
├── m_topBar (QWidget)           ← 手动 setGeometry 定位到顶部，z-order 最高
│   ├── m_titleLabel             ← 🎬 filename.mp4
│   ├── m_infoLabel              ← 1920x1080 / loading...
│   ├── m_amplifyBtn             ← ⛶ 放大/还原
│   └── m_closeBtn               ← ✕ 关闭
│
└── m_ctrl (QWidget)             ← 手动 setGeometry 定位到底部，z-order 最高
    ├── m_playBtn                ← ▶ / ⏸
    ├── m_stopBtn                ← ■
    ├── m_backBtn                ← ⏪ -5s
    ├── m_fwdBtn                 ← ⏩ +5s
    ├── m_progress (QSlider)     ← 进度条（支持点击跳转）
    ├── m_timeLabel              ← 0:13 / 3:52
    ├── m_muteBtn                ← 🔊 / 🔇
    └── m_volume (QSlider)       ← 音量 0-100
```

#### z-order 管理（血泪史）

topBar 和 ctrl **不是**通过 QVBoxLayout/QHBoxLayout 放在 MediaPanel 里的，而是**手动 setGeometry + raise()**。原因：

> **Qt Layout 的潜规则**：只要 widget 被 layout `addWidget` 了，layout 就会永久接管它的 geometry 和 z-order，每次 `resizeEvent` 都强制刷新。如果 MediaPanel 里有一个 QVBoxLayout 管理 m_mediaArea，那 layout 会把 m_mediaArea 压到最上层，盖住手动放的 topBar/ctrl。

所以 MediaPanel 的做法是：
1. **没有主 layout**。m_mediaArea、m_topBar、m_ctrl 都是 MediaPanel 的直接子 widget。
2. `resizeEvent` 里依次 `setGeometry`，然后对 topBar/ctrl 调用 `raise()`。
3. 子类（VideoPanel 等）对 m_mediaArea 内部可以自由加 layout，不影响 MediaPanel 外部。

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
| 降低定时器开销 | pollMousePosition 用 `CoarseTimer`（Windows 默认 15.6ms 时钟）替代 `PreciseTimer`，间隔 150ms，避免强制系统时钟精度 |

---

## 5. 功能栏自动隐藏（当前状态：✅ 已完成）

### 设计目标
- 鼠标在 Panel 内移动 → topBar 和 ctrl 显示
- 鼠标静止 2.5 秒 → topBar 和 ctrl 隐藏（仅播放状态）
- 暂停/停止 → bar 保持显示

### 实现原理
```cpp
// MediaPanel.h
QTimer m_hideTimer;       // 2.5s 单次定时器
QTimer m_pollTimer;       // 80ms 轮询鼠标位置（兜底，正常情况下 Qt 事件即可）
bool m_autoHideEnabled;
bool m_barsVisible;
QPoint m_lastMousePos;

// MediaPanel.cpp — pollMousePosition() 核心逻辑
void MediaPanel::pollMousePosition()
{
    QPoint pos = QCursor::pos();
    if (pos != m_lastMousePos) {           // 鼠标动了
        m_lastMousePos = pos;
        QPoint localPos = mapFromGlobal(pos);
        bool insidePanel = rect().contains(localPos);
        if (insidePanel) {
            if (!m_barsVisible) setBarsVisible(true);  // 显示 bar
            if (m_playing) m_hideTimer.start(2500);     // 重启倒计时
        }
    }
}
```

### 修复记录（原 Z-Order 战争）

**历史问题**：最早 VideoPanel 用的是 `QVideoWidget`，它在 Windows 上创建独立原生 HWND，视频画面永远盖在 topBar/ctrl 之上——Qt 的 `raise()` 只能影响 Qt widget 的逻辑 z-order，对原生 HWND 完全无效。

**修复方案**（2026-10-16 实施）：将 `QVideoWidget` 替换为 `QGraphicsView` + `QGraphicsScene` + `QGraphicsVideoItem`。`QGraphicsVideoItem` 不走独立 HWND，视频帧作为图像帧交给 `QGraphicsScene` 统一渲染，所有 item 都在同一个渲染表面上，z-order 完全可控。

**影响范围**：
- VideoPanel.h/cpp：完全重写
- 其他 Panel：不受影响，保持原有实现

**为什么 pollMousePosition 现在仍然保留？**
修复后 QGraphicsView 是普通 Qt widget，不再有原生 HWND，Qt 的 `enterEvent/leaveEvent/mouseMoveEvent` 可以正常传递。但保留 80ms 轮询作为**兜底机制**，防止某些边缘场景下 Qt 事件丢失（如窗口被拖拽时鼠标短暂移出 Panel 再快速移回）。

### 原方案调研（保留供参考）

| 方案 | 结论 |
|------|------|
| ~~纯 Qt Widget + raise()~~ | ❌ 视频 HWND 盖一切 |
| ~~topBar/ctrl 也加 WA_NativeWindow + SetWindowPos~~ | ❌ Qt 内部有时打乱顺序 |
| ✅ **QGraphicsView + QGraphicsVideoItem** | ✅ 已采用，z-order 100% 可控 |
| 独立顶层窗口 + Qt::ToolTip | 未尝试，实现复杂 |
| overlay 叠加 Qt widget | 实测无效 |

---

## 6. 修复记录 / 待改进项

### ✅ 已修复（2026-10-16 ~ 2026-10-17）

| # | 原问题 | 修复方案 | 改动文件 |
|---|--------|----------|----------|
| 1 | ~~🔴 视频 Panel 的 topBar/ctrl 被视频画面遮挡~~ | VideoPanel 重写为 QGraphicsView + QGraphicsVideoItem，消除原生 HWND 层级问题 | VideoPanel.h / VideoPanel.cpp |
| 2 | ~~🔴 自动隐藏逻辑无法验证~~ | 随 #1 自动修复：QGraphicsView 是普通 Qt widget，事件系统正常工作 | 无额外改动 |
| 3 | ~~🟡 CustomSlider.h 存在但未使用~~ | 直接删除（未被任何代码 include） | CustomSlider.h（已删除） |
| 4 | ~~🟡 AudioPanel 重复的 eventFilter slider 逻辑~~ | 删除 AudioPanel 中 50+ 行重复实现，让继承链自动走 MediaPanel::eventFilter | AudioPanel.h / AudioPanel.cpp |
| 5 | ~~🔴 detachPanels splitter 正向迭代导致迭代器失效崩溃~~ | **所有嵌套 for 循环改为倒序** (`count()-1` 到 `0`)。`setParent(nullptr)` 会让 QSplitter 内部立刻移除该 widget，正向迭代时 `col->count()` 变化导致跳过部分 widget，下一行 `delete colSplitter` 时内部残留游离子对象 → SIGSEGV | MainWindow.cpp |
| 6 | ~~🟡 视频画面圆角不精确~~ | 随 #1 自动修复：QGraphicsView 是普通 Qt widget，MediaPanel 的 setMask 圆角裁剪直接生效 | 无额外改动 |
| 7 | ~~🟡 没有持久化状态~~ | 新增 QSettings：窗口 geometry/state、循环播放偏好、音频面板可见性、上次打开的文件列表 | MainWindow.h / MainWindow.cpp |
| 8 | ~~🟡 没有进度记忆~~ | 关闭面板时保存播放位置（绝对路径→毫秒数）；打开同一文件时延迟 1.8s 后自动 seek 并暂停 | MainWindow.h / MainWindow.cpp |
| 9 | ~~🔴 历史记录不显示~~ | **根因两个**：(1) `main.cpp` 缺 `setOrganizationName` 导致 Windows QSettings 注册表路径不稳定；(2) `QSettings::setValue` 是延迟写入，刚写入就 `value()` 读不到。修复：加 organizationName + `s.sync()` 强制落盘 | main.cpp / MainWindow.cpp |
| 10 | ~~🟢 formatTime 不处理超 1 小时~~ | 超过 1 小时的视频显示 `H:MM:SS`，短视频保持 `M:SS` 格式 | MediaPanel.cpp |
| 11 | ~~🟡 鼠标移动时明显卡顿~~ | **两个性能黑洞**：(1) `PreciseTimer` 强制 Windows 时钟到 1ms 精度，4 面板 × 50次/秒调度；(2) ImagePanel 用 `Qt::SmoothTransformation` 大图缩放开销巨大。修复：改 CoarseTimer + 150ms 间隔；改 FastTransformation + 尺寸变化缓存 | MediaPanel.cpp / ImagePanel.h / ImagePanel.cpp |
| 12 | ~~🔴 关闭视频面板时闪退~~ | 根因同 #5：`detachPanels` 正向迭代 + VideoPanel 析构时的 `m_player->stop()` 阻塞叠加。倒序迭代后彻底解决 | MainWindow.cpp |
| — | 🆕 历史记录功能 | 新增"最近打开"菜单（上限 20 条）+ "清除历史"；三路数据同步：addPanel 实时写入 / saveSettings 关闭兜底 / loadSettings 旧数据迁移 | MainWindow.h / MainWindow.cpp |
| — | 🆕 ImagePanel 重写 | 去掉 SmoothTransformation，加 m_lastRenderSize 缓存避免同尺寸重复缩放 | ImagePanel.h / ImagePanel.cpp |
| — | 🆕 关闭面板流程优化 | `onPanelClosed` 中先 `stop()` 再 `setParent(nullptr)` 再 `relayout()` 最后 `deleteLater()`；确保 detach 和 delete 的顺序安全 | MainWindow.cpp |

### ⚠️ 待改进

| # | 问题 | 影响 | 建议 |
|---|------|------|------|
| 4 | AudioPanel 的拖动逻辑 vs MediaPanel 的拖拽接口不一致 | 两个 Panel 行为不一致 | 统一拖拽接口 |
| 9 | `main.cpp` 里 `QScreen` include 了但没用 | 编译 warning | 删除无用 include |
| 11 | 快捷键定义在 MainWindow 里，是全局的，多 Panel 激活状态不明 | 可能操作错 Panel | 增加 Panel 激活机制，或让快捷键只作用于鼠标悬停的 Panel |
| 13 | `detachPanels` + 立即 delete col splitters 是"硬重建"，每次关闭都销毁并新建整个 Splitter 树 | 关闭/添加面板时可能有轻微闪烁 | 改为增量移除（只 remove 一个 widget，不 destroy 整个 Splitter 树） |
| 14 | 历史记录菜单是无图标纯 QAction，视觉不够区分 | 用户体验 | 给历史记录 Action 加文件类型图标（video / audio / image / pdf / text） |

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

`QCursor::pos()` 是操作系统级别的 API，直接读取当前屏幕坐标，完全绕过 Qt 事件系统。所以用 QTimer 每 80ms 主动读一次。

### 7.2 QSlider 点击跳转的实现

QSlider 默认行为是：点击 groove 时把 thumb 移到最近的 tick 位置，而不是点击的精确位置。MediaPanel 通过 eventFilter 拦截 `MouseButtonPress`，手动计算点击点在 groove 上的比例：

```cpp
// MediaPanel.cpp eventFilter()
QRect groove = sl->style()->subControlRect(..., QStyle::SC_SliderGroove, sl);
QRect handle = sl->style()->subControlRect(..., QStyle::SC_SliderHandle, sl);
double ratio = double(clickPos - groove.x() - handle.width()/2)
             / double(groove.width() - handle.width());
ratio = qBound(0.0, ratio, 1.0);
sl->setValue(int(sl->minimum() + ratio * (sl->maximum() - sl->minimum())));
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
| Qt 6.7 Multimedia | QMediaPlayer + QAudioOutput + QGraphicsVideoItem（视频/音频解码播放） |
| Qt 6.7 MultimediaWidgets | 提供 QGraphicsVideoItem（VideoPanel 视频渲染核心） |
| Qt 6.7 Pdf | QPdfView（PDF 渲染） |
| Qt 6.7 Svg | SVG 图片支持 |
| FFmpeg (Qt Multimedia plugin) | Qt Multimedia 在 Windows 上默认用 FFmpeg 做后端 |