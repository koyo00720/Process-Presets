#include "settingsrepository.h"

#include <QCoreApplication>
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
    result.applyColumnWidth = qBound(
        1, settings_.value(QStringLiteral("ui/applyColumnWidth"), result.applyColumnWidth).toInt(), 600);
    result.executableColumnWidth = qBound(
        1,
        settings_.value(QStringLiteral("ui/executableColumnWidth"), result.executableColumnWidth).toInt(),
        600);
    result.tableFontPointSize = qBound(
        6, settings_.value(QStringLiteral("ui/tableFontPointSize"), result.tableFontPointSize).toInt(), 48);
    const auto boundedSize = [this](const QString &key, const QSize &fallback,
                                    int minimumWidth, int minimumHeight) {
        const QSize savedSize = settings_.value(key, fallback).toSize();
        return QSize(qBound(minimumWidth, savedSize.width(), 2400),
                     qBound(minimumHeight, savedSize.height(), 1600));
    };
    result.mainWindowSize = boundedSize(
        QStringLiteral("ui/mainWindowSize"), result.mainWindowSize, 760, 360);
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
    settings_.setValue(QStringLiteral("ui/applyColumnWidth"), settings.applyColumnWidth);
    settings_.setValue(QStringLiteral("ui/executableColumnWidth"), settings.executableColumnWidth);
    settings_.setValue(QStringLiteral("ui/tableFontPointSize"), settings.tableFontPointSize);
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

} // namespace ProcessPresets
