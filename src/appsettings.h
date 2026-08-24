#pragma once

#include <QLocale>
#include <QSize>
#include <QString>

namespace ProcessPresets {

enum class ThemeMode { System, Light, Dark };
enum class LanguageMode { System, Japanese, English };

struct UiSettings {
    ThemeMode themeMode = ThemeMode::System;
    LanguageMode languageMode = LanguageMode::System;
    int cpuColumnWidth = 62;
    int enabledColumnWidth = 64;
    int executableColumnWidth = 180;
    int applyOnceColumnWidth = 82;
    int periodicColumnWidth = 82;
    int resetColumnWidth = 82;
    int orderColumnWidth = 82;
    int processPriorityColumnWidth = 110;
    int pidColumnWidth = 88;
    int processStartColumnWidth = 150;
    int processCpuColumnWidth = 82;
    int processMemoryColumnWidth = 100;
    bool showProcessStartColumn = true;
    bool showProcessCpuColumn = true;
    bool showProcessMemoryColumn = true;
    bool showCpuUsageRow = true;
    int tableFontPointSize = 10;
    int tableRowHeight = 34;
    int cpuUsageRowHeight = 56;
    int cpuUsageBarWidth = 28;
    QString cpuUsageBarColor;
    int cpuUsageBarTransparencyPercent = 0;
    int separatorLineThickness = 1;
    int separatorLineTransparencyPercent = 50;
    int performanceUpdateIntervalSeconds = 1;
    int periodicApplyIntervalSeconds = 10;
    QSize mainWindowSize = QSize(1000, 600);
    QSize settingsDialogSize = QSize(440, 460);
    QSize cpuStructureDialogSize = QSize(820, 600);
    QSize processSelectionDialogSize = QSize(430, 280);
};

inline bool usesEnglish(const UiSettings &settings)
{
    if (settings.languageMode == LanguageMode::English) {
        return true;
    }
    if (settings.languageMode == LanguageMode::Japanese) {
        return false;
    }
    return QLocale::system().language() == QLocale::English;
}

inline QString localizedText(const UiSettings &settings,
                             const QString &japanese,
                             const QString &english)
{
    return usesEnglish(settings) ? english : japanese;
}

} // namespace ProcessPresets
