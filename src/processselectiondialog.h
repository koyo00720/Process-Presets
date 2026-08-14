#pragma once

#include "processhelper.h"

#include <QDialog>

#include <optional>

class QButtonGroup;

namespace ProcessPresets {

class ProcessSelectionDialog final : public QDialog {
public:
    explicit ProcessSelectionDialog(const QVector<ProcessInfo> &processes,
                                    QSize initialSize,
                                    QWidget *parent = nullptr);

    std::optional<quint32> selectedProcessId() const;
    QSize dialogSize() const;

private:
    QButtonGroup *buttonGroup_ = nullptr;
};

} // namespace ProcessPresets
