# OBSIM Code Wiki

> **OBSIM** (OBS IMitation) 是一个参考 OBS (Open Broadcaster Software) 架构实现的直播推流软件，支持直播录制、RTMP 推流、多源输入和多场景切换。

---

## 目录

1. [项目概述](#1-项目概述)
2. [整体架构](#2-整体架构)
3. [项目目录结构](#3-项目目录结构)
4. [构建与运行](#4-构建与运行)
5. [核心层 (Core Layer)](#5-核心层-core-layer)
6. [前端层 (Frontend Layer)](#6-前端层-frontend-layer)
7. [工具层 (Utility Layer)](#7-工具层-utility-layer)
8. [测试体系](#8-测试体系)
9. [依赖关系总览](#9-依赖关系总览)
10. [关键设计模式与数据流](#10-关键设计模式与数据流)

---

## 1. 项目概述

| 属性 | 描述 |
|------|------|
| **项目名称** | OBSIM (OBS IMitation) |
| **编程语言** | C++20 |
| **UI 框架** | Qt 6.9.0 |
| **多媒体引擎** | FFmpeg 7.1.1 |
| **图像处理** | OpenCV 4.12.0 |
| **测试框架** | Google Test (v1.15.2) |
| **构建系统** | CMake 3.24+ |
| **支持平台** | Windows (主要), macOS |
| **核心功能** | 屏幕/摄像头采集、音频采集(系统+麦克风)、多场景管理、视频滤镜、本地录制、RTMP 推流 |

### 功能特性

- 多输入源支持：屏幕采集、摄像头采集、图片源、文字源
- 多场景管理：独立场景切换，每场景独立源列表
- 实时音频采集：系统音频 (WASAPI) + 麦克风音频 (DShow)
- 音量控制与混音：独立音轨音量、静音控制，实时电平显示
- 视频滤镜：OpenCV 实现的翻转、灰度、亮度/对比度/饱和度调整
- 本地录制：MP4 文件输出 (H.264 + AAC)
- RTMP 推流：直播推送到指定 RTMP 服务器
- 源配置持久化：通过 QSettings 完成场景/源/音频设置的异步读写
- 深色主题：全局 QSS 样式表定义的深色 UI

---

## 2. 整体架构

项目采用经典的三层架构设计，自底向上分为 **工具层 (Utils)**、**核心层 (Core)** 和 **前端层 (Frontend)**：

```
OBSIM 架构层次
══════════════════════════════════════════════════════

┌────────────────────────────────────────────────────┐
│               Frontend Layer (前端层)                │
│  ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ MainWindow│ │ControlBar│ │ScenePreviewWidget │  │
│  │  (主窗口) │ │ (控制栏) │ │ (OpenGL 预览)     │  │
│  └────┬─────┘ └────┬─────┘ └────────┬──────────┘  │
│       │            │                │              │
│  ┌────┴────┐ ┌─────┴──────┐ ┌──────┴────────┐    │
│  │SettingBar│ │FilterPrev.│ │SettingsPrev.  │    │
│  │ (设置栏) │ │ (滤镜预览)│ │ (源设置预览)  │    │
│  └─────────┘ └────────────┘ └───────────────┘    │
├────────────────────────────────────────────────────┤
│               Core Layer (核心层)                   │
│  ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │  Source   │ │  Scene   │ │  AudioManager     │  │
│  │ (源基类)  │ │ (场景)   │ │ (音频管理器)      │  │
│  └────┬─────┘ └──────────┘ └────────┬──────────┘  │
│       │                             │              │
│  ┌────┴─────────────────────────────┴──────────┐  │
│  │   VideoCaptor     AudioCaptor    Recoder    │  │
│  │  (视频采集基类)  (音频采集基类)  (录制基类)  │  │
│  └────┬─────────────────┬──────────┬───────────┘  │
│       │                 │          │              │
│  ┌────┴────┐  ┌─────────┴───┐ ┌───┴──────────┐   │
│  │ScreenCap│  │SystemAudio  │ │FileRecoder   │   │
│  │Camera   │  │MicAudio     │ │StreamPush    │   │
│  └─────────┘  └─────────────┘ └──────────────┘   │
│  ┌──────────────────────────────────────────────┐ │
│  │         Filter (滤镜模块)                      │ │
│  │  OpenCVFilter  +  FilteredVideoCaptor         │ │
│  └──────────────────────────────────────────────┘ │
├────────────────────────────────────────────────────┤
│              Utility Layer (工具层)                 │
│  ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │DeviceMgr │ │ConfigMgr │ │DataSafeQueue      │  │
│  │ (设备枚举)│ │(配置管理)│ │(线程安全队列)     │  │
│  └──────────┘ └──────────┘ └───────────────────┘  │
│  ┌──────────┐ ┌──────────┐                        │
│  │FFmpegFact│ │av_err2str│ (RAII封装/错误处理)    │
│  └──────────┘ └──────────┘                        │
└────────────────────────────────────────────────────┘
```

### 层间依赖关系

- **Frontend → Core**：前端通过组合方式持有 Core 层的对象（如 `MainWindow` 持有 `AudioManager`、`ScenePreviewWidget` 持有 `Scene` 和 `Source`）
- **Core → Utils**：Core 层大量使用 Utils 层的工具类（如 `DataSafeQueue` 用于帧传递，`FFmpegFactory` 提供 RAII 封装）
- **Frontend → Utils**：前端直接使用 `DeviceManager`、`ConfigManager` 等工具

---

## 3. 项目目录结构

```
OBSIM/
├── main.cpp                         # 应用入口点
├── CMakeLists.txt                   # 顶层构建配置
├── CODE_WIKI.md                     # 本文件 (Code Wiki)
├── README.md                        # 项目简介
├── .gitignore
├── resources/
│   ├── resources.qrc                # Qt 资源文件
│   └── style.qss                    # 全局 QSS 深色主题样式表
├── src/
│   ├── core/                        # [核心层] 业务逻辑
│   │   ├── base/                    # 基类
│   │   │   ├── source.h/.cpp        #   源基类 (Source)
│   │   │   ├── videocaptor.h/.cpp   #   视频采集基类 (VideoCaptor)
│   │   │   ├── audiocaptor.h/.cpp   #   音频采集基类 (AudioCaptor)
│   │   │   └── recoder.h/.cpp       #   录制编码基类 (Recoder)
│   │   ├── filter/                  # 滤镜模块
│   │   │   ├── opencvfilter.h/.cpp  #   OpenCV 滤镜
│   │   │   └── filteredvideocaptor.h/.cpp  # 滤镜视频采集器
│   │   ├── audiomanager.h/.cpp      # 音频管理器
│   │   ├── camera.h/.cpp            # 摄像头设备
│   │   ├── cameracapturesource.h/.cpp # 摄像头采集源
│   │   ├── filerecoder.h/.cpp       # 文件录制器
│   │   ├── imagesource.h/.cpp       # 图片源
│   │   ├── micaudiocaptor.h/.cpp    # 麦克风采集器
│   │   ├── scene.h/.cpp             # 场景管理器
│   │   ├── screencaptor.h/.cpp      # 屏幕采集器
│   │   ├── screencapturesource.h/.cpp # 屏幕采集源
│   │   ├── streampush.h/.cpp        # RTMP 推流器
│   │   ├── systemaudiocaptor.h/.cpp # 系统音频采集器 (WASAPI)
│   │   ├── textsource.h/.cpp        # 文字源
│   │   └── videosource.h/.cpp       # 视频源
│   ├── frontend/                    # [前端层] Qt UI
│   │   ├── base/
│   │   │   └── previewbasewidget.h/.cpp  # 预览控件基类
│   │   ├── test/
│   │   │   └── testsource.h         # 测试用源
│   │   └── widget/
│   │       ├── mainwindow.h/.cpp    # 主窗口
│   │       ├── controlbar.h/.cpp    # 控制栏 (场景/源/混音/录制)
│   │       ├── settingbar.h/.cpp    # 设置栏
│   │       ├── scenepreviewwidget.h/.cpp # 场景预览 (OpenGL)
│   │       ├── settingsdialog.h/.cpp     # 设置对话框
│   │       ├── settingspreviewwidget.h/.cpp # 源设置预览
│   │       └── filterpreviewwidget.h/.cpp   # 滤镜预览
│   └── utils/                       # [工具层]
│       ├── PCH.h                    # 预编译头
│       ├── configmanager.h          # 异步配置管理器
│       ├── datasafequeue.h          # 线程安全队列
│       ├── devicemanager.h/.cpp     # 设备枚举器
│       ├── ffmpegfactory.h          # FFmpeg RAII 封装
│       └── av_err2str_cxx.h         # FFmpeg 错误字符串工具
└── test/                            # 单元测试
    ├── CMakeLists.txt               # 测试构建配置
    ├── test_audiomanager.cpp        # 音频管理器测试
    ├── test_configmanager.cpp       # 配置管理器测试
    ├── test_datasafequeue.cpp       # 线程安全队列测试
    ├── test_opencv_filter.cpp       # OpenCV 滤镜测试 (旧)
    ├── test_opencvfilter_gtest.cpp  # OpenCV 滤镜 GTest
    ├── test_recoder.cpp             # 录制器测试
    ├── test_scene_state.cpp         # 场景状态机测试
    ├── test_source_geometry.cpp     # 源几何测试
    └── test_videocaptor.cpp         # 视频采集器测试
```

---

## 4. 构建与运行

### 环境要求

| 依赖 | 版本 | 用途 |
|------|------|------|
| CMake | ≥ 3.24 | 构建系统 |
| Qt | ≥ 6.9.0 | GUI 框架 |
| FFmpeg | ≥ 7.1.1 | 音视频编解码与采集 |
| OpenCV | ≥ 4.12.0 | 图像滤镜处理 |
| OpenGL | - | 3D 渲染加速 |
| Google Test | v1.15.2 (自动拉取) | 单元测试 |
| Visual Leak Detector | - | 内存泄漏检测 (Windows Debug) |

### 构建步骤 (Windows / MSVC)

```bash
# 1. 配置 CMake 路径 (已在 CMakeLists.txt 中指定)
#    Qt:    D:/application/Qt/6.9.0/msvc2022_64
#    FFmpeg:D:/develop-src/ffmpeg-7.1.1
#    OpenCV:D:/develop-src/opencv/build

# 2. 使用 Visual Studio 生成器构建
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# 3. 运行
./build/Debug/OBSIM.exe
```

构建后自动复制以下 DLL 到输出目录：
- **Qt DLLs**: Core, Gui, Widgets, Multimedia, OpenGL, OpenGLWidgets, Concurrent, Network
- **Qt 插件**: `platforms/qwindows.dll`, `multimedia/ffmpegmediaplugin.dll`
- **FFmpeg DLLs**: avcodec-61, avdevice-61, avformat-61, avutil-59, swresample-5, swscale-8
- **OpenCV DLLs**: opencv_world4120.dll, opencv_videoio_ffmpeg4120_64.dll
- **VLD**: vld_x64.dll, dbghelp.dll (仅 Debug)

### 运行测试

```bash
cmake --build build --config Debug
cd build
ctest --output-on-failure -C Debug
```

---

## 5. 核心层 (Core Layer)

核心层是所有业务逻辑的所在地，不直接依赖 Qt GUI 组件，但使用 QtCore 进行信号/槽通信。

### 5.1 Source 源体系

[Source](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/base/source.h) 是所有输入源的抽象基类，定义了源的通用几何属性和渲染接口。

```
Source (抽象基类)
├── VideoSource           # 视频采集源基类
│   ├── ScreenCaptureSource  # 屏幕采集源
│   └── CameraCaptureSource  # 摄像头采集源
├── ImageSource           # 图片源 (静态图片)
└── TextSource            # 文字源 (可编辑文本)
```

#### 核心属性 (Source)

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | `std::string` | 唯一标识符 |
| `display_name` | `QString` | UI 显示名称 |
| `pos_x, pos_y` | `float` | 画布位置 (左上角, 1920x1080 坐标系) |
| `base_width/height` | `float` | 基准尺寸 (原始素材尺寸) |
| `scale_x, scale_y` | `float` | 缩放比例 |
| `rotation` | `float` | 旋转角度 (度) |
| `crop_left/top/right/bottom` | `float` | 裁剪边缘 (像素) |
| `visible` | `bool` | 可见性 |
| `selected` | `bool` | 选中状态 |
| `lock_aspect_ratio` | `bool` | 锁定宽高比 |
| `fixed_aspect_ratio` | `float` | 固定宽高比 |

#### 核心方法 (Source)

| 方法 | 说明 |
|------|------|
| `render()` | 纯虚函数，子类实现具体的 OpenGL 渲染 |
| `update_frame()` | 每帧更新数据，返回是否有更新 |
| `get_bounding_rect()` | 获取画布上的包围矩形 (含缩放) |
| `get_content_rect()` | 获取内容矩形 (含裁剪)，本地坐标 |
| `hit_test(QPointF)` | 命中测试，判断画布坐标是否在源内 |
| `load_resources()` | 加载纹理等资源 |
| `unload_resources()` | 卸载纹理等资源 |

#### VideoSource

[VideoSource](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/videosource.h) 是所有视频类源的基类，持有一个 `VideoCaptor` 实例用于采集。负责：
- 纹理创建与帧上传 (`create_texture`, `upload_frame_to_texture`)
- 帧更新循环 (`update_frame`)
- 滤镜接口 (`filter()`, `start_filter()`, `stop_filter()`)

#### ScreenCaptureSource

[ScreenCaptureSource](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/screencapturesource.h) 封装屏幕采集源，组合 `ScreenCaptor`，携带 `CaptorConfig` 配置 (偏移和尺寸)。

#### CameraCaptureSource

[CameraCaptureSource](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/cameracapturesource.h) 封装摄像头采集源，组合 `Camera`。

#### ImageSource

[ImageSource](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/imagesource.h) 加载静态图片作为源，使用 Qt `QImage` 加载图片文件，通过 OpenGL 纹理渲染。

#### TextSource

[TextSource](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/textsource.h) 使用 QPainter 将文本渲染到 QImage，再上传为 OpenGL 纹理。支持动态修改文本、字体、颜色。

---

### 5.2 VideoCaptor 视频采集体系

[VideoCaptor](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/base/videocaptor.h) 是视频采集的抽象基类，基于 FFmpeg 的 libavdevice 实现。

```
VideoCaptor (抽象基类)
├── ScreenCaptor          # 屏幕采集 (gdigrab)
└── Camera                # 摄像头采集 (dshow)
```

#### 核心流程

```
start() → 创建线程 → capture_loop()
  └→ avformat_open_input → av_read_frame → 解码 → push_frame → 回调通知
stop() → 停止线程 → 清理资源
```

#### 关键方法

| 方法 | 说明 |
|------|------|
| `start()` | 启动采集线程 |
| `stop()` | 停止采集线程 |
| `try_pop_frame()` | 非阻塞弹出采集帧 (AVFramePtr) |
| `set_frame_ready_callback()` | 设置帧就绪回调 |
| `get_input_format_name()` | 纯虚函数，返回 FFmpeg 输入格式名 |
| `get_device_name()` | 纯虚函数，返回设备名 |
| `setup_options()` | 可选的 AV 选项设置 |

#### 帧传递机制

- 采集到的 `AVFrame` 通过 `DataSafeQueue<AVFramePtr>` 传递
- 回调方式：采集线程通过 `FrameReadyCallback` 通知消费者
- 帧格式：统一转换为 `AV_PIX_FMT_BGRA` (便于 OpenGL 纹理上传)

#### ScreenCaptor

[ScreenCaptor](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/screencaptor.h) 使用 FFmpeg 的 `gdigrab` 输入格式捕获 Windows 屏幕。`apply_config()` 允许设置采集区域偏移和尺寸。

#### Camera

[Camera](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/camera.h) 使用 FFmpeg 的 `dshow` 输入格式捕获摄像头。构造函数接受设备描述字符串。

---

### 5.3 AudioCaptor 音频采集体系

[AudioCaptor](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/base/audiocaptor.h) 是音频采集的抽象基类。

```
AudioCaptor (抽象基类)
├── SystemAudioCaptor     # 系统音频采集 (WASAPI)
└── MicAudioCaptor        # 麦克风采集 (DShow)
```

#### 关键方法

| 方法 | 说明 |
|------|------|
| `start()` | 启动音频采集线程 |
| `stop()` | 停止采集 |
| `try_pop_frame()` | 弹出音频帧 |
| `set_record_queue()` | 设置录制队列 (录制时启用) |
| `push_to_record_queue()` | 将帧推送到录制队列 |

#### SystemAudioCaptor

[SystemAudioCaptor](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/systemaudiocaptor.h) 使用 **WASAPI** (Windows Audio Session API) 采集系统音频输出。它**重写了 `init_ctx()` 和 `capture_loop()`**，通过 `IMMDeviceEnumerator → IAudioClient → IAudioCaptureClient` 直接从 WASAPI 获取 PCM 数据，而非通过 FFmpeg 的 wasapi 输入。数据获取后包装为 AVFrame 放入队列。

#### MicAudioCaptor

[MicAudioCaptor](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/micaudiocaptor.h) 使用 FFmpeg 的 `dshow` 输入格式采集麦克风。支持指定设备名 (`audio=设备名`)。

---

### 5.4 AudioManager 音频管理器

[AudioManager](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/audiomanager.h) 是音频采集的总控类，继承自 `QObject`，通过 Qt 信号/槽机制与前端通信。

#### 核心职责

| 职责 | 说明 |
|------|------|
| 音频采集管理 | 管理 `SystemAudioCaptor` 和 `MicAudioCaptor` 的启停 |
| 设备热切换 | `set_system_audio_device()` / `set_mic_audio_device()` 支持运行时更换设备 |
| 电平计算 | `calculate_level_from_frame()` 计算 RMS 电平，平滑处理 |
| 录制队列 | `enable_recording()` / `disable_recording()` 管理录制音频队列 |
| 信号发射 | `levels_updated(float system_level, float mic_level)` 实时发射电平值 |

#### 电平计算算法

1. 从 AVFrame 提取 PCM 样本 (支持 U8/S16/S32/FLT/DBL/S64)
2. 计算 RMS (Root Mean Square) 值
3. 应用不同的映射曲线：
   - 系统音频：`RMS → sqrt(RMS) × 1.5 → clamp(0, 1)`
   - 麦克风：`RMS < 0.008 → 0，否则 sqrt(RMS) × 2.0 → clamp(0, 1)`
4. 平滑处理：上升用 0.3 旧 + 0.7 新，衰减用 0.8 旧 + 0.2 新
5. 60Hz 定时器补充衰减 (每 tick × 0.95)

---

### 5.5 Recoder 录制/推流体系

[Recoder](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/base/recoder.h) 是录制和推流的抽象基类，负责视频+音频的编码与封装。

```
Recoder (抽象基类)
├── FileRecoder           # 本地文件录制 (MP4)
└── StreamPush            # RTMP 直播推流
```

#### 编码参数

| 参数 | 值 |
|------|-----|
| 音频采样率 | 48000 Hz |
| 音频声道 | 2 (立体声) |
| 音频格式 | AV_SAMPLE_FMT_FLTP |
| 音频帧大小 | 1024 samples |
| 最大 FIFO 大小 | 48000 × 10 samples |
| 默认音量 | 0.7 |
| 默认帧率 | 30 fps |

#### 核心流程

```
start() → init_contexts() → 创建编码线程 → main_encode_loop()
  ├→ process_video_frame()    # 从视频队列取出帧 → 编码
  ├→ process_system_audio()   # 从系统音频队列取出帧 → 重采样 → FIFO
  ├→ process_mic_audio()      # 从麦克风音频队列取出帧 → 重采样 → FIFO
  ├→ mix_audio_streams()      # 混合两路音频 (含音量/静音控制)
  └→ encode_audio_frame()     # 从 FIFO 取数据 → 编码
stop() → flush → 写 trailer → 释放
```

#### 音量混合

两路音频 (`m_system_volume`, `m_mic_volume`) 通过 FIFO (`std::deque<float>`) 进行时间对齐混合。支持独立静音 (`m_system_muted`, `m_mic_muted`)。

#### FileRecoder

[FileRecoder](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/filerecoder.h) 实现 `create_format_context()` 和 `open_io()`，输出到本地 MP4 文件。

#### StreamPush

[StreamPush](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/streampush.h) 实现 `create_format_context()` 和 `open_io()`，输出到 RTMP 服务器 URL。

---

### 5.6 Scene 场景管理器

[Scene](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/scene.h) 管理同一场景内的所有源，负责源的层叠顺序、选中状态、交互操作和吸附功能。

#### 核心功能

| 功能 | 说明 |
|------|------|
| 源管理 | `add_source()`, `remove_source()`, `move_to_top/bottom()`, `move_up/down()` |
| 选中管理 | `set_selected_source()`, `clear_selection()`, 单选模式 |
| 命中测试 | `hit_test()` 从顶层到底层遍历 |
| 交互状态机 | `on_mouse_press()`, `on_mouse_move()`, `on_mouse_release()` |
| 吸附 (Snap) | 边缘/中心线吸附，阈值可配，满屏吸附自动缩放 |
| 选中框绘制 | `render_selection_box()` 红色边框 + 8 个白色控点 |
| 悬停框绘制 | `render_hover_box()` 半透明白色边框 |
| 吸附线绘制 | `render_snap_lines()` 红色半透明辅助线 |

#### 交互模式 (InteractionMode)

| 模式 | 说明 |
|------|------|
| `None` | 无操作 |
| `Hover` | 悬停 (高亮源) |
| `Dragging` | 拖动源 (含吸附) |
| `Resizing` | 缩放源 (8 方向控点) |
| `Selecting` | 框选 (预留) |

#### 吸附机制

- 吸附目标：画布边界 + 其他源的 4 条边
- 吸附阈值：默认 25 个逻辑像素
- 满屏吸附：当源接近画布四角且接近满屏尺寸时，自动等比缩放填充全屏

---

### 5.7 Filter 滤镜模块

#### OpenCVFilter

[OpenCVFilter](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/filter/opencvfilter.h) 基于 OpenCV 实现图像滤镜处理。

| 滤镜类型 | 参数 |
|----------|------|
| 翻转 (Flip) | `flip_code`: 0 (垂直), 1 (水平), -1 (两者) |
| 灰度 (Grayscale) | 启用/禁用 |
| 色彩调整 | `brightness` (-1.0~1.0), `contrast` (0.0~3.0), `saturation` (0.0~3.0) |

#### FilteredVideoCaptor

[FilteredVideoCaptor](file:///d:/ProjectFiles/ClionProject/OBSIM/src/core/filter/filteredvideocaptor.h) 是一个**装饰器模式**实现，包装一个 `VideoCaptor` 实例：

- 原始帧先进入 `m_raw_queue`
- 滤镜线程从 `m_raw_queue` 取出帧，应用 `OpenCVFilter` 处理
- 处理后帧放入 `m_filtered_queue`
- 消费者通过 `try_pop_filtered_frame()` 获取滤镜后帧

---

## 6. 前端层 (Frontend Layer)

前端层基于 Qt Widgets 构建，使用 OpenGL 进行渲染。

### 6.1 MainWindow 主窗口

[MainWindow](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/mainwindow.h) 是整个应用的控制器，负责：

- 初始化 UI 布局
- 管理 `AudioManager`, `FileRecoder`, `StreamPush` 等核心对象
- 转发 UI 操作到业务逻辑
- 处理场景/源的增删改查
- 录制和推流的启动/停止
- 异步加载和保存配置 (场景、源、音频设置)

#### 布局结构

```
MainWindow (QWidget)
└── QVBoxLayout
    └── QSplitter (垂直)
        ├── ScenePreviewWidget    (拉伸因子: 1)  ← OpenGL 预览画布
        ├── SettingBar            (拉伸因子: 0)  ← 源设置栏
        └── ControlBar            (拉伸因子: 0)  ← 控制栏
```

#### 核心信号连接

```cpp
// 音频电平更新
AudioManager::levels_updated → ControlBar::update_audio_levels

// 源操作
SourceControlBlock 的 display/camera/text/image 请求 → MainWindow 处理
SourceControlBlock::source_list_selection_changed → Scene 选中切换

// 场景操作
SceneControlBlock::scene_added/removed/selection_changed → ScenePreviewWidget

// 录制/推流
StreamRecordBlock::recording_started/stopped → FileRecoder
StreamRecordBlock::streaming_started/stopped → StreamPush
```

---

### 6.2 ScenePreviewWidget 场景预览

[ScenePreviewWidget](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/scenepreviewwidget.h) 继承自 `QOpenGLWidget`，是核心的渲染控件：

**画布逻辑坐标系**: 1920 × 1080 (固定)，通过 `setup_viewport_and_clear()` 映射到实际控件尺寸。

#### 渲染循环 (60fps)

```
paintGL() 每帧调用:
├── setup_viewport_and_clear()
├── render_canvas_background()         # 绘制深色背景
├── render_all_sources_with_clip()     # 遍历源列表，逐个渲染
│   └── for each source:
│       ├── 设置裁剪区域 (stencil)
│       ├── glPushMatrix → 应用变换 (位置/缩放/旋转)
│       ├── source->render()
│       └── glPopMatrix
├── render_mosaic_for_selected()       # 选中源的马赛克效果
├── current_scene().render_selection_box()  # 选中框 + 控点
└── current_scene().render_snap_lines()     # 吸附辅助线
```

#### 帧捕获回调 (录制/推流用)

```cpp
// 录制时 OpenGL 帧 → Recoder
paintGL() → m_frame_capture_callback(data, stride, w, h)
  → Recoder::feed_frame(data, stride, w, h)
```

#### 多场景管理

使用 `std::vector<SceneData>` 存储多个场景，每个 `SceneData` 包含：
- `scene` (Scene 对象，管理源指针)
- `source_storage` (std::vector<std::unique_ptr<Source>>，管理源生命周期)
- `name` (场景名称)

---

### 6.3 PreviewBaseWidget 预览基类

[PreviewBaseWidget](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/base/previewbasewidget.h) 是滤镜/设置预览窗口的基类，提供：

- 内嵌 `QOpenGLWidget` 用于渲染
- 定时器驱动刷新 (`m_render_timer`)
- `create_control_area()` 虚函数供子类实现控制面板
- `create_split_panel()` 快速创建带分割面板的布局

子类：
- [FilterPreviewWidget](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/filterpreviewwidget.h) - 滤镜参数编辑 (翻转/灰度/色彩调整)
- [SettingsPreviewWidget](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/settingspreviewwidget.h) - 源属性编辑 (名称等)

---

### 6.4 ControlBar 控制栏

[ControlBar](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/controlbar.h) 包含四个控制块 (ControlBlock)：

| 组件 | 功能 |
|------|------|
| **SceneControlBlock** | 场景列表管理 (添加/删除/切换) |
| **SourceControlBlock** | 输入源管理 (添加屏幕/摄像头/文字/图片源, 排序, 删除) |
| **AudioMixerBlock** | 混音控制 (音量滑块, 静音按钮, 电平表, 设备选择) |
| **StreamRecordBlock** | 录制/推流控制 (开始/停止, 输出路径, 推流地址) |

此外还包含多个辅助对话框：
- `TextSourceDialog` - 文字源配置
- `ImageSourceDialog` - 图片源配置
- `SourceTypeDialog` - 源类型选择
- `SourceNameDialog` - 源命名和硬件选择

---

### 6.5 SettingsDialog 设置对话框

[SettingsDialog](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/settingsdialog.h) 提供应用全局设置：

- 左侧页面列表："输出"、"直播"
- **输出页**：录像输出路径 (浏览/打开文件夹)
- **直播页**：RTMP 推流地址配置
- 设置异步保存到 `config.ini`

---

### 6.6 SettingBar 设置栏

[SettingBar](file:///d:/ProjectFiles/ClionProject/OBSIM/src/frontend/widget/settingbar.h) 位于预览区和控制栏之间，显示当前选中源的名称，提供"滤镜"和"设置"两个操作按钮。

---

## 7. 工具层 (Utility Layer)

### 7.1 DeviceManager

[DeviceManager](file:///d:/ProjectFiles/ClionProject/OBSIM/src/utils/devicemanager.h) 负责枚举系统硬件设备：

| 设备类型 | 实现方式 | 数据结构 |
|----------|----------|----------|
| 显示器 | `QGuiApplication::screens()` | `DisplayInfo` (索引, 名称, 几何, DPI, 刷新率) |
| 摄像头 | `QMediaDevices::videoInputs()` | `CameraInfo` (索引, 名称, ID) |
| 音频输出 | WASAPI `IMMDeviceEnumerator` | `AudioOutputInfo` (ID, 名称, 默认) |
| 音频输入 | WASAPI `IMMDeviceEnumerator` / DShow | `AudioInputInfo` (名称, 原始名, 默认) |

### 7.2 ConfigManager

[ConfigManager](file:///d:/ProjectFiles/ClionProject/OBSIM/src/utils/configmanager.h) 提供异步配置读写基础设施：

- 配置文件位置：`{可执行文件目录}/config.ini`
- 格式：`QSettings::IniFormat`
- 异步保存：`AsyncSettings::async_save(writer)` 使用 `QtConcurrent::run`
- 异步加载：`AsyncSettings::async_load<T>(reader)` 返回 `QFuture<T>`
- 线程安全：全局 `QMutex` 保证 I/O 互斥

### 7.3 DataSafeQueue

[DataSafeQueue](file:///d:/ProjectFiles/ClionProject/OBSIM/src/utils/datasafequeue.h) 是一个线程安全的有界队列模板类，使用 `std::queue`, `std::mutex` 和 `std::condition_variable` 实现。

**关键特性**：
- 有界队列：默认最大 256 个元素
- 阻塞 push/pop：当满/空时通过 condition_variable 等待
- 非阻塞 try_push/try_pop：超时返回
- 支持丢弃旧帧的 `push_no_wait()` 和 `try_pop_drain()`
- `pop_with_drain()` 丢弃队列中除最新帧外的所有数据
- 暂停/停止控制原子标志

### 7.4 FFmpegFactory

[FFmpegFactory](file:///d:/ProjectFiles/ClionProject/OBSIM/src/utils/ffmpegfactory.h) 提供 FFmpeg 核心类型的 RAII 封装：

| 智能指针类型 | 删除器 | 管理的 FFmpeg 类型 |
|--------------|--------|--------------------|
| `AVFormatContextPtr` | `shared_ptr` + `AVFormatContextDeleter` | `AVFormatContext` (输入) |
| `AVFormatOutputContextPtr` | `unique_ptr` + `AVFormatOutputContextDeleter` | `AVFormatContext` (输出) |
| `AVCodecContextPtr` | `unique_ptr` + `AVCodecContextDeleter` | `AVCodecContext` |
| `SwsContextPtr` | `unique_ptr` + `SwsContextDeleter` | `SwsContext` |
| `AVPacketPtr` | `shared_ptr` + `AVPacketDeleter` | `AVPacket` |
| `AVFramePtr` | `shared_ptr` + `AVFrameDeleter` | `AVFrame` |
| `AVStreamPtr` | `unique_ptr` + `AVStreamDeleter` | `AVStream` |
| `SwrContextPtr` | `unique_ptr` + `SwrContextDeleter` | `SwrContext` |

辅助函数：`make_output_context()`, `get_stream_by_index()`

### 7.5 PCH.h 预编译头

[PCH.h](file:///d:/ProjectFiles/ClionProject/OBSIM/src/utils/PCH.h) 包含项目广泛使用的头文件：
- Qt 核心库：`<QApplication>`, `<QtCore>`, `<QtGui>`, `<QtWidgets>`
- C++ 标准库：`<iostream>`, `<string>`, `<vector>`, `<algorithm>`, `<memory>`

---

## 8. 测试体系

### 测试结构

```
test/
├── CMakeLists.txt          # 测试构建配置
├── test_datasafequeue.cpp  # DataSafeQueue 多线程安全测试
├── test_opencvfilter_gtest.cpp # OpenCVFilter GTest 测试
├── test_videocaptor.cpp    # VideoCaptor Mock 测试
├── test_recoder.cpp        # Recoder 编码流程测试
├── test_source_geometry.cpp # Source 几何计算测试 (QTest)
├── test_scene_state.cpp    # Scene 状态机测试 (QTest)
├── test_configmanager.cpp  # ConfigManager 异步读写测试 (QTest)
├── test_audiomanager.cpp   # AudioManager 依赖注入测试 (QTest)
└── test_opencv_filter.cpp  # 遗留的旧 OpenCV 手写测试
```

### 测试构建

测试目标通过 `add_obsim_test()` 和 `add_obsim_qt_test()` 两个 CMake 函数添加：

- `add_obsim_test()`: 纯 C++ 测试 (无 Qt GUI 依赖)，链接 `obsim_core_static` + GTest
- `add_obsim_qt_test()`: Qt 依赖测试，额外链接 `Qt6::Test` + `Qt6::Widgets`

---

## 9. 依赖关系总览

### 构建目标依赖图

```
OBSIM (可执行文件)
├── obsim_core_static (静态库)
│   ├── Qt6::Core
│   ├── Qt6::Gui
│   ├── Qt6::Widgets
│   ├── Qt6::OpenGL
│   ├── Qt6::Multimedia
│   ├── Qt6::Concurrent
│   ├── OpenCV (core, imgproc, imgcodecs)
│   ├── OpenGL::GL
│   └── FFmpeg (avdevice, avformat, avcodec, avutil, swscale, swresample)
├── Qt6::Widgets
└── Qt6::OpenGLWidgets
```

### 库用途

| 库 | 用途 |
|----|------|
| **Qt6::Core** | Qt 基础 (QObject, 信号槽, QSettings, QThread) |
| **Qt6::Gui** | QImage, QFont, QPainter, QColor |
| **Qt6::Widgets** | QWidget, QOpenGLWidget, QListWidget, 对话框 |
| **Qt6::OpenGL** | OpenGL 支持 |
| **Qt6::OpenGLWidgets** | QOpenGLWidget 控件 |
| **Qt6::Multimedia** | QMediaDevices (摄像头枚举) |
| **Qt6::Concurrent** | QtConcurrent::run (异步配置读写) |
| **FFmpeg** | 音视频采集 (avdevice)、解码 (avcodec)、封装 (avformat)、格式转换 (swscale, swresample) |
| **OpenCV** | 图像滤镜 (翻转、灰度、色彩调整) |
| **OpenGL** | 硬件加速渲染、纹理处理 |
| **Google Test** | 单元测试框架 |

### 内部模块依赖

```
Frontend → Core:        MainWindow → AudioManager, FileRecoder, StreamPush
                        ScenePreviewWidget → Scene, Source 及其子类
                        ControlBar → DeviceManager

Core → Utils:           VideoCaptor/AudioCaptor → FFmpegFactory, DataSafeQueue, av_err2str
                        AudioManager → DataSafeQueue
                        MainWindow → ConfigManager

Core ← Core:            Scene → Source
                        VideoSource → VideoCaptor
                        FilteredVideoCaptor → VideoCaptor, OpenCVFilter
                        AudioManager → SystemAudioCaptor, MicAudioCaptor

Utils → External:       DeviceManager → QMediaDevices, WASAPI COM 接口
                        FFmpegFactory → FFmpeg 头文件
                        ConfigManager → QSettings, QtConcurrent
```

---

## 10. 关键设计模式与数据流

### 10.1 数据流总图

```
┌─────────────┐    采集线程      ┌──────────────────┐    帧回调      ┌───────────────┐
│ ScreenCaptor│──────────────→   │ DataSafeQueue    │─────────────→ │ VideoSource   │
│  Camera     │  AVFramePtr      │ (VideoCaptor)    │  FrameReady   │ (纹理上传)    │
└─────────────┘                  └──────────────────┘               └───────┬───────┘
                                                                           │
┌─────────────┐    采集线程      ┌──────────────────┐    帧回调      ┌───────┴───────┐
│SystemAudio  │──────────────→   │ DataSafeQueue    │─────────────→ │ ScenePreview  │
│MicAudio     │  AVFramePtr      │ (AudioCaptor)    │  FrameReady   │ (OpenGL渲染)  │
└─────────────┘                  └──────────────────┘               └───────┬───────┘
                                                                           │
                                                 录制/推流时               │
                                                 ┌─────────────────────────┘
                                                 ▼
                                        ┌──────────────────┐
                                        │ Recoder          │
                                        │ (编码线程)       │
                                        │ ┌──────────────┐ │
                                        │ │ 视频编码队列  │←── feed_frame() (BGRA)
                                        │ │ 系统音频队列  │←── SystemAudioCaptor
                                        │ │ 麦克风音频队列│←── MicAudioCaptor
                                        │ │ 混音 → 编码   │
                                        │ └──────────────┘ │
                                        └──────┬───────────┘
                                               │
                                    ┌──────────┴──────────┐
                                    ▼                      ▼
                              ┌──────────┐          ┌──────────┐
                              │ MP4 File │          │ RTMP URL │
                              │ (本地录制)│          │ (直播推流)│
                              └──────────┘          └──────────┘
```

### 10.2 关键设计模式

| 模式 | 使用位置 | 说明 |
|------|----------|------|
| **策略模式** | Source 类体系 | 纯虚函数 `render()`, `update_frame()` 定义渲染策略 |
| **模板方法** | VideoCaptor/AudioCaptor/Recoder | `start() → capture_loop() / encoding_loop()` 定义骨架，子类实现具体细节 |
| **装饰器模式** | FilteredVideoCaptor | 包装 VideoCaptor，添加滤镜处理层 |
| **观察者模式** | Qt 信号/槽 | AudioManager → ControlBar (电平更新), MainWindow 全局事件处理 |
| **RAII** | FFmpegFactory | 智能指针 + 删除器管理 FFmpeg 资源生命周期 |
| **生产者-消费者** | DataSafeQueue | 采集线程 (生产者) → 队列 → 渲染/编码线程 (消费者) |
| **单例模式** | AsyncSettings::io_mutex | 全局 I/O 互斥锁 |

### 10.3 音频采集特别说明

`SystemAudioCaptor` 并未使用 FFmpeg 的 wasapi 输入，而是**直接通过 Windows WASAPI COM 接口**获取 PCM 音频数据。这种做法的优势：
- 更低延迟
- 更精确的设备选择控制 (通过设备 ID)
- 更灵活的缓冲区管理

### 10.4 配置序列化结构

```
config.ini
├── [General]
│   ├── output_path    (录像输出路径)
│   └── stream_url     (推流地址)
├── [Audio]
│   ├── system_volume, system_muted
│   ├── mic_volume, mic_muted
│   ├── system_audio_device
│   └── mic_audio_device
└── [Scenes]
    ├── count, current_index
    └── Scene_0..N/
        ├── name
        └── Sources/
            ├── count
            └── Source_0..N/
                ├── type, display_name, pos_x/y, scale_x/y
                ├── visible, rotation, lock_aspect_ratio
                ├── (类型相关: offset/尺寸/text/font/color/file_path)
                └── filter_*  (滤镜参数)
```

---

*本文档由 Code Wiki 自动生成，基于对 OBSIM 项目源代码的静态分析。*