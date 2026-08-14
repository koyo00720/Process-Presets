#pragma once

#include "models.h"

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

QVector<ProcessInfo> findProcessesByExecutable(const QString &executableName);
AffinityApplyResult applyAffinity(quint32 processId, const QVector<CpuLeaf> &selectedLeaves);

} // namespace ProcessPresets

