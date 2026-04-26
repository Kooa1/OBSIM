//
// Created by 66 on 2025/8/22.
//

#ifndef ZPLAYER_SDLWINDOW_H
#define ZPLAYER_SDLWINDOW_H

#include <thread>

#include "../PCH.h"
#include "../Utils/ffmpegfactory.h"

extern "C" {
#include <SDL.h>
};

class SDLWindow : public QWidget {
    Q_OBJECT

public:
    explicit SDLWindow(QWidget *parent = nullptr);

    ~SDLWindow() override;

    const bool &get_rendering_symbol() const;

    const bool &get_rendering_pause_symbol() const;

    const bool &get_sdl_initialized_symbol() const;

    // void set_rendering_pause_symbol(bool flag);

    // void stop_stream_line() const;

    // void set_seek_symbol(bool flag, int target_second) const;

    // QString open_file_dialog();

public slots:
    // bool open_file();

    // bool open_file(const QString &file_name);

    // void start_rendering();

    // void stop_rendering();

    void shutdown_audio_device();

    void render_frame(std::optional<AVFramePtr> frame);

    void render_pure_color(int r, int g, int b, int a) const;

    void set_volume(int value);

    void silent_volume();

    void restore_volume();

private:
    bool init_SDL();

    bool init_SDL_desired(std::tuple<int, uint16_t, int, SDL_AudioFormat>);

    void init_content_policy();

    // void init_conn();

    void clean_player_core();

    void destroy();

    void sdl_change_size(int w, int h);

    void create_sdl_texture(int video_wid, int video_hei);

    static SDL_Rect resize_sdl_texture(int frame_w, int frame_h, int window_w, int window_h);

    // static void audio_callback(void *userdata, Uint8 *stream, int len);

    void calculate_diff(double audio_clock);

private:
    std::string file_path;

    SDL_Window *sdl_window = nullptr;
    SDL_Renderer *sdl_renderer = nullptr;
    SDL_Texture *sdl_texture = nullptr;

    SDL_AudioSpec sdl_desired;

    int wind_wid, wind_hei;
    QSize sdl_wind_size;

    bool sdl_initialized;
    bool sdl_rendering;
    bool sdl_pause;

    int integer_remain;
    double float_remain;
    double previous_clock;

    int volume;
    int silent;

private:
    // std::unique_ptr<PlayerCore> player_core;

    // SynWrapper *syn_wrapper;

signals:
    void size_hint_change();

    void whole_video_duration(double video_duration);

    void play_over();

    void accumulate_total_sample(int sample);

    void add_second();

protected:
    QSize sizeHint() const override;

    void paintEvent(QPaintEvent *event) override;

    void showEvent(QShowEvent *event) override;

    QPaintEngine *paintEngine() const override;
};

#endif //ZPLAYER_SDLWINDOW_H
