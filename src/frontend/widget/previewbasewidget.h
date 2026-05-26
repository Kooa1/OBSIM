#ifndef OBSIM_PREVIEWBASEWIDGET_H
#define OBSIM_PREVIEWBASEWIDGET_H

#include <QTimer>
#include <QSplitter>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QtOpenGLWidgets/QOpenGLWidget>

class Source;

class PreviewBaseWidget : public QWidget {
    Q_OBJECT
public:
    explicit PreviewBaseWidget(QWidget *parent = nullptr);
    ~PreviewBaseWidget() override;

    void set_source(Source *source);
    void start_preview();
    void stop_preview();

signals:
    void close_requested();

protected:
    void closeEvent(QCloseEvent *event) override;
    virtual QWidget* create_control_area() = 0;
    void add_control_area();

    Source *m_source = nullptr;
    QTimer *m_render_timer = nullptr;
    QSplitter *m_splitter = nullptr;
    QWidget *m_control_area = nullptr;

private:
    class PreviewGLWidget : public QOpenGLWidget {
    public:
        explicit PreviewGLWidget(PreviewBaseWidget *owner, QWidget *parent = nullptr);
    protected:
        void initializeGL() override;
        void resizeGL(int w, int h) override;
        void paintGL() override;
    private:
        PreviewBaseWidget *m_owner;
    };
    PreviewGLWidget *m_gl_widget = nullptr;
};

#endif