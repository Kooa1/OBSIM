# Recoder 基类抽取重构方案

## 目标

将当前 `Recoder` 类拆分为"基类 + 派生类"架构，使其写文件逻辑（录制到本地 MP4）和推流逻辑（推送到 RTMP 服务器）可以复用同一套编码管线。

---

## 现状分析

当前 `Recoder` 将所有逻辑耦合在一个类中：

- `encoding_loop()` — 主编排循环
- `init_video_encoder()` — 视频编码器初始化（`libx264` / `mpeg4`）
- `init_audio_encoder()` — 音频编码器初始化（`AAC` / `MP3` / `Opus`）
- `handle_audio_source()` / `mix_audio_sources()` / `encode_audio_fifo()` — 音频混音处理
- `handle_video_frame()` — 视频帧处理（BGRA → YUV420P 色彩空间转换）
- `encode_video_frame()` / `encode_audio_frame()` — 底层编码并写入
- `drain_video_queue()` / `flush_encoders()` — 刷空队列和编码器
- `open_output_and_allocate_frames()` — **与输出目标强相关**（`avio_open` 写本地文件）

**关键差异点（录制 vs 推流）：**

| 维度 | 文件录制 (MP4) | RTMP 推流 |
|---|---|---|
| `avformat_alloc_output_context2` 格式名 | `"mp4"` | `"flv"` |
| `avio_open` 目标 | 本地文件路径 | `rtmp://...` URL |
| 编码参数 | 高码率 (10Mbps) | 可能更低码率 |
| 写入方式 | `av_interleaved_write_frame` | 相同（但底层走网络） |
| 其他设置 | 无 | 可能需 `AVFMT_NOFILE` 设置 |

**其余所有逻辑（编码器初始化、音视频处理、帧管理、线程管理等）完全一致。**

---

## 重构方案

### 类层次设计

```
MediaOutput  (抽象基类, 新文件)
    ↑                    ↑
    │                    │
FileRecorder           StreamRecorder  (新文件)
(当前 Recoder 改名)    (新增, 推流)
```

### 文件变更清单

| 操作 | 旧文件 | 新文件 |
|---|---|---|
| **新建** | — | `frontend/core/mediaoutput.h` |
| **新建** | — | `frontend/core/mediaoutput.cpp` |
| **新建** | — | `frontend/core/streamrecorder.h` |
| **新建** | — | `frontend/core/streamrecorder.cpp` |
| **重命名** | `frontend/core/recoder.h` | `frontend/core/filerecorder.h` |
| **重命名** | `frontend/core/recoder.cpp` | `frontend/core/filerecorder.cpp` |
| **修改** | `frontend/widget/mainwindow.h` | 使用 `FileRecorder` 替代 `Recoder` |
| **修改** | `frontend/widget/mainwindow.cpp` | 适配新类名 |
| **修改** | `CMakeLists.txt` | 替换/添加源文件 |
| **删除** | `frontend/core/recoder.h` | — |
| **删除** | `frontend/core/recoder.cpp` | — |

---

### 1. MediaOutput 抽象基类

**文件：** `frontend/core/mediaoutput.h` / `mediaoutput.cpp`

**职责：** 封装录制/推流共用的编码管线，提供虚函数让子类定制"输出目标的创建"。

