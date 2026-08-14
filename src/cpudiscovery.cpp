#include "cpudiscovery.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QSettings>
#include <QSysInfo>
#include <QThread>

#include <algorithm>
#include <climits>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace ProcessPresets {
namespace {

#ifdef Q_OS_WIN
struct CoreInfo {
    int processorGroup = 0;
    QVector<int> logicalProcessors;
    BYTE efficiencyClass = 0;
    QString label;
};

struct L3Domain {
    int processorGroup = 0;
    KAFFINITY mask = 0;
};

BYTE coreEfficiencyClass(const PROCESSOR_RELATIONSHIP &relationship)
{
    // MinGW 13's winnt.h still calls this byte Reserved[0], although Windows
    // 10+ exposes it as EfficiencyClass immediately after Flags.
    const auto *bytes = reinterpret_cast<const BYTE *>(&relationship);
    return bytes[sizeof(BYTE)];
}

QByteArray logicalProcessorInformation(LOGICAL_PROCESSOR_RELATIONSHIP relationship)
{
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(relationship, nullptr, &bytes);
    if (bytes == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return {};
    }

    QByteArray buffer(static_cast<qsizetype>(bytes), Qt::Uninitialized);
    if (!GetLogicalProcessorInformationEx(
            relationship,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
            &bytes)) {
        return {};
    }
    buffer.resize(static_cast<qsizetype>(bytes));
    return buffer;
}

template <typename Callback>
void forEachTopologyEntry(const QByteArray &buffer, Callback callback)
{
    BYTE *current = reinterpret_cast<BYTE *>(const_cast<char *>(buffer.constData()));
    BYTE *const end = current + buffer.size();
    while (current < end) {
        auto *entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(current);
        if (entry->Size == 0 || current + entry->Size > end) {
            return;
        }
        callback(*entry);
        current += entry->Size;
    }
}

QVector<CoreInfo> discoverCores()
{
    QVector<CoreInfo> cores;
    const QByteArray buffer = logicalProcessorInformation(RelationProcessorCore);
    forEachTopologyEntry(buffer, [&cores](const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX &entry) {
        if (entry.Relationship != RelationProcessorCore) {
            return;
        }
        const PROCESSOR_RELATIONSHIP &relationship = entry.Processor;
        for (WORD maskIndex = 0; maskIndex < relationship.GroupCount; ++maskIndex) {
            const GROUP_AFFINITY &groupMask = relationship.GroupMask[maskIndex];
            CoreInfo core;
            core.processorGroup = static_cast<int>(groupMask.Group);
            core.efficiencyClass = coreEfficiencyClass(relationship);
            for (int bit = 0; bit < static_cast<int>(sizeof(KAFFINITY) * CHAR_BIT); ++bit) {
                if ((groupMask.Mask & (static_cast<KAFFINITY>(1) << bit)) != 0) {
                    core.logicalProcessors.append(bit);
                }
            }
            if (!core.logicalProcessors.isEmpty()) {
                cores.append(core);
            }
        }
    });

    std::sort(cores.begin(), cores.end(), [](const CoreInfo &left, const CoreInfo &right) {
        if (left.processorGroup != right.processorGroup) {
            return left.processorGroup < right.processorGroup;
        }
        return left.logicalProcessors.first() < right.logicalProcessors.first();
    });
    return cores;
}

QVector<L3Domain> discoverL3Domains()
{
    QVector<L3Domain> domains;
    const QByteArray buffer = logicalProcessorInformation(RelationCache);
    forEachTopologyEntry(buffer, [&domains](const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX &entry) {
        if (entry.Relationship != RelationCache || entry.Cache.Level != 3 || entry.Cache.GroupMask.Mask == 0) {
            return;
        }
        domains.append({static_cast<int>(entry.Cache.GroupMask.Group), entry.Cache.GroupMask.Mask});
    });

    std::sort(domains.begin(), domains.end(), [](const L3Domain &left, const L3Domain &right) {
        if (left.processorGroup != right.processorGroup) {
            return left.processorGroup < right.processorGroup;
        }
        return left.mask < right.mask;
    });
    return domains;
}

bool domainContainsCore(const L3Domain &domain, const CoreInfo &core)
{
    if (domain.processorGroup != core.processorGroup) {
        return false;
    }
    for (const int logicalProcessor : core.logicalProcessors) {
        if ((domain.mask & (static_cast<KAFFINITY>(1) << logicalProcessor)) == 0) {
            return false;
        }
    }
    return true;
}

CpuNode leafFor(const CoreInfo &core, int logicalProcessor)
{
    CpuNode leaf;
    leaf.label = core.label;
    leaf.processorGroup = core.processorGroup;
    leaf.logicalProcessor = logicalProcessor;
    return leaf;
}

CpuNode nodeForCore(const CoreInfo &core, bool showThreads)
{
    if (!showThreads) {
        return leafFor(core, core.logicalProcessors.first());
    }

    CpuNode node;
    node.label = core.label;
    for (int threadIndex = 0; threadIndex < core.logicalProcessors.size(); ++threadIndex) {
        CpuNode thread;
        thread.label = QStringLiteral("T%1").arg(threadIndex + 1);
        thread.processorGroup = core.processorGroup;
        thread.logicalProcessor = core.logicalProcessors.at(threadIndex);
        node.children.append(thread);
    }
    return node;
}

void assignCoreLabels(QVector<CoreInfo> *cores)
{
    QMap<int, QSet<int>> efficiencyClassesByGroup;
    for (const CoreInfo &core : *cores) {
        efficiencyClassesByGroup[core.processorGroup].insert(core.efficiencyClass);
    }

    QMap<int, int> highestPerformanceClassByGroup;
    for (auto group = efficiencyClassesByGroup.cbegin(); group != efficiencyClassesByGroup.cend(); ++group) {
        const QSet<int> &classes = group.value();
        highestPerformanceClassByGroup.insert(
            group.key(), *std::max_element(classes.cbegin(), classes.cend()));
    }

    QMap<int, int> performanceCoreCounts;
    QMap<int, int> efficiencyCoreCounts;
    for (CoreInfo &core : *cores) {
        const bool heterogeneous = efficiencyClassesByGroup.value(core.processorGroup).size() > 1;
        const bool performanceCore = !heterogeneous
            || core.efficiencyClass == highestPerformanceClassByGroup.value(core.processorGroup);
        if (performanceCore) {
            core.label = QStringLiteral("P%1").arg(performanceCoreCounts[core.processorGroup] + 1);
            performanceCoreCounts[core.processorGroup] += 1;
        } else {
            core.label = QStringLiteral("E%1").arg(efficiencyCoreCounts[core.processorGroup] + 1);
            efficiencyCoreCounts[core.processorGroup] += 1;
        }
    }
}
#endif

} // namespace

