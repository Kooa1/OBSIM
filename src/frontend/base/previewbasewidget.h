#ifndef OBSIM_PREVIEWBASEWIDGET_H
#define OBSIM_PREVIEWBASEWIDGET_H

#include <QTimer>
#include <QSplitter>
#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QtOpenGLWidgets/QOpenGLWidget>

class Source;

/// @brief Base widget for OpenGL-based preview windows
class PreviewBaseWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit PreviewBaseWidget(QWidget *parent = nullptr);
    ~PreviewBaseWidget() override;

    /// @brief Set the source to preview
    /// @param source Pointer to source
    void set_source(Source *source);
    /// @brief Start preview rendering
    void start_preview();
    /// @brief Stop preview rendering
    void stop_preview();

signals:
    /// @brief Emitted when close is requested
    void close_requested();

protected:
    void closeEvent(QCloseEvent *event) override;
    /// @brief Create control area (to be implemented by subclass)
    /// @return Pointer to created widget
    virtual QWidget* create_control_area() = 0;
    /// @brief Add control area to layout
    void add_control_area();

    /// @brief Structure for split control panel
    struct SplitControlPanel {
        QSplitter *splitter;           ///< Main splitter
        QListWidget *list_widget;      ///< Left list widget
        QWidget *right_panel;          ///< Right detail panel
        QPushButton *btn_add;          ///< Add button
        QPushButton *btn_remove;       ///< Remove button
    };
    SplitControlPanel create_split_panel(const QString &title, QWidget *parent);

    Source *m_source = nullptr;               ///< Source being previewed
    QTimer *m_render_timer = nullptr;         ///< Render timer
    QSplitter *m_splitter = nullptr;          ///< Main splitter
    QWidget *m_control_area = nullptr;        ///< Control area widget

    /// @brief Called on frame update
    virtual void on_frame_update();

private:
    /// @brief Internal OpenGL widget for rendering
    class PreviewGLWidget : public QOpenGLWidget {
    public:
        explicit PreviewGLWidget(PreviewBaseWidget *owner, QWidget *parent = nullptr);
    protected:
        void initializeGL() override;
        void resizeGL(int w, int h) override;
        void paintGL() override;
    private:
        PreviewBaseWidget *m_owner; ///< Owning preview widget
    };
    PreviewGLWidget *m_gl_widget = nullptr; ///< OpenGL widget
};

#endif