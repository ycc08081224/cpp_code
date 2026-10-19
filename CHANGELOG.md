# 全能媒体播放器 — 版本更新记录

格式: 基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)

---

## [1.2.1] — 2026-10-19

### 修复

- 🔴 **悬停音量图标看不到音量条** — 原来的音量条是 Panel 内部的浮动子控件，而 `QVideoWidget` 在 Windows 上是**原生 HWND**：任何画在视频区域上的 Qt 控件都会被它盖住（`raise()` 只影响 Qt 的逻辑 z-order，对 HWND 无效），给浮层加 `WA_NativeWindow` 也一样不行（同级子窗口的 z 序 Qt 管不着）。修复：浮层改成**无父的顶层窗口**（`Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus`），用屏幕坐标定位在音量按钮正上方；顶层窗口永远浮在普通窗口之上，实测能正常绘制在视频画面之上
- 🔴 **静音被其他面板连累** — `playAll()` 里有一段 `firstVideoAudio` 逻辑：第一个视频 `setMuted(false)`、其余 `setMuted(true)`，于是第 2 个视频一播，第 1 个就被静音。修复：`playAll()` 只管播放，**不再改任何面板的静音状态**；每个面板的静音按钮只影响自己，`muteToggled` 信号仅用于把菜单「全部静音」的勾选状态与实际情况对齐（菜单项本身仍是显式的全局操作）
- 🟡 **音量浮层配色** — 红底改成黑底（`#000000` + 半透明白边），滑块与已填充部分保持品牌红，不再和播放器整体深色界面割裂

---

## [1.2.0] — 2026-10-18

### 修复

- 🔴 **图片 / PDF / 文本面板的标题栏和控制条永久隐藏** — `MediaPanel` 构造末尾无条件 `setBarsVisible(false)`，而 `m_pollTimer` 只在 `m_autoHideEnabled` 为真时才跑，且只有 `VideoPanel` 调用了 `enableAutoHide(true)`。结果这三类面板打开后**既看不到文件名，也没有 ✕ 关闭按钮**，只能靠菜单"全部清除"退出。修复：基类统一 `enableAutoHide(true)` + `setBarsVisible(true)`
- 🔴 **自动隐藏对静态面板不生效** — 只有 `VideoPanel` 调用了 `enableAutoHide(true)`，图片/PDF/文本面板的轮询从未运行。修复：基类统一启用；播放类面板「鼠标移入显示 / 移出隐藏」，静态面板控制条常驻（以界面内「使用说明」为准，见 `dev_document.md` §5 的历史遗留说明）
- 🔴 **音频面板播完就停，不循环** — `AudioPanel::onMediaStatusChanged` 是空实现，`m_loop` 从未生效。修复：`EndOfMedia` 时循环重播
- 🔴 **音频悬浮条不跟随主窗口** — 位置只在 `addPanel` 时算一次，之后拖动/缩放窗口它就停在原地甚至跑到窗口外。修复：新增 `AudioPanel::anchorToParent()`，主窗口 Resize/Move 时重新贴底居中
- 🔴 **拖动进度条时滑块回弹** — `onPositionChanged` 无条件覆盖滑块位置，用户拖到一半就被播放进度抢回去。修复：`m_seeking` 期间跳过位置刷新（视频/音频都改）
- 🔴 **视频循环播放失效** — `EndOfMedia` 状态下 `setPosition(0)` 在部分后端无效，导致停在片尾。修复：先 `play()` 再 `setPosition(0)`
- 🔴 **窗口位置/大小持久化被覆盖** — `MainWindow` 构造函数里 `loadSettings()` 刚恢复完几何，`main.cpp` 又强制 `resize(90%)` + `move(居中)`，等于每次启动都重置。修复：删除 `main.cpp` 中的覆盖逻辑
- 🔴 **音频面板从不恢复** — `saveSettings` 只保存 `m_panels` 的路径，音频面板不在此列，`loadSettings` 里判断 `m_audioPanel` 时它必然是 null。修复：新增 `audio/path` 键单独持久化
- 🟡 **静音图标状态错乱** — `onVolumeChanged` 用 `value == 0` 推断静音，与 `setMuted()` 的真实状态打架（静音后拖一下音量，图标就变回有声）。修复：新增 `updateMuteIcon()` 统一判断
- 🟡 **终止程序 / 全部清除时丢失未保存文本** — 直接 `deleteLater()`，改过的内容静默消失。修复：`closeEvent` 逐个确认；`clearAll` 增加二次确认并保存播放进度
- 🟡 **关闭最后一个面板后残留空拖拽条** — `onPanelClosed` 里 `if (!m_panels.isEmpty())` 才 `relayout()`，空列表时 `detachPanels()` 不执行，空的列 splitter 越堆越多。修复：无条件 `relayout()`
- 🟡 **状态栏提示全部不可见** — 1.1.2 用 `statusBar()->hide()` 解决了白色底色，但代码里几十处 `showMessage`（加载失败/已达上限/恢复进度…）随之全部失效。修复：改为深色样式常驻，视图菜单可关闭
- 🟡 **Word / PPT 文档打开时界面卡死** — `TextPanel` 构造里同步 `waitForFinished(45000)`，打开 docx 期间整个窗口无响应最多 45 秒。修复：改为 `QProcess` 异步 + 60 秒兜底，加载期间只读并显示"正在提取"
- 🟡 **GBK 文本显示乱码** — 原来先按 UTF-8 读、`isEmpty()` 判断失败再读本地编码，而 UTF-8 解码失败会产生替换字符（非空），判据失效。修复：用 `QStringDecoder` 的 `hasError()` 严格判断
- 🟡 **全局拖放过滤器吞掉一切拖放事件** — `eventFilter` 装在整个 `qApp` 上，对每个 widget 的 DragEnter/DragMove/Drop 都 `return true`，导致无法往文本编辑器拖文字、文件对话框内拖拽异常。修复：仅接管主窗口区域内的"本地文件"拖放，其余放行
- 🟢 **音量条点击定位用 `QStyle::subControlRect`** — 与 1.1.1 修的进度条是同一个坑，自定义 stylesheet 下返回的 groove 矩形不可信。修复：改用 `me->pos().y() / height()`
- 🟢 **日志与图标写到硬编码路径** — `main.cpp` 里 `D:/gongzuo/.../build/crash.log` 发布版必定写失败；每次 qDebug 都开关文件。修复：改 `QStandardPaths::AppDataLocation`，日志重定向仅在 `MM_DEBUG=1` 时启用
- 🟢 **PDF / 图片 / 文本面板残留无意义控件** — 静音按钮、播放列表按钮、音量条仍在。修复：全部隐藏
- 🟢 **音量调节双重 seek（音频面板）** — `sliderMoved` 和 `valueChanged` 都连了 `seek()`。修复：去掉 `valueChanged`

