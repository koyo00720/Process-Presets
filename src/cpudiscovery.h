#pragma once

#include "models.h"

#include <QMap>

namespace ProcessPresets {

QMap<int, int> activeProcessorCounts();
QString currentCpuFingerprint();
CpuStructure createDetectedCpuStructure();

} // namespace ProcessPresets
