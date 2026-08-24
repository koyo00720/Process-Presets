#include "settingsrepository.h"

#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace ProcessPresets {
namespace {

QString defaultConfigFilePath()
{
    const QDir applicationDirectory(QCoreApplication::applicationDirPath());
    if (QFileInfo(applicationDirectory.absolutePath()).isWritable()) {
        return applicationDirectory.filePath(QStringLiteral("ProcessPresets.conf"));
    }

    const QString configDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDirectory);
    return QDir(configDirectory).filePath(QStringLiteral("ProcessPresets.conf"));
}

QJsonDocument documentFromSetting(const QVariant &setting)
{
    return QJsonDocument::fromJson(setting.toString().toUtf8());
}

QString compactJson(const QJsonArray &array)
{
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QString themeModeToString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Dark:
        return QStringLiteral("dark");
    case ThemeMode::System:
        return QStringLiteral("system");
    }
    return QStringLiteral("system");
}

ThemeMode themeModeFromString(const QString &value)
{
    if (value == QStringLiteral("light")) {
        return ThemeMode::Light;
    }
    if (value == QStringLiteral("dark")) {
        return ThemeMode::Dark;
    }
    return ThemeMode::System;
}

QString languageModeToString(LanguageMode mode)
{
    switch (mode) {
    case LanguageMode::Japanese:
        return QStringLiteral("japanese");
    case LanguageMode::English:
        return QStringLiteral("english");
    case LanguageMode::System:
        return QStringLiteral("system");
    }
    return QStringLiteral("system");
}

LanguageMode languageModeFromString(const QString &value)
{
    if (value == QStringLiteral("japanese")) {
        return LanguageMode::Japanese;
    }
    if (value == QStringLiteral("english")) {
        return LanguageMode::English;
    }
    return LanguageMode::System;
}

} // namespace

SettingsRepository::SettingsRepository()
    : filePath_(defaultConfigFilePath())
    , settings_(filePath_, QSettings::IniFormat)
{
}

QString SettingsRepository::configFilePath() const
{
    return filePath_;
}

