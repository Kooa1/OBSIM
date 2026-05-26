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

// ==================== 控制块基类 ====================
class ControlBlock : public QWidget {
    Q_OBJECT

public:
    explicit ControlBlock(const QString &title, QWidget *parent = nullptr)
        : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        // 标题栏
        auto *title_bar = new QHBoxLayout();
        auto *icon_label = new QLabel("📋");
        auto *title_label = new QLabel(title);
        title_label->setStyleSheet("font-weight: bold; font-size: 13px;");
        title_bar->addWidget(icon_label);
        title_bar->addWidget(title_label);
        title_bar->addStretch();

        layout->addLayout(title_bar);

        // 内容区（子类填充）
        m_content_layout = new QVBoxLayout();
        layout->addLayout(m_content_layout);
    }

protected:
    QVBoxLayout *m_content_layout;
};


// ==================== 场景控制块 ====================
class SceneControlBlock : public ControlBlock {
    Q_OBJECT

public:
    explicit SceneControlBlock(QWidget *parent = nullptr);

    QListWidget *scene_list() const { return m_scene_list; }

    void add_item(const QString &name);
    void remove_item(int index);

signals:
    void scene_added(const QString &name);
    void scene_removed(int index);
    void scene_selection_changed(int index);

private:
    QListWidget *m_scene_list;
};


// ==================== 输入源控制块 ====================
class SourceControlBlock : public ControlBlock {
    Q_OBJECT

public:
    explicit SourceControlBlock(QWidget *parent = nullptr);

    QListWidget *source_list() const { return m_source_list; }

signals:
    void display_capture_requested(const CaptorConfig &config, const QString &name);

    void camera_capture_requested(const QString &device_desc, const QString &name);

    void text_source_requested(const QString &text, const QFont &font, const QColor &color, const QString &name);

    void image_source_requested(const QString &file_path, const QString &name);

    void source_remove_requested(int index);

    void source_move_up_requested(int row);

    void source_move_down_requested(int row);

    void source_list_selection_changed(int row);

private:
    QListWidget *m_source_list;
};


// ==================== 混音控制块 ====================
class AudioMixerBlock : public ControlBlock {
    Q_OBJECT

public:
    explicit AudioMixerBlock(QWidget *parent = nullptr);

    // 添加一个音轨
    void add_track(const QString &name, float volume = 1.0f);

    // 移除一个音轨
    void remove_track(const QString &name);

    // 清空所有音轨
    void clear_tracks();

    void update_track_level(const QString &name, float level);

signals:
    void track_volume_changed(const QString &name, float volume);

    void track_muted_changed(const QString &name, bool muted);

private:
    struct TrackWidget {
        QWidget *container;
        QLabel *name_label;
        QSlider *volume_slider;
        QProgressBar *level_meter;
        QPushButton *mute_btn;
    };

    QVBoxLayout *m_tracks_layout;
    std::map<QString, TrackWidget> m_tracks;

    TrackWidget create_track_widget(const QString &name, float volume);
};


// ==================== 直播录制控制块 ====================
class StreamRecordBlock : public ControlBlock {
    Q_OBJECT

public:
    explicit StreamRecordBlock(QWidget *parent = nullptr);

    QString output_path() const { return m_output_path; }

    bool is_recording() const { return m_recording; }
    bool is_streaming() const { return m_streaming; }

    void set_output_path(const QString &path) { m_output_path = path; }
    void set_stream_url(const QString &url) { m_stream_url = url; }

signals:
    void streaming_started(const QString &rtmp_url);

    void streaming_stopped();

    void recording_started(const QString &output_path);

    void recording_stopped();

private:
    QPushButton *m_btn_start_stream;
    QPushButton *m_btn_start_record;
    QPushButton *m_btn_settings;
    QPushButton *m_btn_exit;
    QString m_output_path;
    QString m_stream_url;
    bool m_recording = false;
    bool m_streaming = false;
};


// ==================== 文字源配置对话框 ====================
class TextSourceDialog : public QDialog {
    Q_OBJECT

public:
    explicit TextSourceDialog(QWidget *parent = nullptr);

    QString source_name() const;

    QString text_content() const;

    QFont selected_font() const;

    QColor selected_color() const;

private:
    QLineEdit *m_name_edit;
    QPlainTextEdit *m_text_content;
    QFontComboBox *m_font_combo;
    QSpinBox *m_size_spin;
    QPushButton *m_color_btn;
    QColor m_color;
};


// ==================== 图片源配置对话框 ====================
class ImageSourceDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImageSourceDialog(QWidget *parent = nullptr);

    QString source_name() const;
    QString file_path() const;

private:
    QLineEdit *m_name_edit;
    QLineEdit *m_path_edit;
};


// ==================== 输入源类型选择对话框 ====================
class SourceTypeDialog : public QDialog {
    Q_OBJECT

public:
    explicit SourceTypeDialog(QWidget *parent = nullptr);

    QString selected_type() const { return m_selected_type; }

private:
    QString m_selected_type;
};


// ==================== 输入源命名对话框 ====================
class SourceNameDialog : public QDialog {
    Q_OBJECT

public:
    explicit SourceNameDialog(const QVector<DisplayInfo> &displays = {},
                              const QVector<CameraInfo> &cameras = {},
                              bool is_camera_type = false,
                              QWidget *parent = nullptr);

    QString source_name() const { return m_name_edit->text().trimmed(); }

    int selected_display_index() const;

    int selected_camera_index() const;

private:
    QLineEdit *m_name_edit;
    QComboBox *m_display_combo = nullptr;
    QComboBox *m_camera_combo = nullptr;
};


// ==================== 总控制栏 ====================
class ControlBar : public QWidget {
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);

    ~ControlBar() override = default;

    SceneControlBlock *scene_control() const { return m_scene_block; }
    SourceControlBlock *source_control() const { return m_source_block; }
    AudioMixerBlock *audio_mixer() const { return m_audio_mixer_block; }
    StreamRecordBlock *stream_record() const { return m_stream_record_block; }

    void update_audio_levels(float system_level, float mic_level);

private:
    void init_UI();

    void init_control_blocks();

    void init_layout();

    QHBoxLayout *main_layout = nullptr;
    QSplitter *main_splitter = nullptr;

    SceneControlBlock *m_scene_block = nullptr;
    SourceControlBlock *m_source_block = nullptr;
    AudioMixerBlock *m_audio_mixer_block = nullptr;
    StreamRecordBlock *m_stream_record_block = nullptr;
};

#endif //OBSIM_CONTROLBAR_H
