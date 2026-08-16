#pragma once

#include "appsettings.h"

#include <QDialog>

#include <functional>

class QRadioButton;
class QSpinBox;

namespace ProcessPresets {

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(const UiSettings &settings,
                            std::function<void()> deleteCustomCpuStructures,
                            QWidget *parent = nullptr);

    UiSettings settings() const;
    bool deleteCustomCpuStructuresRequested() const;

private:
    UiSettings initialSettings_;
    QRadioButton *systemThemeButton_ = nullptr;
    QRadioButton *lightThemeButton_ = nullptr;
    QRadioButton *darkThemeButton_ = nullptr;
    QRadioButton *systemLanguageButton_ = nullptr;
    QRadioButton *japaneseLanguageButton_ = nullptr;
    QRadioButton *englishLanguageButton_ = nullptr;
    QSpinBox *cpuColumnWidthSpinBox_ = nullptr;
    QSpinBox *enabledColumnWidthSpinBox_ = nullptr;
    QSpinBox *executableColumnWidthSpinBox_ = nullptr;
    QSpinBox *applyOnceColumnWidthSpinBox_ = nullptr;
    QSpinBox *periodicColumnWidthSpinBox_ = nullptr;
    QSpinBox *resetColumnWidthSpinBox_ = nullptr;
    QSpinBox *orderColumnWidthSpinBox_ = nullptr;
    QSpinBox *processPriorityColumnWidthSpinBox_ = nullptr;
    QSpinBox *tableFontPointSizeSpinBox_ = nullptr;
    QSpinBox *periodicApplyIntervalSpinBox_ = nullptr;
    bool deleteCustomCpuStructuresRequested_ = false;
    std::function<void()> deleteCustomCpuStructures_;
};

} // namespace ProcessPresets
