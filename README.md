# OBSIM

> **OBS IMitation** — 一个参考 [OBS (Open Broadcaster Software)](https://obsproject.com/) 架构实现的直播推流软件。

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Qt 6.9](https://img.shields.io/badge/Qt-6.9-41CD52?logo=qt)](https://www.qt.io/)
[![FFmpeg 7.1](https://img.shields.io/badge/FFmpeg-7.1-007808?logo=ffmpeg)](https://ffmpeg.org/)
[![OpenCV 4.12](https://img.shields.io/badge/OpenCV-4.12-5C3EE8?logo=opencv)](https://opencv.org/)
[![CMake 3.24+](https://img.shields.io/badge/CMake-3.24%2B-064F8C?logo=cmake)](https://cmake.org/)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows)](https://www.microsoft.com/windows)

---

OBSIM 是一个基于 C++20 的直播推流软件，支持多输入源采集、多场景管理、实时视频滤镜、本地录制和 RTMP 直播推流。项目从零开始构建，旨在深入理解直播推流的技术全链路。

---

## 功能特性

- **多输入源** — 屏幕采集、摄像头采集、图片源、文字源，支持自由布局与变换
- **多场景管理** — 独立管理多个场景，每场景拥有独立的源列表和布局
- **实时音频采集** — 系统音频（WASAPI）+ 麦克风音频（DShow），独立音量/静音控制与实时电平显示
- **视频滤镜** — 基于 OpenCV 的翻转、灰度、亮度/对比度/饱和度实时调整
- **本地录制** — MP4 文件输出（H.264 + AAC 编码）
- **RTMP 推流** — 直播推送到任意 RTMP 服务器
- **源配置持久化** — 场景布局、源属性、音频设置自动保存与加载
- **深色主题** — 全局 QSS 样式深色 UI

---

## 快速开始

### 环境要求

| 依赖 | 版本 | 用途 |
|------|------|------|
| CMake | ≥ 3.24 | 构建系统 |
| Qt | ≥ 6.9.0 | GUI 框架 |
| FFmpeg | ≥ 7.1.1 | 音视频编解码与采集 |
| OpenCV | ≥ 4.12.0 | 图像滤镜处理 |
| OpenGL | — | 硬件加速渲染 |
| Google Test | v1.15.2（自动拉取） | 单元测试 |

### 构建步骤 (Windows / MSVC)

```bash
# 克隆项目
git clone https://github.com/your-username/OBSIM.git
cd OBSIM

# 配置
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build --config Debug

# 运行
./build/Debug/OBSIM.exe
```

> **注意**：构建前请在 CMakeLists.txt 中配置 Qt、FFmpeg、OpenCV 的本地路径。

### 运行测试

```bash
cmake --build build --config Debug
cd build
ctest --output-on-failure -C Debug
```

---

## 项目架构

项目采用经典的三层架构，自底向上分为 **工具层 (Utils)**、**核心层 (Core)** 和 **前端层 (Frontend)**：

```
┌──────────────────────────────────────────────────────────┐
│                   Frontend Layer (UI)                     │
│   MainWindow · ControlBar · ScenePreview · Settings      │
│   SettingBar · FilterPreview · SettingsPreview           │
├──────────────────────────────────────────────────────────┤
│                    Core Layer (业务逻辑)                   │
│   Source体系 · Scene管理器 · AudioManager · Recoder      │
│   FileOutput · StreamOutput · Filter模块                 │
│   ScreenCaptor · Camera · AudioCaptor                   │
├──────────────────────────────────────────────────────────┤
│                   Utility Layer (工具)                    │
│   DeviceManager · ConfigManager · DataSafeQueue          │
│   FFmpegFactory (RAII封装) · av_err2str                 │
└──────────────────────────────────────────────────────────┘
```

### 核心数据流

```
采集线程                   编码线程              输出线程
┌──────────┐  AVFrame  ┌──────────┐  AVPacket  ┌────────────┐
│ VideoSrc │ ────────→ │ Recoder  │ ────────→ │ FileOutput │ → MP4 文件
│ AudioSrc │           │ (编码引擎)│           ├────────────┤
└──────────┘           │          │           │StreamOutput│ → RTMP 推流
                       └──────────┘           └────────────┘
```

录制和推流共享同一个 Recoder 编码引擎，编码后的数据包通过独立的 OutputChannel 通道分别写入文件和推送到网络，两者互不阻塞。

### 关键模块

| 模块 | 说明 |
|------|------|
| **Source 体系** | 源抽象基类，派生视频源 (VideoSource)、屏幕源 (ScreenCaptureSource)、摄像头源 (CameraCaptureSource)、图片源 (ImageSource)、文字源 (TextSource) |
| **Scene** | 场景管理器，负责源的层叠排序、选中管理、交互操作（拖动/缩放/吸附） |
| **AudioManager** | 音频采集总控，管理系统音频 (WASAPI) 和麦克风音频 (DShow) 的启停、音量、静音和电平计算 |
| **Recoder** | 视频+音频编码引擎，负责 H.264/AAC 编码、音频重采样与混音 |
| **FileOutput** | 独立 IO 线程，将编码后的数据包写入 MP4 文件 |
| **StreamOutput** | 独立 IO 线程，将编码后的数据包推送到 RTMP 服务器 |
| **Filter** | OpenCV 滤镜模块，支持翻转、灰度、色彩调整等实时滤镜 |

---

## 项目目录结构

```
OBSIM/
├── CMakeLists.txt                    # 顶层构建配置
├── README.md                         # 项目介绍
├── resources/
│   ├── resources.qrc                 # Qt 资源文件
│   └── style.qss                     # 深色主题样式表
├── src/
│   ├── main.cpp                      # 应用入口
│   ├── core/                         # 核心层——业务逻辑
│   │   ├── base/                     # 基类与抽象
│   │   │   ├── source.h/.cpp         #   源基类
│   │   │   ├── videocaptor.h/.cpp    #   视频采集基类
│   │   │   ├── audiocaptor.h/.cpp    #   音频采集基类
│   │   │   ├── recoder.h/.cpp        #   编码引擎基类
│   │   │   └── outputchannel.h       #   输出通道抽象接口
│   │   ├── filter/                   # 滤镜模块
│   │   │   ├── opencvfilter.h/.cpp
│   │   │   └── filteredvideocaptor.h/.cpp
│   │   ├── audiomanager.h/.cpp       # 音频管理器
│   │   ├── camera.h/.cpp             # 摄像头设备
│   │   ├── cameracapturesource.h/.cpp
│   │   ├── fileoutput.h/.cpp         # 文件输出通道
│   │   ├── imagesource.h/.cpp
│   │   ├── micaudiocaptor.h/.cpp
│   │   ├── scene.h/.cpp              # 场景管理器
│   │   ├── screencaptor.h/.cpp
│   │   ├── screencapturesource.h/.cpp
│   │   ├── streamoutput.h/.cpp       # 推流输出通道
│   │   ├── systemaudiocaptor.h/.cpp  # 系统音频 (WASAPI)
│   │   ├── textsource.h/.cpp
│   │   └── videosource.h/.cpp
│   ├── frontend/                     # 前端层——Qt UI
│   │   ├── base/
│   │   │   └── previewbasewidget.h/.cpp
│   │   └── widget/
│   │       ├── mainwindow.h/.cpp     # 主窗口
│   │       ├── controlbar.h/.cpp     # 控制栏
│   │       ├── settingbar.h/.cpp
│   │       ├── scenepreviewwidget.h/.cpp  # OpenGL 场景预览
│   │       ├── settingsdialog.h/.cpp
│   │       ├── settingspreviewwidget.h/.cpp
│   │       └── filterpreviewwidget.h/.cpp
│   └── utils/                        # 工具层
│       ├── PCH.h                     # 预编译头
│       ├── configmanager.h           # 异步配置管理器
│       ├── datasafequeue.h           # 线程安全有界队列
│       ├── devicemanager.h/.cpp      # 设备枚举器
│       ├── ffmpegfactory.h           # FFmpeg RAII 封装
│       └── av_err2str_cxx.h          # FFmpeg 错误字符串工具
└── test/                             # 单元测试
    ├── CMakeLists.txt
    ├── test_audiomanager.cpp
    ├── test_configmanager.cpp
    ├── test_datasafequeue.cpp
    ├── test_opencvfilter_gtest.cpp
    ├── test_recoder.cpp
    ├── test_scene_state.cpp
    ├── test_source_geometry.cpp
    └── test_videocaptor.cpp
```

---

## 设计亮点

- **编码与输出分离**：Recoder 仅负责编码，FileOutput / StreamOutput 各自运行在独立线程中处理 IO，互不阻塞
- **线程安全队列**：`DataSafeQueue` 模板类提供有界、阻塞/非阻塞、带丢弃策略的多生产者-多消费者队列
- **RAII 资源管理**：通过 `FFmpegFactory` 对 FFmpeg 核心类型进行智能指针封装，杜绝内存泄漏
- **OpenGL 加速渲染**：场景预览基于 `QOpenGLWidget`，支持 60fps 硬件加速渲染
- **WASAPI 直采**：系统音频采集绕过 FFmpeg，直接通过 Windows WASAPI COM 接口获取最低延迟音频数据

---

## 依赖与致谢

### 开源库

| 库 | 用途 |
|----|------|
| [Qt](https://www.qt.io/) | GUI 框架（Core, Widgets, OpenGL, Multimedia, Concurrent） |
| [FFmpeg](https://ffmpeg.org/) | 音视频编解码、采集、封装（libavdevice/format/codec/util/swscale/swresample） |
| [OpenCV](https://opencv.org/) | 图像滤镜处理（core, imgproc, imgcodecs） |
| [Google Test](https://github.com/google/googletest) | 单元测试框架 |
| [OpenGL](https://www.opengl.org/) | 3D 图形渲染加速 |

### 致谢

- 本项目参考了 [OBS Studio](https://obsproject.com/) 的架构设计理念，在此表示感谢
- 感谢所有开源库的维护者，没有这些优秀的开源项目就没有本项目的诞生

---

## 许可证

本项目仅供学习研究使用。