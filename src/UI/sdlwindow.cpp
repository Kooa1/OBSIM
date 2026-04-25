//
// Created by 66 on 2025/8/22.
//

#include "UI/sdlwindow.h"

#include "UI/mainwindow.h"

SDLWindow::SDLWindow(QWidget *parent)
    : QWidget(parent),
      wind_wid(1920), wind_hei(1080),
      sdl_wind_size(QSize(wind_wid, wind_hei)),
      sdl_initialized(false),
      sdl_rendering(false),
      sdl_pause(false),
      integer_remain(0),
      float_remain(0.0),
      previous_clock(0.0),
      volume(50) {
    // syn_wrapper = new SynWrapper(this);
    // player_core = std::make_unique<PlayerCore>(*syn_wrapper);

    init_content_policy();
}

SDLWindow::~SDLWindow() {
    destroy();
}

const bool &SDLWindow::get_rendering_symbol() const {
    return sdl_rendering;
}

const bool &SDLWindow::get_rendering_pause_symbol() const {
    return sdl_pause;
}

const bool &SDLWindow::get_sdl_initialized_symbol() const {
    return sdl_initialized;
}

// void SDLWindow::set_rendering_pause_symbol(const bool flag) {
//     this->sdl_pause = flag;
//     player_core->set_pause_symbol(sdl_pause);
// }

// void SDLWindow::stop_stream_line() const {
//     player_core->stop_stream_line();
// }

// void SDLWindow::set_seek_symbol(const bool flag, const int target_second) const {
//     player_core->set_seek_symbol(flag, target_second);
// }

// QString SDLWindow::open_file_dialog() {
//     const QString desktop_path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
//     const QString filter =
//             "Media Files (*.mp3 *.wav *.flac *.aac *.ogg *.mp4 *.avi *.mkv *.mov *.wmv *.flv *.m4a *.webm);;"
//             "Audio Files (*.mp3 *.wav *.flac *.aac *.ogg *.m4a);;"
//             "Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm);;"
//             "All Files (*.*)";
//
//     const QString file_path_qstr = QFileDialog::getOpenFileName(nullptr, "选择文件", desktop_path, filter);
//     return file_path_qstr;
// }

// bool SDLWindow::open_file() {
//     const QString desktop_path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
//     const QString filter =
//             "Media Files (*.mp3 *.wav *.flac *.aac *.ogg *.mp4 *.avi *.mkv *.mov *.wmv *.flv *.m4a *.webm);;"
//             "Audio Files (*.mp3 *.wav *.flac *.aac *.ogg *.m4a);;"
//             "Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm);;"
//             "All Files (*.*)";
//
//     const QString file_path_qstr = QFileDialog::getOpenFileName(nullptr, "选择文件", desktop_path, filter);
//     // const QString file_path_qstr = "/Users/wuwenze/Desktop/3《4K60帧》APink+-+NoNoNo+++Mr.Chu.mp4";
//
//     if (!file_path_qstr.isEmpty()) {
//         file_path = file_path_qstr.toStdString();
//         if (player_core->init_context(file_path)) {
//             const auto &[audio, video] = player_core->get_stream_state();
//             if (audio) {
//                 init_SDL_desired(player_core->get_audio_info());
//             }
//
//             if (video) {
//                 const std::tuple<int, int, double> base_info(player_core->get_base_info());
//                 sdl_change_size(std::get<0>(base_info), std::get<1>(base_info));
//                 create_sdl_texture(std::get<0>(base_info), std::get<1>(base_info));
//                 emit whole_video_duration(std::get<2>(base_info));
//             }
//
//             if (!video) {
//                 emit whole_video_duration(player_core->audio_info.get_audio_duration());
//             }
//
//             player_core->init_decoder();
//             player_core->start_task();
//
//             SDL_PauseAudio(0);
//
//             return true;
//         }
//     }
//     return false;
// }

// bool SDLWindow::open_file(const QString &file_name) {
//     if (!file_name.isEmpty()) {
//         file_path = file_name.toStdString();
//         if (player_core->init_context(file_path)) {
//             const auto &[audio, video] = player_core->get_stream_state();
//             if (audio) {
//                 init_SDL_desired(player_core->get_audio_info());
//             }
//
//             if (video) {
//                 const std::tuple<int, int, double> base_info(player_core->get_base_info());
//                 sdl_change_size(std::get<0>(base_info), std::get<1>(base_info));
//                 create_sdl_texture(std::get<0>(base_info), std::get<1>(base_info));
//                 emit whole_video_duration(std::get<2>(base_info));
//             }
//
//             if (!video) {
//                 emit whole_video_duration(player_core->audio_info.get_audio_duration());
//             }
//
//             player_core->init_decoder();
//             player_core->start_task();
//
//             SDL_PauseAudio(0);
//
//             return true;
//         }
//     }
//     return false;
// }