UiSettings SettingsRepository::loadUiSettings() const
{
    UiSettings result;
    result.themeMode = themeModeFromString(
        settings_.value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString());
    result.languageMode = languageModeFromString(
        settings_.value(QStringLiteral("ui/language"), QStringLiteral("system")).toString());
    result.cpuColumnWidth = qBound(
        1, settings_.value(QStringLiteral("ui/cpuColumnWidth"), result.cpuColumnWidth).toInt(), 600);
    // The former single Apply width is retained as the migration fallback for
    // configuration files written before the individual action-column widths.
    const int legacyActionColumnWidth = qBound(
        1, settings_.value(QStringLiteral("ui/applyColumnWidth"), result.applyOnceColumnWidth).toInt(), 600);
    const auto actionColumnWidth = [this, legacyActionColumnWidth](const QString &key) {
        return qBound(1, settings_.value(key, legacyActionColumnWidth).toInt(), 600);
    };
    result.enabledColumnWidth = actionColumnWidth(QStringLiteral("ui/enabledColumnWidth"));
    result.applyOnceColumnWidth = actionColumnWidth(QStringLiteral("ui/applyOnceColumnWidth"));
    result.periodicColumnWidth = actionColumnWidth(QStringLiteral("ui/periodicColumnWidth"));
    result.resetColumnWidth = actionColumnWidth(QStringLiteral("ui/resetColumnWidth"));
    result.orderColumnWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/orderColumnWidth"),
                        settings_.value(QStringLiteral("ui/priorityColumnWidth"),
                                        result.orderColumnWidth)).toInt(),
        600);
    result.processPriorityColumnWidth = actionColumnWidth(
        QStringLiteral("ui/processPriorityColumnWidth"));
    result.pidColumnWidth = actionColumnWidth(QStringLiteral("ui/pidColumnWidth"));
    result.processStartColumnWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/processStartColumnWidth"),
                        result.processStartColumnWidth).toInt(),
        600);
    result.processCpuColumnWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/processCpuColumnWidth"),
                        result.processCpuColumnWidth).toInt(),
        600);
    result.processMemoryColumnWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/processMemoryColumnWidth"),
                        result.processMemoryColumnWidth).toInt(),
        600);
    result.showProcessStartColumn = settings_.value(
        QStringLiteral("ui/showProcessStartColumn"), result.showProcessStartColumn).toBool();
    result.showProcessCpuColumn = settings_.value(
        QStringLiteral("ui/showProcessCpuColumn"), result.showProcessCpuColumn).toBool();
    result.showProcessMemoryColumn = settings_.value(
        QStringLiteral("ui/showProcessMemoryColumn"), result.showProcessMemoryColumn).toBool();
    result.showCpuUsageRow = settings_.value(
        QStringLiteral("ui/showCpuUsageRow"), result.showCpuUsageRow).toBool();
    result.executableColumnWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/executableColumnWidth"), result.executableColumnWidth).toInt(),
        600);
    result.tableFontPointSize = qBound(
        6, settings_.value(QStringLiteral("ui/tableFontPointSize"), result.tableFontPointSize).toInt(), 48);
    result.tableRowHeight = qBound(
        1, settings_.value(QStringLiteral("ui/tableRowHeight"), result.tableRowHeight).toInt(), 600);
    result.cpuUsageRowHeight = qBound(
        20,
        settings_.value(QStringLiteral("ui/cpuUsageRowHeight"), result.cpuUsageRowHeight).toInt(),
        300);
    result.cpuUsageBarWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/cpuUsageBarWidth"), result.cpuUsageBarWidth).toInt(),
        600);
    const QColor cpuUsageBarColor(settings_.value(
        QStringLiteral("ui/cpuUsageBarColor"), result.cpuUsageBarColor).toString());
    result.cpuUsageBarColor = cpuUsageBarColor.isValid()
        ? cpuUsageBarColor.name(QColor::HexRgb)
        : QString();
    result.cpuUsageBarTransparencyPercent = qBound(
        0,
        settings_.value(QStringLiteral("ui/cpuUsageBarTransparencyPercent"),
                        result.cpuUsageBarTransparencyPercent).toInt(),
        100);
    result.separatorLineThickness = qBound(
        1,
        settings_.value(QStringLiteral("ui/separatorLineThickness"),
                        result.separatorLineThickness).toInt(),
        20);
    result.separatorLineTransparencyPercent = qBound(
        0,
        settings_.value(QStringLiteral("ui/separatorLineTransparencyPercent"),
                        result.separatorLineTransparencyPercent).toInt(),
        100);
    result.performanceUpdateIntervalSeconds = qBound(
        1,
        settings_.value(QStringLiteral("ui/performanceUpdateIntervalSeconds"),
                        result.performanceUpdateIntervalSeconds).toInt(),
        3600);
    result.periodicApplyIntervalSeconds = qBound(
        1,
        settings_.value(QStringLiteral("ui/periodicApplyIntervalSeconds"),
                        result.periodicApplyIntervalSeconds).toInt(),
        3600);
    const auto boundedSize = [this](const QString &key, const QSize &fallback,
                                    int minimumWidth, int minimumHeight) {
        const QSize savedSize = settings_.value(key, fallback).toSize();
        return QSize(qBound(minimumWidth, savedSize.width(), 2400),
                     qBound(minimumHeight, savedSize.height(), 1600));
    };
    result.mainWindowSize = boundedSize(
        QStringLiteral("ui/mainWindowSize"), result.mainWindowSize, 560, 280);
    const QSize savedDialogSize = settings_.value(
        QStringLiteral("ui/settingsDialogSize"), result.settingsDialogSize).toSize();
    result.settingsDialogSize = QSize(
        qBound(360, savedDialogSize.width(), 1400),
        qBound(300, savedDialogSize.height(), 1000));
    result.cpuStructureDialogSize = boundedSize(
        QStringLiteral("ui/cpuStructureDialogSize"), result.cpuStructureDialogSize, 620, 420);
    result.processSelectionDialogSize = boundedSize(
        QStringLiteral("ui/processSelectionDialogSize"), result.processSelectionDialogSize, 360, 220);
    return result;
}