```cpp
class MediaOutput {
public:
    MediaOutput();
    virtual ~MediaOutput();

    // ========== 公共接口 ==========
    void start(const QString &output_url, int canvas_w, int canvas_h, int fps = 30,
               DataSafeQueue<AVFramePtr> *system_audio_src = nullptr,
               DataSafeQueue<AVFramePtr> *mic_audio_src = nullptr);
    void stop();
    bool is_recording() const { return m_recording.load(); }
    void feed_frame(const uint8_t *data, int stride, int capture_w, int capture_h);

protected:
    // ========== 子类必须实现的虚函数 ==========
    // 创建 AVFormatContext 并打开输出（文件或网络）
    // 包括 avformat_alloc_output_context2 + avio_open
    virtual bool init_output_context() = 0;

    // 返回封装格式名称，如 "mp4" / "flv"
    virtual const char* output_format_name() const = 0;

    // ========== 子类可选择重写的虚函数 ==========
    virtual void setup_encoder_opts(AVDictionary **video_opts, AVDictionary **audio_opts);
    virtual int video_bitrate() const { return 10'000'000; }

    // ========== 共用的编码管线和成员（从 Recoder 移入） ==========
    // --- 方法 ---
    void encoding_loop();
    bool init_video_encoder();
    bool init_audio_encoder();
    bool allocate_frames();
    void handle_audio_source(...);
    size_t mix_audio_sources();
    void encode_audio_fifo();
    void handle_video_frame(const VideoFrame &video_data);
    void drain_video_queue();
    void flush_swr_and_mix_remaining();
    void drain_audio_fifo_remaining();
    void flush_encoders();
    bool init_audio_swr(SwrContextPtr &swr, const AVFrame *frame);
    void encode_video_frame(int64_t pts);
    bool encode_audio_frame(int64_t audio_pts);
    void clear_ctx();

    // --- 成员变量 ---
    std::atomic<bool> m_recording{false};
    std::thread m_encode_thread;
    std::unique_ptr<DataSafeQueue<VideoFrame>> m_video_queue;
    DataSafeQueue<AVFramePtr> *m_system_audio_src = nullptr;
    DataSafeQueue<AVFramePtr> *m_mic_audio_src = nullptr;

    int m_canvas_w = 0, m_canvas_h = 0, m_fps = 30;
    QString m_output_url;

    // FFmpeg 资源（smart pointer）
    AVFormatMuxPtr m_fmt_ctx;
    AVCodecContextPtr m_video_enc_ctx;
    AVCodecContextPtr m_audio_enc_ctx;
    SwsContextPtr m_sws_ctx;
    SwrContextPtr m_sys_swr;
    SwrContextPtr m_mic_swr;
    AVFramePtr m_yuv_frame;
    AVFramePtr m_audio_frame;
    AVPacketPtr m_pkt;
    AVStream *m_video_stream = nullptr;
    AVStream *m_audio_stream = nullptr;

    // 编码状态
    std::deque<float> m_audio_fifo[2], m_sys_fifo[2], m_mic_fifo[2];
    int64_t m_audio_clock = 0, m_video_pts = 0, m_audio_pts = 0;
    int m_audio_frames_received = 0, m_audio_frames_encoded = 0;
    int64_t m_sys_silence_samples = 0, m_mic_silence_samples = 0;
    int m_last_capture_w = 0, m_last_capture_h = 0;
    int m_audio_frame_size = 0;
    bool m_has_audio = false;

    static constexpr int AUDIO_SAMPLE_RATE = 48000;
    static constexpr int AUDIO_CHANNELS = 2;
    // ...
};
```

**核心改动点 — `encoding_loop()` 中的差异适配：**

当前 Recoder 中的 `encoding_loop()` 包含：
```cpp
avformat_alloc_output_context2(&raw_fmt, nullptr, "mp4", m_output_path.toUtf8().constData());
```

改为调用虚函数：
```cpp
if (!init_output_context()) break;
```

---

### 2. FileRecorder（当前 Recoder 改名）

**文件：** `frontend/core/filerecorder.h` / `filerecorder.cpp`

```cpp
class FileRecorder : public MediaOutput {
public:
    FileRecorder() = default;
    ~FileRecorder() override = default;

protected:
    bool init_output_context() override;
    const char* output_format_name() const override { return "mp4"; }
};
```

`init_output_context()` 实现：
```cpp
bool FileRecorder::init_output_context() {
    AVFormatContext *raw = nullptr;
    avformat_alloc_output_context2(&raw, nullptr, "mp4", m_output_url.toUtf8().constData());
    if (!raw) return false;
    m_fmt_ctx.reset(raw);

    if (avio_open(&m_fmt_ctx->pb, m_output_url.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "open output file failed\n");
        return false;
    }
    return true;
}
```

---

### 3. StreamRecorder（新增）