// void SDLWindow::start_rendering() {
//     if (!sdl_initialized && !init_SDL()) {
//         return;
//     }
//
//     if (sdl_rendering) {
//         return;
//     }
//
//     sdl_rendering = true;
//     sdl_pause = false;
//
//     if (const auto &[audio, video] = player_core->get_stream_state(); video) {
//     setAttribute(Qt::WA_PaintOnScreen, true);
//     }
// }

// void SDLWindow::stop_rendering() {
//     if (!sdl_rendering) {
//         return;
//     }
//
//     sdl_rendering = false;
//     sdl_pause = false;
//
//     if (this->sizeHint() != QSize(300, 200)) {
//         sdl_change_size(300, 200);
//     }
//
//     if (const auto &[audio, video] = player_core->get_stream_state(); video) {
//         setAttribute(Qt::WA_PaintOnScreen, false);
//     }
//
//     if (sdl_renderer) {
//         // 停止渲染时绘制黑色背景
//         render_pure_color(0, 0, 0, 255);
//     }
// }

void SDLWindow::shutdown_audio_device() {
    SDL_CloseAudio();
}

void SDLWindow::render_frame(std::optional<AVFramePtr> frame) {
    if (!frame.has_value() || !frame.value()) {
        render_pure_color(0, 0, 0, 255);
        play_over();
        return;
    }

    // return;

    const auto raw_frame = frame->get();

    if (const int ret = SDL_UpdateYUVTexture(sdl_texture, nullptr,
                                             raw_frame->data[0], raw_frame->linesize[0],
                                             raw_frame->data[1], raw_frame->linesize[1],
                                             raw_frame->data[2], raw_frame->linesize[2]); ret != 0) {
        return;
    }

    SDL_RenderClear(sdl_renderer);

    const SDL_Rect dest_rect = resize_sdl_texture(raw_frame->width, raw_frame->height, this->width(), this->height());

    SDL_RenderCopy(sdl_renderer, sdl_texture, nullptr, &dest_rect);
    SDL_RenderPresent(sdl_renderer);
}

void SDLWindow::render_pure_color(const int r, const int g, const int b, const int a) const {
    SDL_SetRenderDrawColor(sdl_renderer, r, g, b, a);
    SDL_RenderClear(sdl_renderer);
    SDL_RenderPresent(sdl_renderer);
}

void SDLWindow::set_volume(const int value) {
    volume = value;
}

void SDLWindow::silent_volume() {
    silent = volume;
    volume = 0;
}

void SDLWindow::restore_volume() {
    volume = silent;
}

bool SDLWindow::init_SDL() {
    if (sdl_initialized) {
        return true;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        qDebug() << "SDL_Init failed:" << SDL_GetError();
        return false;
    }
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        qDebug() << "SDL_Init failed:" << SDL_GetError();
        return false;
    }

    if (!this->winId()) {
        qDebug() << "Window ID not available yet";
        return false;
    }

    sdl_window = SDL_CreateWindowFrom(reinterpret_cast<void *>(winId()));
    if (!sdl_window) {
        qDebug() << "SDL_CreateWindowFrom failed:" << SDL_GetError();
        destroy();
        SDL_Quit();
        return false;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer) {
        qDebug() << "SDL_CreateRenderer failed:" << SDL_GetError();
        destroy();
        sdl_window = nullptr;
        SDL_Quit();
        return false;
    }

    SDL_zero(sdl_desired);

    // init_conn();

#if defined(Q_OS_WIN32)
    render_pure_color(0, 0, 0, 255);
#elif defined(Q_OS_MAC)
    SDL_PumpEvents();
#endif

    sdl_initialized = true;
    return true;
}

bool SDLWindow::init_SDL_desired(std::tuple<int, uint16_t, int, SDL_AudioFormat> audio_tuple) {
    sdl_desired.channels = std::get<0>(audio_tuple);
    sdl_desired.format = std::get<1>(audio_tuple);
    sdl_desired.freq = std::get<2>(audio_tuple);
    sdl_desired.samples = std::get<3>(audio_tuple);
    // sdl_desired.callback = &SDLWindow::audio_callback;
    sdl_desired.userdata = this;

    // cout << "freq : " << sdl_desired.freq << "\n";
    // cout << "samples : " << sdl_desired.samples << "\n";

    if (SDL_OpenAudio(&sdl_desired, nullptr) < 0) {
        std::cerr << "SDL_OpenAudio FAILED: " << SDL_GetError() << std::endl;
        return false;
    }
    return true;
}

