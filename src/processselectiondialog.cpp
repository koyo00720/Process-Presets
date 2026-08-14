#include "processselectiondialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ProcessPresets {

ProcessSelectionDialog::ProcessSelectionDialog(const QVector<ProcessInfo> &processes,
                                               QSize initialSize,
                                               QWidget *parent)
    : QDialog(parent)
    , buttonGroup_(new QButtonGroup(this))
{
    setWindowTitle(QStringLiteral("適用するプロセスを選択"));
    setModal(true);
    setMinimumSize(360, 220);
    resize(initialSize);

    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(
        QStringLiteral("同じ.exe名のプロセスが複数見つかりました。適用先のPIDを1つ選択してください。"),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *listWidget = new QWidget(scrollArea);
    auto *listLayout = new QVBoxLayout(listWidget);
    listLayout->setContentsMargins(8, 8, 8, 8);
    listLayout->setSpacing(6);

    for (int index = 0; index < processes.size(); ++index) {
        const ProcessInfo &process = processes.at(index);
        auto *radio = new QRadioButton(
            QStringLiteral("PID %1 — %2").arg(process.processId).arg(process.executableName),
            listWidget);
        radio->setProperty("processId", process.processId);
        radio->setChecked(index == 0);
        buttonGroup_->addButton(radio);
        listLayout->addWidget(radio);
    }
    listLayout->addStretch();
    scrollArea->setWidget(listWidget);
    layout->addWidget(scrollArea, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QSize ProcessSelectionDialog::dialogSize() const
{
    return size();
}

std::optional<quint32> ProcessSelectionDialog::selectedProcessId() const
{
    const auto *button = buttonGroup_->checkedButton();
    if (button == nullptr) {
        return std::nullopt;
    }
    return button->property("processId").toUInt();
}

} // namespace ProcessPresets
