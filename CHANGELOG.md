# 全能媒体播放器 — 版本更新记录

格式: 基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)

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