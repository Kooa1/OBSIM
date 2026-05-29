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

/// @brief Preview widget for applying and adjusting filters on a video source
class FilterPreviewWidget : public PreviewBaseWidget {
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit FilterPreviewWidget(QWidget *parent = nullptr);
    /// @brief Set the source to filter
    /// @param source Pointer to source
    void set_source(Source *source);

signals:
    /// @brief Emitted when apply is confirmed
    void apply_confirmed();
    /// @brief Emitted when any filter parameter changes
    void filter_params_changed();

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

    OpenCVFilter *m_filter = nullptr;        ///< OpenCV filter instance
    QListWidget *m_param_list = nullptr;     ///< Parameter list widget
    QStackedWidget *m_param_stack = nullptr; ///< Parameter page stack
    FilterParams m_pending_params;           ///< Pending filter parameters
    bool m_applying = false;                 ///< Whether apply is in progress
};

#endif