QString currentCpuFingerprint()
{
    QStringList components;
    components.append(QSysInfo::currentCpuArchitecture());
#ifdef Q_OS_WIN
    QSettings cpuKey(
        QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
        QSettings::NativeFormat);
    components.append(cpuKey.value(QStringLiteral("ProcessorNameString")).toString());
    components.append(cpuKey.value(QStringLiteral("Identifier")).toString());
    components.append(cpuKey.value(QStringLiteral("VendorIdentifier")).toString());
#endif
    const QMap<int, int> processorCounts = activeProcessorCounts();
    for (auto iterator = processorCounts.cbegin(); iterator != processorCounts.cend(); ++iterator) {
        components.append(QStringLiteral("%1:%2").arg(iterator.key()).arg(iterator.value()));
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(components.join(QLatin1Char('|')).toUtf8(),
                                 QCryptographicHash::Sha256).toHex());
}

QMap<int, int> activeProcessorCounts()
{
    QMap<int, int> result;
#ifdef Q_OS_WIN
    const WORD groupCount = GetActiveProcessorGroupCount();
    for (WORD group = 0; group < groupCount; ++group) {
        result.insert(group, static_cast<int>(GetActiveProcessorCount(group)));
    }
#else
    result.insert(0, qMax(1, QThread::idealThreadCount()));
#endif
    return result;
}

