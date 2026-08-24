#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace ProcessPresets {

struct CpuNode {
    QString label;
    std::optional<int> processorGroup;
    std::optional<int> logicalProcessor;
    QList<CpuNode> children;

    QJsonObject toJson() const;
    static CpuNode fromJson(const QJsonObject &json);
};

struct CpuStructure {
    QString id;
    QString name;
    bool isAutoDetected = false;
    QString autoDetectedCpuFingerprint;
    QList<CpuNode> roots;

    QJsonObject toJson() const;
    static CpuStructure fromJson(const QJsonObject &json);
};

enum class ProcessPriorityClass {
    Idle,
    BelowNormal,
    Normal,
    AboveNormal,
    High,
    Realtime,
};

struct ProcessPreset {
    QString id;
    QString executableName;
    QSet<QString> selectedCpuKeys;
    bool enabled = true;
    bool periodicApplyEnabled = false;
    bool applyToAll = false;
    int order = 1;
    ProcessPriorityClass processPriority = ProcessPriorityClass::Normal;
    QString groupId;

    QJsonObject toJson() const;
    static ProcessPreset fromJson(const QJsonObject &json);
};

struct ProcessOrderAssignment {
    QString executableName;
    int order = 1;
    quint32 processId = 0;

    QJsonObject toJson() const;
    static ProcessOrderAssignment fromJson(const QJsonObject &json);
};

struct ProcessPresetOwnership {
    QString executableName;
    quint32 processId = 0;
    QString presetId;

    QJsonObject toJson() const;
    static ProcessPresetOwnership fromJson(const QJsonObject &json);
};

struct CpuLeaf {
    QString label;
    int processorGroup = 0;
    int logicalProcessor = 0;
    QStringList path;
};

QString makeId();
QString cpuKey(int processorGroup, int logicalProcessor);
QString processPriorityClassKey(ProcessPriorityClass priority);
ProcessPriorityClass processPriorityClassFromKey(const QString &key);
QVector<CpuLeaf> collectCpuLeaves(const QList<CpuNode> &roots);
int hierarchyDepth(const QList<CpuNode> &roots);

// Returns an empty string only when the hierarchy can safely be applied on
// this computer.  A leaf is a CPU thread; every non-leaf is a display group.
QString validateCpuStructure(const CpuStructure &structure,
                             const QMap<int, int> &availableThreadsByGroup);

} // namespace ProcessPresets
