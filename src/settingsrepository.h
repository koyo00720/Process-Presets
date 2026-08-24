#pragma once

#include "appsettings.h"
#include "models.h"

#include <QSettings>

namespace ProcessPresets {

class SettingsRepository final {
public:
    SettingsRepository();

    QString configFilePath() const;

    UiSettings loadUiSettings() const;
    void saveUiSettings(const UiSettings &settings);

    QVector<CpuStructure> loadCpuStructures() const;
    QString loadActiveCpuStructureId() const;
    void saveCpuStructures(const QVector<CpuStructure> &structures,
                           const QString &activeStructureId);

    QVector<ProcessPreset> loadProcessPresets() const;
    void saveProcessPresets(const QVector<ProcessPreset> &presets);

    QVector<ProcessOrderAssignment> loadProcessOrderAssignments() const;
    void saveProcessOrderAssignments(const QVector<ProcessOrderAssignment> &assignments);
    QVector<ProcessPresetOwnership> loadProcessPresetOwnerships() const;
    void saveProcessPresetOwnerships(const QVector<ProcessPresetOwnership> &ownerships);

private:
    QString filePath_;
    QSettings settings_;
};

} // namespace ProcessPresets