**文件：** `frontend/core/streamrecorder.h` / `streamrecorder.cpp`

```cpp
class StreamRecorder : public MediaOutput {
public:
    StreamRecorder() = default;
    ~StreamRecorder() override = default;

protected:
    bool init_output_context() override;
    const char* output_format_name() const override { return "flv"; }
    void setup_encoder_opts(AVDictionary **video_opts, AVDictionary **audio_opts) override;
    int video_bitrate() const override { return 5'000'000; }  // 推流可用更低码率
};
```

`init_output_context()` 实现：
```cpp
bool StreamRecorder::init_output_context() {
    AVFormatContext *raw = nullptr;
    avformat_alloc_output_context2(&raw, nullptr, "flv", m_output_url.toUtf8().constData());
    if (!raw) return false;
    m_fmt_ctx.reset(raw);

    if (!(m_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmt_ctx->pb, m_output_url.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "open output stream failed\n");
            return false;
        }
    }
    return true;
}
```

---

### 4. 修改 MainWindow

将 `std::unique_ptr<Recoder> m_recoder;` 改为 `std::unique_ptr<FileRecorder> m_recorder;`

或保持面向基类编程：`std::unique_ptr<MediaOutput> m_media_output;` 然后在需要时向下转型。

**推荐方式：** 使用基类指针 + 工厂方法。

```cpp
// mainwindow.h
#include "core/mediaoutput.h"
// ...
std::unique_ptr<MediaOutput> m_media_output;

// mainwindow.cpp 构造函数中：
// 录制
m_media_output = std::make_unique<FileRecorder>();
// 或推流
// m_media_output = std::make_unique<StreamRecorder>();
```

---

### 5. 修改 CMakeLists.txt

在 `SOURCE_FILES` / 构建定义中：

去掉 `frontend/core/recoder.cpp`，添加：
- `frontend/core/mediaoutput.cpp`
- `frontend/core/filerecorder.cpp`
- `frontend/core/streamrecorder.cpp`

由于使用了 `file(GLOB_RECURSE ...)` 自动扫描，只需确保新文件在 `frontend/` 下即可被自动包含。

---

## 执行步骤

| 步骤 | 操作 | 涉及文件 |
|---|---|---|
| 1 | 创建 `MediaOutput` 基类头文件，从 `Recoder` 提取公共接口和成员 | `mediaoutput.h` (新建) |
| 2 | 创建 `MediaOutput` 基类实现文件，提取 `encoding_loop` 等公共方法 | `mediaoutput.cpp` (新建) |
| 3 | 将 `recoder.h` 重命名为 `filerecorder.h`，继承 `MediaOutput` | `filerecorder.h` |
| 4 | 将 `recoder.cpp` 重命名为 `filerecorder.cpp`，适配基类虚函数 | `filerecorder.cpp` |
| 5 | 创建 `StreamRecorder` 头文件和实现文件 | `streamrecorder.h`, `streamrecorder.cpp` |
| 6 | 更新 `MainWindow` 中 `Recoder` → `MediaOutput` / `FileRecorder` | `mainwindow.h`, `mainwindow.cpp` |
| 7 | 更新 `CMakeLists.txt` 移除旧文件引用（GLOB 可自动发现，但需确认） | `CMakeLists.txt` |
| 8 | 验证编译通过 | — |

---

## 风险与注意事项

1. **名称修正**：原文件名 `recoder.h` 为 typo（缺少一个 r），重构时一并修正为 `filerecorder` / `streamrecorder`
2. **AUTOMOC 兼容**：基类无需 `Q_OBJECT`，保持纯 C++ 类即可，避免 Qt MOC 依赖
3. **`open_output_and_allocate_frames` 拆分**：当前该方法同时做了"打开输出"和"分配帧缓冲区"两件事。重构后"打开输出"移到 `init_output_context()` 子类实现，"分配帧缓冲区"留在基类 `allocate_frames()` 中
4. **推流场景的 `AVFMT_NOFILE`**：部分 RTMP 输出格式可能不需要 `avio_open`，`StreamRecorder` 的 `init_output_context()` 已做判断