void SettingsRepository::saveUiSettings(const UiSettings &settings)
{
    settings_.setValue(QStringLiteral("ui/theme"), themeModeToString(settings.themeMode));
    settings_.setValue(QStringLiteral("ui/language"), languageModeToString(settings.languageMode));
    settings_.setValue(QStringLiteral("ui/cpuColumnWidth"), settings.cpuColumnWidth);
    settings_.setValue(QStringLiteral("ui/enabledColumnWidth"), settings.enabledColumnWidth);
    settings_.setValue(QStringLiteral("ui/executableColumnWidth"), settings.executableColumnWidth);
    settings_.setValue(QStringLiteral("ui/applyOnceColumnWidth"), settings.applyOnceColumnWidth);
    settings_.setValue(QStringLiteral("ui/periodicColumnWidth"), settings.periodicColumnWidth);
    settings_.setValue(QStringLiteral("ui/resetColumnWidth"), settings.resetColumnWidth);
    settings_.setValue(QStringLiteral("ui/orderColumnWidth"), settings.orderColumnWidth);
    settings_.setValue(QStringLiteral("ui/processPriorityColumnWidth"),
                       settings.processPriorityColumnWidth);
    settings_.setValue(QStringLiteral("ui/pidColumnWidth"), settings.pidColumnWidth);
    settings_.setValue(QStringLiteral("ui/processStartColumnWidth"),
                       settings.processStartColumnWidth);
    settings_.setValue(QStringLiteral("ui/processCpuColumnWidth"),
                       settings.processCpuColumnWidth);
    settings_.setValue(QStringLiteral("ui/processMemoryColumnWidth"),
                       settings.processMemoryColumnWidth);
    settings_.setValue(QStringLiteral("ui/showProcessStartColumn"),
                       settings.showProcessStartColumn);
    settings_.setValue(QStringLiteral("ui/showProcessCpuColumn"),
                       settings.showProcessCpuColumn);
    settings_.setValue(QStringLiteral("ui/showProcessMemoryColumn"),
                       settings.showProcessMemoryColumn);
    settings_.setValue(QStringLiteral("ui/showCpuUsageRow"), settings.showCpuUsageRow);
    settings_.setValue(QStringLiteral("ui/tableFontPointSize"), settings.tableFontPointSize);
    settings_.setValue(QStringLiteral("ui/tableRowHeight"), settings.tableRowHeight);
    settings_.setValue(QStringLiteral("ui/cpuUsageRowHeight"), settings.cpuUsageRowHeight);
    settings_.setValue(QStringLiteral("ui/cpuUsageBarWidth"), settings.cpuUsageBarWidth);
    settings_.setValue(QStringLiteral("ui/cpuUsageBarColor"), settings.cpuUsageBarColor);
    settings_.setValue(QStringLiteral("ui/cpuUsageBarTransparencyPercent"),
                       settings.cpuUsageBarTransparencyPercent);
    settings_.setValue(QStringLiteral("ui/separatorLineThickness"),
                       settings.separatorLineThickness);
    settings_.setValue(QStringLiteral("ui/separatorLineTransparencyPercent"),
                       settings.separatorLineTransparencyPercent);
    settings_.setValue(QStringLiteral("ui/performanceUpdateIntervalSeconds"),
                       settings.performanceUpdateIntervalSeconds);
    settings_.setValue(QStringLiteral("ui/periodicApplyIntervalSeconds"),
                       settings.periodicApplyIntervalSeconds);
    settings_.setValue(QStringLiteral("ui/mainWindowSize"), settings.mainWindowSize);
    settings_.setValue(QStringLiteral("ui/settingsDialogSize"), settings.settingsDialogSize);
    settings_.setValue(QStringLiteral("ui/cpuStructureDialogSize"), settings.cpuStructureDialogSize);
    settings_.setValue(QStringLiteral("ui/processSelectionDialogSize"), settings.processSelectionDialogSize);
    settings_.sync();
}

