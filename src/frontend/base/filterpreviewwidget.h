#ifndef OBSIM_FILTERPREVIEWWIDGET_H
#define OBSIM_FILTERPREVIEWWIDGET_H

#include "../widget/previewbasewidget.h"
#include <QLabel>
#include <QVBoxLayout>

class FilterPreviewWidget : public PreviewBaseWidget {
    Q_OBJECT
public:
    explicit FilterPreviewWidget(QWidget *parent = nullptr);

protected:
    QWidget* create_control_area() override;
};

#endif