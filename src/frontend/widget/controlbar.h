#ifndef OBSIM_CONTROLBAR_H
#define OBSIM_CONTROLBAR_H

#include <algorithm>

#include <QWidget>
#include <QStandardPaths>
#include <QInputDialog>

#include "../../utils/PCH.h"
#include "../../utils/devicemanager.h"
#include "../../core/textsource.h"
#include "../../core/base/videocaptor.h"
#include "settingsdialog.h"

#include <QColorDialog>
#include <QFontComboBox>
#include <QFileDialog>
#include <QMessageBox>

struct CaptorConfig;

/// @brief Base class for control blocks with a title bar and content area
class ControlBlock : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param title Block title
    /// @param parent Parent widget
    explicit ControlBlock(const QString &title, QWidget *parent = nullptr)
        : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        auto *title_bar = new QHBoxLayout();
        auto *icon_label = new QLabel("📋");
        auto *title_label = new QLabel(title);
        title_label->setObjectName("block_title");
        title_bar->addWidget(icon_label);
        title_bar->addWidget(title_label);
        title_bar->addStretch();

        layout->addLayout(title_bar);

        m_content_layout = new QVBoxLayout();
        layout->addLayout(m_content_layout);
    }

protected:
    QVBoxLayout *m_content_layout; ///< Content area layout
};


/// @brief Control block for scene management
class SceneControlBlock : public ControlBlock {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SceneControlBlock(QWidget *parent = nullptr);

    QListWidget *scene_list() const { return m_scene_list; }

    /// @brief Add a scene item
    /// @param name Scene name
    void add_item(const QString &name);
    /// @brief Remove a scene item
    /// @param index Index to remove
    void remove_item(int index);

signals:
    /// @brief Emitted when a scene is added
    /// @param name Scene name
    void scene_added(const QString &name);
    /// @brief Emitted when a scene is removed
    /// @param index Removed index
    void scene_removed(int index);
    /// @brief Emitted when scene selection changes
    /// @param index Selected index
    void scene_selection_changed(int index);

private:
    QListWidget *m_scene_list; ///< Scene list widget
};


/// @brief Control block for source management
class SourceControlBlock : public ControlBlock {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SourceControlBlock(QWidget *parent = nullptr);

    QListWidget *source_list() const { return m_source_list; }

signals:
    /// @brief Emitted when display capture is requested
    void display_capture_requested(const CaptorConfig &config, const QString &name);

    /// @brief Emitted when camera capture is requested
    void camera_capture_requested(const QString &device_desc, const QString &name);

    /// @brief Emitted when a text source is requested
    void text_source_requested(const QString &text, const QFont &font, const QColor &color, const QString &name);

    /// @brief Emitted when an image source is requested
    void image_source_requested(const QString &file_path, const QString &name);

    /// @brief Emitted when a source is to be removed
    void source_remove_requested(int index);

    /// @brief Emitted when a source is to be moved up
    void source_move_up_requested(int row);

    /// @brief Emitted when a source is to be moved down
    void source_move_down_requested(int row);

    /// @brief Emitted when source list selection changes
    void source_list_selection_changed(int row);

private:
    QListWidget *m_source_list; ///< Source list widget
};


class LevelMeterWidget;

/// @brief Control block for audio mixing
class AudioMixerBlock : public ControlBlock {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit AudioMixerBlock(QWidget *parent = nullptr);

    /// @brief Add an audio track
    /// @param name Track name
    /// @param volume Initial volume
    void add_track(const QString &name, float volume = 1.0f);
    /// @brief Remove an audio track
    /// @param name Track name
    void remove_track(const QString &name);
    /// @brief Clear all tracks
    void clear_tracks();
    /// @brief Update level meter for a track
    /// @param name Track name
    /// @param level Level value (0-1)
    void update_track_level(const QString &name, float level);
    /// @brief Set track mute state
    /// @param name Track name
    /// @param muted Muted state
    void set_track_muted(const QString &name, bool muted);
    /// @brief Sync selected audio device IDs
    void sync_audio_devices(const QString &system_device_id, const QString &mic_device_name) {
        m_selected_system_device_id = system_device_id;
        m_selected_mic_device_name = mic_device_name;
    }
    /// @brief Set track volume
    /// @param name Track name
    /// @param volume Volume value
    void set_track_volume(const QString &name, float volume);

signals:
    /// @brief Emitted when track volume changes
    void track_volume_changed(const QString &name, float volume);
    /// @brief Emitted when track mute state changes
    void track_muted_changed(const QString &name, bool muted);
    /// @brief Emitted when system audio device is selected
    void system_audio_device_selected(const QString &device_id);
    /// @brief Emitted when microphone device is selected
    void mic_audio_device_selected(const QString &device_name);

private:
    struct TrackWidget {
        QWidget *container;           ///< Track container widget
        QLabel *name_label;           ///< Track name label
        QSlider *volume_slider;       ///< Volume slider
        LevelMeterWidget *level_meter; ///< Level meter widget
        QPushButton *mute_btn;         ///< Mute button
        QPushButton *settings_btn;     ///< Settings button
    };

    QVBoxLayout *m_tracks_layout;                             ///< Tracks layout
    std::map<QString, TrackWidget> m_tracks;                  ///< Track map

