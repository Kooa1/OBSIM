#include "settingspreviewwidget.h"
#include "../../core/cameracapturesource.h"
#include "../../core/screencapturesource.h"
#include "../../core/textsource.h"
#include "../../core/imagesource.h"
#include <QPushButton>
#include <QHBoxLayout>

SettingsPreviewWidget::SettingsPreviewWidget(QWidget *parent)
    : PreviewBaseWidget(parent) {
    setWindowTitle("设置");
    m_control_area = create_control_area();
    add_control_area();
}

void SettingsPreviewWidget::set_all_sources(const QVector<Source*> &sources) {
    m_all_sources = sources;
    populate_source_list();
}

void SettingsPreviewWidget::populate_source_list() {
    if (!m_source_combo || !m_source) return;

    m_source_combo->blockSignals(true);
    m_source_combo->clear();

    const char *current_type = m_source->source_type_name();

    for (Source *src : m_all_sources) {
        if (qstrcmp(src->source_type_name(), current_type) == 0) {
            QString label = src->display_name;
            m_source_combo->addItem(label, reinterpret_cast<quintptr>(src));
            if (src == m_source) {
                m_source_combo->setCurrentIndex(m_source_combo->count() - 1);
            }
        }
    }

    m_source_combo->blockSignals(false);

    if (m_name_edit) {
        m_name_edit->blockSignals(true);
        m_name_edit->setText(m_source->display_name);
        m_name_edit->blockSignals(false);
    }

    m_original_name = m_source->display_name;
}

QWidget* SettingsPreviewWidget::create_control_area() {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *combo_label = new QLabel("输入源选择:");
    m_source_combo = new QComboBox();
    layout->addWidget(combo_label);
    layout->addWidget(m_source_combo);

    auto *name_label = new QLabel("源名称:");
    m_name_edit = new QLineEdit();
    m_name_edit->setPlaceholderText("请输入源名称");
    layout->addWidget(name_label);
    layout->addWidget(m_name_edit);

    layout->addStretch();

    auto *btn_layout = new QHBoxLayout();
    btn_layout->addStretch();
    auto *cancel_btn = new QPushButton("取消");
    auto *apply_btn = new QPushButton("确定");
    btn_layout->addWidget(cancel_btn);
    btn_layout->addWidget(apply_btn);
    layout->addLayout(btn_layout);

    connect(m_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index < 0) return;
        Source *src = reinterpret_cast<Source*>(m_source_combo->itemData(index).value<quintptr>());
        if (!src || src == m_source) return;

        PreviewBaseWidget::set_source(src);

        m_name_edit->blockSignals(true);
        m_name_edit->setText(src->display_name);
        m_name_edit->blockSignals(false);
        m_original_name = src->display_name;
    });

    connect(m_name_edit, &QLineEdit::textChanged, this, [](const QString &) {});

    connect(apply_btn, &QPushButton::clicked, this, &SettingsPreviewWidget::on_apply);
    connect(cancel_btn, &QPushButton::clicked, this, &SettingsPreviewWidget::on_cancel);

    return container;
}

void SettingsPreviewWidget::on_apply() {
    QString new_name = m_name_edit->text().trimmed();
    if (m_source && !new_name.isEmpty() && new_name != m_original_name) {
        emit source_name_changed(m_source, new_name);
    }
    close();
}

void SettingsPreviewWidget::on_cancel() {
    if (m_name_edit) {
        m_name_edit->setText(m_original_name);
    }
    close();
}