#ifndef OBSIM_CONTROLBAR_H
#define OBSIM_CONTROLBAR_H

#include <algorithm>

#include <QWidget>

#include "../utils/PCH.h"
#include "../utils/displaymanager.h"

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
    void camera_capture_requested(const QString &name);

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

signals:
    void start_stream_clicked();

    void start_record_clicked();

private:
    QPushButton *m_btn_start_stream;
    QPushButton *m_btn_start_record;
    QPushButton *m_btn_settings;
};


// ==================== 输入源类型选择对话框 ====================
class SourceTypeDialog : public QDialog {
    Q_OBJECT

public:
    explicit SourceTypeDialog(QWidget *parent = nullptr);

    QString selectedType() const { return m_selected_type; }

private:
    QString m_selected_type;
};


// ==================== 输入源命名对话框 ====================
class SourceNameDialog : public QDialog {
    Q_OBJECT

public:
    explicit SourceNameDialog(const QVector<DisplayInfo> &displays = {}, QWidget *parent = nullptr);

    QString sourceName() const { return m_name_edit->text().trimmed(); }
    int selectedDisplayIndex() const;

private:
    QLineEdit *m_name_edit;
    QComboBox *m_display_combo = nullptr;
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