### 新增

- ⌨️ **实装帮助里承诺过但一直没写的快捷键** — `Ctrl+O` 添加文件、`Ctrl+Shift+O` 添加文件夹、`Space` 播放/暂停（鼠标所在面板）、`←/→` 前后 5 秒、`Ctrl+1~9` 直接设定放大比例、`Ctrl+0` 还原、`Ctrl+W` 关闭鼠标所在面板、`Ctrl+L` 播放列表、`Ctrl+M` 音频悬浮窗、`Esc` 退出全屏
- ⏩ **倍速播放** — 播放菜单新增 0.5x / 0.75x / 1.0x / 1.25x / 1.5x / 2.0x，已打开的面板立即生效，新面板继承当前速度，随 `preferences/rate` 持久化
- 🖼️ **图片面板重做为 QGraphicsView** — 鼠标滚轮缩放、按住拖动平移、双击还原适应窗口；超大图先降采样到 3840 再显示
- 🎞️ **GIF 动画播放** — `.gif` 改用 `QMovie` 逐帧驱动，原来只显示第一帧
- 📄 **PDF 面板新增"适合宽度 / 整页"切换按钮**
- 🖥️ **视图菜单** — 新增"显示/隐藏播放列表""还原面板尺寸""显示状态栏"三项
- 📊 **状态栏深色主题** — 恢复所有操作提示的可见性

### 优化

- `m_pollTimer` 只对可播放面板（视频/音频）启动，静态面板不再空转定时器；间隔 100ms、类型 `CoarseTimer`
- 删除 `m_hideTimer` / `restartHideCountdown()` / `onHideTimeout()` 这组「2.5 秒静止自动收起」的废弃逻辑（`m_hideTimer` 原本就是个从未 connect 的死对象）
- 关闭面板/全部清除时先 `stop()` 并保存进度，避免退出瞬间残留声音
- `MainWindow` 菜单勾选状态（循环、倍速）与实际设置双向同步，不再出现"设置是关的但菜单是勾的"
- 全屏状态随 `window/fullScreen` 恢复

---

## [1.1.1] — 2026-10-17

### 修复

- 🔴 **进度条点击无法跳转到正确位置** — `QStyle::subControlRect` 在自定义 stylesheet 下返回了完全错误的 groove 矩形（`QRect(220,0 4x15)`），导致点击 x<220 → ratio 被 clamp 到 0 → seek(0)，点击 x≥224 → ratio 直接变 1 → seek(duration)，呈现"瞬间变满又瞬间回到开头"的症状。修复：放弃 subControlRect，直接用 `me->pos().x() / m_progress->width()` 计算点击比例，因为 QMouseEvent::pos() 本身就是相对于 slider 的坐标，完全不受 QStyle 影响

---

## [1.1.0] — 2026-10-17

### 新增

- 📂 **最近打开历史记录** — 菜单 [文件 → 最近打开] 展示最近 20 条加载过的文件，点击即加载；支持 [清除历史]；自动清理已删除文件；关闭程序时自动把当前打开文件合并进历史；旧版本 `files/lastFiles` 首次启动时自动迁移
- 🖼️ **高性能图片渲染** — ImagePanel 从 `Qt::SmoothTransformation`（双三次滤波）改为 `Qt::FastTransformation`（最近邻），同时增加 `m_lastRenderSize` 缓存，尺寸不变时跳过重复缩放
- 🧭 **更完整的快捷键说明** — 帮助对话框补充了 Space 播放暂停、Ctrl+Shift+O 添加文件夹、Ctrl+1~9 放大切换、Ctrl+M 音频悬浮窗等

