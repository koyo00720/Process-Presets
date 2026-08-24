#pragma once

#include "models.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace ProcessPresets {

struct ProcessInfo {
    quint32 processId = 0;
    QString executableName;
};

struct AffinityApplyResult {
    bool success = false;
    QString message;
};

struct ProcessPerformanceSnapshot {
    bool timingAvailable = false;
    bool memoryAvailable = false;
    QDateTime startTime;
    quint64 processorTime100Nanoseconds = 0;
    quint64 workingSetBytes = 0;
};

struct ProcessorPerformanceSnapshot {
    int processorGroup = 0;
    int logicalProcessor = 0;
    quint64 idleTime100Nanoseconds = 0;
    quint64 totalTime100Nanoseconds = 0;
};

QVector<ProcessInfo> findProcessesByExecutable(const QString &executableName);
ProcessPerformanceSnapshot queryProcessPerformance(quint32 processId);
QVector<ProcessorPerformanceSnapshot> queryProcessorPerformance();
int activeLogicalProcessorCount();
AffinityApplyResult applyAffinity(quint32 processId, const QVector<CpuLeaf> &selectedLeaves);
AffinityApplyResult applyProcessPriority(quint32 processId, ProcessPriorityClass priority);
AffinityApplyResult resetAffinity(quint32 processId);

} // namespace ProcessPresets