void SDLWindow::init_content_policy() {
    this->setAttribute(Qt::WA_NativeWindow);
    this->setAttribute(Qt::WA_OpaquePaintEvent, true);
    this->setAttribute(Qt::WA_NoSystemBackground, true);
    this->setFocusPolicy(Qt::StrongFocus);
}

// void SDLWindow::init_conn() {
//     connect(syn_wrapper, &SynWrapper::sync_render_data, this, &SDLWindow::render_frame, Qt::QueuedConnection);
// }

void SDLWindow::clean_player_core() {
    if (sdl_texture) {
        SDL_DestroyTexture(sdl_texture);
    }
}

void SDLWindow::destroy() {
    if (sdl_texture) {
        SDL_DestroyTexture(sdl_texture);
    }
    if (sdl_renderer) {
        SDL_DestroyRenderer(sdl_renderer);
    }
    if (sdl_window) {
        sdl_window = nullptr;
    }
    if (sdl_initialized) {
        SDL_Quit();
    }
}

void SDLWindow::sdl_change_size(const int w, const int h) {
    sdl_wind_size = QSize(w, h);
    // this->sizeHint();

    this->updateGeometry();
    emit size_hint_change();
}

void SDLWindow::create_sdl_texture(const int video_wid, const int video_hei) {
    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, video_wid,
                                    video_hei);
    if (!sdl_texture) {
        SDL_Log("create sdl texture failed");
        destroy();
    }
}

SDL_Rect SDLWindow::resize_sdl_texture(const int frame_w, const int frame_h, const int window_w, const int window_h) {
    SDL_Rect rect;

    const float frame_aspect = static_cast<float>(frame_w) / frame_h;

    if (const float window_aspect = static_cast<float>(window_w) / window_h; frame_aspect > window_aspect) {
        // 视频更宽，以宽度为准
        rect.w = window_w;
        rect.h = static_cast<int>(window_w / frame_aspect);
        rect.x = 0;
        rect.y = (window_h - rect.h) / 2;
    } else {
        // 视频更高，以高度为准
        rect.h = window_h;
        rect.w = static_cast<int>(window_h * frame_aspect);
        rect.x = (window_w - rect.w) / 2;
        rect.y = 0;
    }

    return rect;
}

// void SDLWindow::audio_callback(void *userdata, Uint8 *stream, int len) {
//     auto *sdl_obj = static_cast<SDLWindow *>(userdata);
//     if (!sdl_obj || !sdl_obj->player_core) {
//         SDL_memset(stream, 0, len);
//         return;
//     }
//
//     const auto &a_q = sdl_obj->player_core->get_audio_queue();
//     if (!a_q) {
//         SDL_memset(stream, 0, len);
//         return;
//     }
//
//     const auto data = a_q->pop();
//     if (!data.has_value() || data.value().empty()) {
//         SDL_memset(stream, 0, len);
//         sdl_obj->play_over();
//         return;
//     }
//
//     const auto &frame = data.value();
//     const auto frame_data = reinterpret_cast<const Uint8 *>(frame.data());
//     const int frame_size = static_cast<int>(frame.size());
//     const int mix_len = std::min(len, frame_size);
//
//     const int bytes_sample = SDL_AUDIO_BITSIZE(sdl_obj->sdl_desired.format) / 8;
//     const int channels = sdl_obj->sdl_desired.channels;
//     const int sample = mix_len / (bytes_sample * channels);
//
//     auto &clock = sdl_obj->player_core->get_audio_clock();
//     clock.fetch_add(static_cast<double>(sample) / sdl_obj->sdl_desired.freq);
//
//     sdl_obj->calculate_diff(clock.load());
//
//     SDL_memset(stream, 0, len);
//     // SDL_MixAudio(stream, frame_data, mix_len, SDL_MIX_MAXVOLUME);
//     SDL_MixAudioFormat(stream, frame_data, sdl_obj->sdl_desired.format, mix_len, sdl_obj->volume);
// }

void SDLWindow::calculate_diff(const double audio_clock) {
    const double diff = audio_clock - previous_clock;

    if (diff < 0) {
        previous_clock = audio_clock;
        float_remain = 0.0;
        return;
    }

    if (diff >= 1.0) {
        previous_clock = audio_clock;
        float_remain = 0.0;
        return;
    }

    float_remain += diff;

    while (float_remain >= 1.0) {
        emit add_second();
        float_remain -= 1.0;
    }

    previous_clock = audio_clock;
}

QSize SDLWindow::sizeHint() const {
    return sdl_wind_size.isValid() ? sdl_wind_size : QSize(300, 200);
}

void SDLWindow::paintEvent(QPaintEvent *event) {
    if (!sdl_rendering) {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
    }
}

void SDLWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!sdl_initialized) {
        init_SDL();
    }
}

QPaintEngine *SDLWindow::paintEngine() const {
    return nullptr;
}
