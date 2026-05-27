#ifndef OBSIM_FILTERPREVIEWWIDGET_H
#define OBSIM_FILTERPREVIEWWIDGET_H

#include "../base/previewbasewidget.h"
#include "../../core/filter/opencvfilter.h"
#include <QStackedWidget>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>

class OpenCVFilter;

class FilterPreviewWidget : public PreviewBaseWidget {
    Q_OBJECT
public:
    explicit FilterPreviewWidget(QWidget *parent = nullptr);
    void set_source(Source *source);

signals:
    void apply_confirmed();

protected:
    QWidget* create_control_area() override;
    void on_frame_update() override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void on_item_clicked(int currentRow);
    void apply_flip_code(int code);
    void apply_brightness(int value);
    void apply_contrast(int value);
    void apply_saturation(int value);
    void on_reset();
    void on_apply();

private:
    void init_filter();
    void inject_current_params();
    void build_param_pages();
    QWidget* create_flip_page();
    QWidget* create_grayscale_page();
    QWidget* create_color_adjust_page();
    QWidget* create_no_param_page(const char *text);

    OpenCVFilter *m_filter = nullptr;
    QListWidget *m_param_list = nullptr;
    QStackedWidget *m_param_stack = nullptr;
    FilterParams m_pending_params;
    bool m_applying = false;
};

#endif