QVector<CpuStructure> SettingsRepository::loadCpuStructures() const
{
    const QJsonDocument document = documentFromSetting(settings_.value(QStringLiteral("cpuStructures")));
    if (!document.isArray()) {
        return {};
    }

    QVector<CpuStructure> structures;
    for (const QJsonValue &value : document.array()) {
        if (value.isObject()) {
            structures.append(CpuStructure::fromJson(value.toObject()));
        }
    }
    return structures;
}

QString SettingsRepository::loadActiveCpuStructureId() const
{
    return settings_.value(QStringLiteral("activeCpuStructureId")).toString();
}

void SettingsRepository::saveCpuStructures(const QVector<CpuStructure> &structures,
                                            const QString &activeStructureId)
{
    QJsonArray serialized;
    for (const CpuStructure &structure : structures) {
        serialized.append(structure.toJson());
    }
    settings_.setValue(QStringLiteral("cpuStructures"), compactJson(serialized));
    settings_.setValue(QStringLiteral("activeCpuStructureId"), activeStructureId);
    settings_.sync();
}

QVector<ProcessPreset> SettingsRepository::loadProcessPresets() const
{
    const QJsonDocument document = documentFromSetting(settings_.value(QStringLiteral("processPresets")));
    if (!document.isArray()) {
        return {};
    }

    QVector<ProcessPreset> presets;
    for (const QJsonValue &value : document.array()) {
        if (value.isObject()) {
            presets.append(ProcessPreset::fromJson(value.toObject()));
        }
    }
    return presets;
}

void SettingsRepository::saveProcessPresets(const QVector<ProcessPreset> &presets)
{
    QJsonArray serialized;
    for (const ProcessPreset &preset : presets) {
        serialized.append(preset.toJson());
    }
    settings_.setValue(QStringLiteral("processPresets"), compactJson(serialized));
    settings_.sync();
}

QVector<ProcessOrderAssignment> SettingsRepository::loadProcessOrderAssignments() const
{
    const QJsonDocument document = documentFromSetting(
        settings_.value(QStringLiteral("processOrderAssignments")));
    if (!document.isArray()) {
        return {};
    }

    QVector<ProcessOrderAssignment> assignments;
    for (const QJsonValue &value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const ProcessOrderAssignment assignment = ProcessOrderAssignment::fromJson(
            value.toObject());
        if (!assignment.executableName.trimmed().isEmpty() && assignment.processId != 0) {
            assignments.append(assignment);
        }
    }
    return assignments;
}

void SettingsRepository::saveProcessOrderAssignments(
    const QVector<ProcessOrderAssignment> &assignments)
{
    QJsonArray serialized;
    for (const ProcessOrderAssignment &assignment : assignments) {
        serialized.append(assignment.toJson());
    }
    settings_.setValue(QStringLiteral("processOrderAssignments"), compactJson(serialized));
    settings_.sync();
}

QVector<ProcessPresetOwnership> SettingsRepository::loadProcessPresetOwnerships() const
{
    const QJsonDocument document = documentFromSetting(
        settings_.value(QStringLiteral("processPresetOwnerships")));
    if (!document.isArray()) {
        return {};
    }

    QVector<ProcessPresetOwnership> ownerships;
    for (const QJsonValue &value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const ProcessPresetOwnership ownership = ProcessPresetOwnership::fromJson(
            value.toObject());
        if (!ownership.executableName.trimmed().isEmpty()
            && ownership.processId != 0 && !ownership.presetId.isEmpty()) {
            ownerships.append(ownership);
        }
    }
    return ownerships;
}

void SettingsRepository::saveProcessPresetOwnerships(
    const QVector<ProcessPresetOwnership> &ownerships)
{
    QJsonArray serialized;
    for (const ProcessPresetOwnership &ownership : ownerships) {
        serialized.append(ownership.toJson());
    }
    settings_.setValue(QStringLiteral("processPresetOwnerships"), compactJson(serialized));
    settings_.sync();
}

} // namespace ProcessPresets
