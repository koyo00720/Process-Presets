#pragma once

#include "appsettings.h"

#include <QDialog>

#include <functional>

class QRadioButton;
class QSpinBox;
class QCheckBox;
class QPushButton;

namespace ProcessPresets {

class SettingsDialog final : public QDialog {
public:
    explicit SettingsDialog(const UiSettings &settings,
                            std::function<void()> deleteCustomCpuStructures,
                            std::function<void(const UiSettings &)> applySettings,
                            QWidget *parent = nullptr);

    UiSettings settings() const;
    bool deleteCustomCpuStructuresRequested() const;

private:
    void updateCpuUsageBarColorButton();

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
    QSpinBox *pidColumnWidthSpinBox_ = nullptr;
    QSpinBox *processStartColumnWidthSpinBox_ = nullptr;
    QSpinBox *processCpuColumnWidthSpinBox_ = nullptr;
    QSpinBox *processMemoryColumnWidthSpinBox_ = nullptr;
    QCheckBox *showProcessStartColumnCheckBox_ = nullptr;
    QCheckBox *showProcessCpuColumnCheckBox_ = nullptr;
    QCheckBox *showProcessMemoryColumnCheckBox_ = nullptr;
    QCheckBox *showCpuUsageRowCheckBox_ = nullptr;
    QSpinBox *tableFontPointSizeSpinBox_ = nullptr;
    QSpinBox *tableRowHeightSpinBox_ = nullptr;
    QSpinBox *cpuUsageRowHeightSpinBox_ = nullptr;
    QSpinBox *cpuUsageBarWidthSpinBox_ = nullptr;
    QPushButton *cpuUsageBarColorButton_ = nullptr;
    QSpinBox *cpuUsageBarTransparencySpinBox_ = nullptr;
    QSpinBox *separatorLineThicknessSpinBox_ = nullptr;
    QSpinBox *separatorLineTransparencySpinBox_ = nullptr;
    QSpinBox *performanceUpdateIntervalSpinBox_ = nullptr;
    QSpinBox *periodicApplyIntervalSpinBox_ = nullptr;
    QString cpuUsageBarColor_;
    bool deleteCustomCpuStructuresRequested_ = false;
    std::function<void()> deleteCustomCpuStructures_;
    std::function<void(const UiSettings &)> applySettings_;
};

} // namespace ProcessPresets