### 修复

- 🔴 **关闭面板闪退（根因：detachPanels 迭代器失效）** — `QSplitter::setParent(nullptr)` 会立刻把 widget 从 splitter 内部列表移除，正向迭代时 `col->count()` 变小导致跳过部分 widget，后续 `delete colSplitter` 时内部残留的游离子对象触发 SIGSEGV。修复：所有嵌套 for 循环改为倒序迭代
- 🔴 **历史记录不显示** — 两个根因叠加：(1) `main.cpp` 缺少 `app.setOrganizationName()` 导致 Windows 下 QSettings 注册表路径不稳定；(2) `QSettings::setValue` 是延迟写入，刚写入紧接着 `value()` 读不到。修复：补 organizationName + `s.sync()` 强制落盘
- 🟡 **鼠标移动明显卡顿** — 两个性能黑洞：(1) `pollMousePosition` 用 `Qt::PreciseTimer` 强制 Windows 系统时钟到 1ms 精度，4 面板并发导致 UI 线程每秒被调度 50+ 次；(2) ImagePanel 大图 SmoothTransformation 每次 resize 消耗 CPU。修复：改 `CoarseTimer` + 150ms 间隔；改 FastTransformation + 尺寸缓存
- 🟡 **关闭流程顺序隐患** — 原 `onPanelClosed` 在 `deleteLater()` 后才 `setParent(nullptr)` + `relayout()`，有极小概率 splitter 重建撞上待删 panel。修复：`stop()` → `setParent(nullptr)` → `relayout()` → `deleteLater()`

### 技术重构

- `main.cpp` 添加 `app.setOrganizationName("MultiMediaPlayer")`，统一 QSettings 存储路径为 `HKCU\Software\MultiMediaPlayer\MultiMediaPlayer\`
- ImagePanel.h/cpp 完全重写：去掉 SmoothTransformation，增加 m_lastRenderSize 缓存
- pollMousePosition 定时器间隔从 80ms 调整到 150ms，从 PreciseTimer 改为默认 CoarseTimer

### 新增开发者调试工具

- `main.cpp` 加入 crash signal handler（SIGSEGV/SIGABRT/SIGFPE/SIGILL）和 qDebug 日志重定向到 `build/crash.log`，崩溃时自动记录到文件。调试完毕可移除

---

## [1.0.0] — 2026-10-16

### 新增

- 🎬 **多面板并排媒体播放** — 最多 12 个 Panel 同时显示，视频/音频/图片/PDF/文本混合并排
- 🎥 **VideoPanel 基于 QGraphicsView + QGraphicsVideoItem** — 完全规避了 QVideoWidget 原生 HWND 盖过顶层控件的 z-order 问题
- 🎵 **AudioPanel 悬浮窗口** — 独立悬浮窗口显示音频信息，跟随主窗口移动
- 🖱️ **功能栏自动隐藏** — 鼠标静止 2.5 秒后顶部标题栏 + 底部控制条自动隐藏，播放状态下生效
- 📥 **文件拖拽** — 从资源管理器拖文件/文件夹到主窗口即加载
- ⏱️ **进度记忆** — 关闭面板时保存绝对路径→播放位置（毫秒），下次打开同一文件时自动恢复（≥95% 时长不保存，≥90% 不恢复）
- 💾 **持久化状态** — 关闭后重开恢复窗口位置/大小/最大化/全屏、循环偏好、上次打开文件列表
- 🔊 **全面板控制** — 菜单"播放全部/暂停全部/停止全部/全部静音/循环播放"一键操作
- 🖼️ **圆角外观** — MediaPanel 整体圆角 14px，通过 `setMask` + QPainterPath 实现
- ⛶ **单面板 4 级放大** — 原始 → 1/4 → 1/3 → 1/2 宽度循环切换
- 📊 **QSlider 点击跳转** — eventFilter 拦截 MouseButtonPress 手动计算 groove 比例，实现点击任意位置精确跳转

### 修复

- 🔴 VideoPanel 顶栏/控制栏被视频画面遮挡（z-order 被原生 HWND 覆盖）
- 🟡 AudioPanel 重复的 eventFilter slider 逻辑（50+ 行冗余，与 MediaPanel 基类重复）
- 🟡 文件夹扫描递归子目录（改为单层扫描）
- 🟡 formatTime 不处理超 1 小时（短视频 M:SS，长视频 H:MM:SS）
- 🟡 CustomSlider.h 存在但未被任何代码引用（已删除）

---

### 版本号规则

- **主版本号**：不兼容的架构变更（如重写渲染引擎）
- **次版本号**：新增功能 + bug 修复（当前节奏 ~2-3 天一个次版本）
- **修订号**：纯 bug 修复，无新功能