    QString m_selected_system_device_id;                      ///< Selected system audio device ID
    QString m_selected_mic_device_name;                        ///< Selected microphone device name

    TrackWidget create_track_widget(const QString &name, float volume);
};


/// @brief Control block for streaming and recording
class StreamRecordBlock : public ControlBlock {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit StreamRecordBlock(QWidget *parent = nullptr);

    QString output_path() const { return m_output_path; }

    bool is_recording() const { return m_recording; }
    bool is_streaming() const { return m_streaming; }

    void set_output_path(const QString &path) { m_output_path = path; }
    void set_stream_url(const QString &url) { m_stream_url = url; }

signals:
    /// @brief Emitted when streaming starts
    void streaming_started(const QString &rtmp_url);

    /// @brief Emitted when streaming stops
    void streaming_stopped();

    /// @brief Emitted when recording starts
    void recording_started(const QString &output_path);

    /// @brief Emitted when recording stops
    void recording_stopped();

private:
    QPushButton *m_btn_start_stream;   ///< Start/stop stream button
    QPushButton *m_btn_start_record;   ///< Start/stop record button
    QPushButton *m_btn_settings;       ///< Settings button
    QPushButton *m_btn_exit;           ///< Exit button
    QString m_output_path;              ///< Recording output path
    QString m_stream_url;               ///< Streaming URL
    bool m_recording = false;           ///< Recording state
    bool m_streaming = false;           ///< Streaming state
};


/// @brief Dialog for configuring a text source
class TextSourceDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit TextSourceDialog(QWidget *parent = nullptr);

    /// @brief Get the configured source name
    /// @return Source name string
    QString source_name() const;

    /// @brief Get the configured text content
    /// @return Text content string
    QString text_content() const;

    /// @brief Get the selected font
    /// @return Selected font
    QFont selected_font() const;

    /// @brief Get the selected color
    /// @return Selected color
    QColor selected_color() const;

private:
    QLineEdit *m_name_edit;        ///< Name editor
    QPlainTextEdit *m_text_content; ///< Text content editor
    QFontComboBox *m_font_combo;   ///< Font combo box
    QSpinBox *m_size_spin;         ///< Font size spinner
    QPushButton *m_color_btn;      ///< Color picker button
    QColor m_color;                ///< Selected color
};


/// @brief Dialog for configuring an image source
class ImageSourceDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit ImageSourceDialog(QWidget *parent = nullptr);

    /// @brief Get the configured source name
    /// @return Source name string
    QString source_name() const;
    /// @brief Get the configured file path
    /// @return File path string
    QString file_path() const;

private:
    QLineEdit *m_name_edit; ///< Name editor
    QLineEdit *m_path_edit; ///< Path editor
};


/// @brief Dialog for selecting input source type
class SourceTypeDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SourceTypeDialog(QWidget *parent = nullptr);

    /// @brief Get the selected source type
    /// @return Type string
    QString selected_type() const { return m_selected_type; }

private:
    QString m_selected_type; ///< Selected type string
};


/// @brief Dialog for naming and configuring an input source
class SourceNameDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param displays Available displays
    /// @param cameras Available cameras
    /// @param is_camera_type Whether camera type
    /// @param parent Parent widget
    explicit SourceNameDialog(const QVector<DisplayInfo> &displays = {},
                              const QVector<CameraInfo> &cameras = {},
                              bool is_camera_type = false,
                              QWidget *parent = nullptr);

    QString source_name() const { return m_name_edit->text().trimmed(); }

    /// @brief Get the selected display index
    /// @return Display index
    int selected_display_index() const;

    /// @brief Get the selected camera index
    /// @return Camera index
    int selected_camera_index() const;

private:
    QLineEdit *m_name_edit;          ///< Name editor
    QComboBox *m_display_combo = nullptr; ///< Display selector
    QComboBox *m_camera_combo = nullptr;  ///< Camera selector
};


/// @brief Main control bar aggregating scene, source, audio and stream controls
class ControlBar : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit ControlBar(QWidget *parent = nullptr);

    ~ControlBar() override = default;

    SceneControlBlock *scene_control() const { return m_scene_block; }
    SourceControlBlock *source_control() const { return m_source_block; }
    AudioMixerBlock *audio_mixer() const { return m_audio_mixer_block; }
    StreamRecordBlock *stream_record() const { return m_stream_record_block; }

    /// @brief Update audio level meters
    /// @param system_level System audio level
    /// @param mic_level Microphone level
    void update_audio_levels(float system_level, float mic_level);

private:
    void init_UI();
    void init_control_blocks();
    void init_layout();

    QHBoxLayout *main_layout = nullptr;           ///< Main layout
    QSplitter *main_splitter = nullptr;            ///< Main splitter

    SceneControlBlock *m_scene_block = nullptr;          ///< Scene control block
    SourceControlBlock *m_source_block = nullptr;         ///< Source control block
    AudioMixerBlock *m_audio_mixer_block = nullptr;       ///< Audio mixer block
    StreamRecordBlock *m_stream_record_block = nullptr;   ///< Stream/record block
};

#endif //OBSIM_CONTROLBAR_H