CpuStructure createDetectedCpuStructure()
{
    CpuStructure structure;
    structure.id = makeId();
    structure.name = QStringLiteral("現在のPC（自動検出）");
    structure.isAutoDetected = true;
    structure.autoDetectedCpuFingerprint = currentCpuFingerprint();

#ifdef Q_OS_WIN
    QVector<CoreInfo> cores = discoverCores();
    if (!cores.isEmpty()) {
        assignCoreLabels(&cores);
        const QVector<L3Domain> l3Domains = discoverL3Domains();
        QMap<int, QVector<int>> coreIndexesByGroup;
        for (int index = 0; index < cores.size(); ++index) {
            coreIndexesByGroup[cores.at(index).processorGroup].append(index);
        }

        const bool hasMultipleProcessorGroups = coreIndexesByGroup.size() > 1;
        for (auto group = coreIndexesByGroup.cbegin(); group != coreIndexesByGroup.cend(); ++group) {
            QVector<int> domainIndexes;
            for (int domainIndex = 0; domainIndex < l3Domains.size(); ++domainIndex) {
                if (l3Domains.at(domainIndex).processorGroup == group.key()) {
                    domainIndexes.append(domainIndex);
                }
            }

            QMap<int, QVector<int>> coresByDomain;
            bool hasCcxStructure = domainIndexes.size() > 1;
            if (hasCcxStructure) {
                for (const int coreIndex : group.value()) {
                    int matchedDomain = -1;
                    for (const int domainIndex : domainIndexes) {
                        if (domainContainsCore(l3Domains.at(domainIndex), cores.at(coreIndex))) {
                            matchedDomain = domainIndex;
                            break;
                        }
                    }
                    if (matchedDomain < 0) {
                        hasCcxStructure = false;
                        break;
                    }
                    coresByDomain[matchedDomain].append(coreIndex);
                }
            }

            bool hasSmt = false;
            for (const int coreIndex : group.value()) {
                hasSmt = hasSmt || cores.at(coreIndex).logicalProcessors.size() > 1;
            }

            QList<CpuNode> groupContents;
            if (hasCcxStructure) {
                int ccxNumber = 1;
                for (const int domainIndex : domainIndexes) {
                    const QVector<int> ccxCores = coresByDomain.value(domainIndex);
                    if (ccxCores.isEmpty()) {
                        continue;
                    }
                    CpuNode ccx;
                    ccx.label = QStringLiteral("CCX%1").arg(ccxNumber++);
                    for (const int coreIndex : ccxCores) {
                        ccx.children.append(nodeForCore(cores.at(coreIndex), hasSmt));
                    }
                    groupContents.append(ccx);
                }
            } else {
                for (const int coreIndex : group.value()) {
                    groupContents.append(nodeForCore(cores.at(coreIndex), hasSmt));
                }
            }

            if (hasMultipleProcessorGroups) {
                CpuNode processorGroup;
                processorGroup.label = QStringLiteral("Processor Group %1").arg(group.key());
                processorGroup.children = groupContents;
                structure.roots.append(processorGroup);
            } else {
                structure.roots.append(groupContents);
            }
        }
    }
#endif

    if (!structure.roots.isEmpty()) {
        return structure;
    }

    // Fallback for non-Windows builds or an unexpected topology query failure.
    const QMap<int, int> groups = activeProcessorCounts();
    const bool hasMultipleProcessorGroups = groups.size() > 1;
    for (auto group = groups.cbegin(); group != groups.cend(); ++group) {
        QList<CpuNode> groupContents;
        for (int threadNumber = 0; threadNumber < group.value(); ++threadNumber) {
            CpuNode core;
            core.label = QStringLiteral("P%1").arg(threadNumber + 1);
            core.processorGroup = group.key();
            core.logicalProcessor = threadNumber;
            groupContents.append(core);
        }
        if (hasMultipleProcessorGroups) {
            CpuNode processorGroup;
            processorGroup.label = QStringLiteral("Processor Group %1").arg(group.key());
            processorGroup.children = groupContents;
            structure.roots.append(processorGroup);
        } else {
            structure.roots.append(groupContents);
        }
    }
    return structure;
}

} // namespace ProcessPresets
