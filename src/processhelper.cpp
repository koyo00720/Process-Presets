#include "processhelper.h"

#include <QFileInfo>
#include <QHash>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <psapi.h>
#  include <tlhelp32.h>
#endif

namespace ProcessPresets {
namespace {

#ifdef Q_OS_WIN
quint64 fileTimeValue(const FILETIME &fileTime)
{
    ULARGE_INTEGER value {};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    return value.QuadPart;
}

QDateTime dateTimeFromFileTime(const FILETIME &fileTime)
{
    constexpr quint64 windowsToUnixEpoch100Nanoseconds = 116444736000000000ULL;
    const quint64 value = fileTimeValue(fileTime);
    if (value < windowsToUnixEpoch100Nanoseconds) {
        return {};
    }
    const qint64 milliseconds = static_cast<qint64>(
        (value - windowsToUnixEpoch100Nanoseconds) / 10000ULL);
    return QDateTime::fromMSecsSinceEpoch(milliseconds);
}

QString windowsErrorMessage(DWORD error)
{
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    QString message;
    if (length != 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
        LocalFree(buffer);
    }
    if (message.isEmpty()) {
        message = QStringLiteral("Win32 error %1").arg(error);
    }
    return message;
}
#endif

} // namespace

QVector<ProcessInfo> findProcessesByExecutable(const QString &executableName)
{
    QVector<ProcessInfo> result;
#ifdef Q_OS_WIN
    const QString requestedName = QFileInfo(executableName.trimmed()).fileName();
    if (requestedName.isEmpty()) {
        return result;
    }

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    BOOL hasEntry = Process32FirstW(snapshot, &entry);
    while (hasEntry) {
        const QString currentName = QString::fromWCharArray(entry.szExeFile);
        if (currentName.compare(requestedName, Qt::CaseInsensitive) == 0) {
            result.append({static_cast<quint32>(entry.th32ProcessID), currentName});
        }
        hasEntry = Process32NextW(snapshot, &entry);
    }
    CloseHandle(snapshot);
#else
    Q_UNUSED(executableName)
#endif
    return result;
}

ProcessPerformanceSnapshot queryProcessPerformance(quint32 processId)
{
    ProcessPerformanceSnapshot result;
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
        FALSE,
        static_cast<DWORD>(processId));
    if (process == nullptr) {
        process = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(processId));
    }
    if (process == nullptr) {
        return result;
    }

    FILETIME creationTime {};
    FILETIME exitTime {};
    FILETIME kernelTime {};
    FILETIME userTime {};
    if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime)) {
        result.timingAvailable = true;
        result.startTime = dateTimeFromFileTime(creationTime);
        result.processorTime100Nanoseconds = fileTimeValue(kernelTime) + fileTimeValue(userTime);
    }

    PROCESS_MEMORY_COUNTERS_EX memoryCounters {};
    memoryCounters.cb = sizeof(memoryCounters);
    if (GetProcessMemoryInfo(
            process,
            reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memoryCounters),
            sizeof(memoryCounters))) {
        result.memoryAvailable = true;
        result.workingSetBytes = static_cast<quint64>(memoryCounters.WorkingSetSize);
    }
    CloseHandle(process);
#else
    Q_UNUSED(processId)
#endif
    return result;
}

QVector<ProcessorPerformanceSnapshot> queryProcessorPerformance()
{
    QVector<ProcessorPerformanceSnapshot> result;
#ifdef Q_OS_WIN
    struct NativeProcessorPerformanceInformation {
        LARGE_INTEGER idleTime;
        LARGE_INTEGER kernelTime;
        LARGE_INTEGER userTime;
        LARGE_INTEGER dpcTime;
        LARGE_INTEGER interruptTime;
        ULONG interruptCount;
    };

    using NtQuerySystemInformationFunction = LONG (WINAPI *)(ULONG, PVOID, ULONG, PULONG);
    constexpr ULONG systemProcessorPerformanceInformation = 8;
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto querySystemInformation = ntdll == nullptr
        ? nullptr
        : reinterpret_cast<NtQuerySystemInformationFunction>(
              GetProcAddress(ntdll, "NtQuerySystemInformation"));
    const DWORD processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (querySystemInformation == nullptr || processorCount == 0) {
        return result;
    }

    QVector<NativeProcessorPerformanceInformation> nativeSnapshots(
        static_cast<int>(processorCount));
    ULONG returnedBytes = 0;
    const LONG status = querySystemInformation(
        systemProcessorPerformanceInformation,
        nativeSnapshots.data(),
        static_cast<ULONG>(nativeSnapshots.size()
                           * sizeof(NativeProcessorPerformanceInformation)),
        &returnedBytes);
    if (status < 0) {
        return result;
    }

    const int returnedCount = returnedBytes == 0
        ? nativeSnapshots.size()
        : qMin(nativeSnapshots.size(),
               static_cast<int>(returnedBytes
                                / sizeof(NativeProcessorPerformanceInformation)));
    int flatProcessorIndex = 0;
    const WORD groupCount = GetActiveProcessorGroupCount();
    for (WORD group = 0; group < groupCount && flatProcessorIndex < returnedCount; ++group) {
        const DWORD groupProcessorCount = GetActiveProcessorCount(group);
        for (DWORD logicalProcessor = 0;
             logicalProcessor < groupProcessorCount && flatProcessorIndex < returnedCount;
             ++logicalProcessor, ++flatProcessorIndex) {
            const NativeProcessorPerformanceInformation &native =
                nativeSnapshots.at(flatProcessorIndex);
            ProcessorPerformanceSnapshot snapshot;
            snapshot.processorGroup = static_cast<int>(group);
            snapshot.logicalProcessor = static_cast<int>(logicalProcessor);
            snapshot.idleTime100Nanoseconds = static_cast<quint64>(native.idleTime.QuadPart);
            snapshot.totalTime100Nanoseconds = static_cast<quint64>(
                native.kernelTime.QuadPart + native.userTime.QuadPart);
            result.append(snapshot);
        }
    }
#endif
    return result;
}

int activeLogicalProcessorCount()
{
#ifdef Q_OS_WIN
    return qMax(1, static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
#else
    return 1;
#endif
}

AffinityApplyResult applyAffinity(quint32 processId, const QVector<CpuLeaf> &selectedLeaves)
{
#ifndef Q_OS_WIN
    Q_UNUSED(processId)
    Q_UNUSED(selectedLeaves)
    return {false, QStringLiteral("CPUアフィニティの適用は Windows でのみ利用できます。")};
#else
    if (selectedLeaves.isEmpty()) {
        return {false, QStringLiteral("適用するCPUスレッドを少なくとも1つ選択してください。")};
    }

    QHash<int, KAFFINITY> masksByGroup;
    const WORD groupCount = GetActiveProcessorGroupCount();
    for (const CpuLeaf &leaf : selectedLeaves) {
        if (leaf.processorGroup < 0 || leaf.processorGroup >= groupCount) {
            return {false, QStringLiteral("Processor Group %1 はこのPCに存在しません。")
                               .arg(leaf.processorGroup)};
        }
        const int availableThreads = static_cast<int>(
            GetActiveProcessorCount(static_cast<WORD>(leaf.processorGroup)));
        const int bitsInMask = static_cast<int>(sizeof(KAFFINITY) * CHAR_BIT);
        if (leaf.logicalProcessor < 0 || leaf.logicalProcessor >= availableThreads
            || leaf.logicalProcessor >= bitsInMask) {
            return {false,
                    QStringLiteral("Processor Group %1 の Thread No. %2 は範囲外です。")
                        .arg(leaf.processorGroup)
                        .arg(leaf.logicalProcessor)};
        }
        KAFFINITY mask = masksByGroup.value(leaf.processorGroup, 0);
        mask |= static_cast<KAFFINITY>(1) << leaf.logicalProcessor;
        masksByGroup.insert(leaf.processorGroup, mask);
    }

    // SetProcessAffinityMask accepts one processor-group mask.  This covers
    // normal desktop CPUs (one group, up to 64 logical processors) and gives
    // an explicit, actionable error on larger multi-group configurations.
    if (masksByGroup.size() != 1) {
        return {false,
                QStringLiteral("このプリセットは複数の Processor Group をまたいでいます。"
                               "Windows のプロセスアフィニティでは一度に1つのGroupだけを選択してください。")};
    }

    const HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(processId));
    if (process == nullptr) {
        const DWORD error = GetLastError();
        return {false,
                QStringLiteral("PID %1 を開けません: %2")
                    .arg(processId)
                    .arg(windowsErrorMessage(error))};
    }

    const KAFFINITY mask = masksByGroup.cbegin().value();
    const BOOL applied = SetProcessAffinityMask(process, static_cast<DWORD_PTR>(mask));
    const DWORD error = applied ? ERROR_SUCCESS : GetLastError();
    CloseHandle(process);

    if (!applied) {
        return {false,
                QStringLiteral("PID %1 にCPUアフィニティを設定できません: %2")
                    .arg(processId)
                    .arg(windowsErrorMessage(error))};
    }
    return {true, QStringLiteral("PID %1 にCPUアフィニティを適用しました。").arg(processId)};
#endif
}

AffinityApplyResult applyProcessPriority(quint32 processId, ProcessPriorityClass priority)
{
#ifndef Q_OS_WIN
    Q_UNUSED(processId)
    Q_UNUSED(priority)
    return {false, QStringLiteral("プロセス優先度の設定は Windows でのみ利用できます。")};
#else
    DWORD priorityClass = NORMAL_PRIORITY_CLASS;
    switch (priority) {
    case ProcessPriorityClass::Idle:
        priorityClass = IDLE_PRIORITY_CLASS;
        break;
    case ProcessPriorityClass::BelowNormal:
        priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
        break;
    case ProcessPriorityClass::Normal:
        priorityClass = NORMAL_PRIORITY_CLASS;
        break;
    case ProcessPriorityClass::AboveNormal:
        priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
        break;
    case ProcessPriorityClass::High:
        priorityClass = HIGH_PRIORITY_CLASS;
        break;
    case ProcessPriorityClass::Realtime:
        priorityClass = REALTIME_PRIORITY_CLASS;
        break;
    }

    const HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(processId));
    if (process == nullptr) {
        return {false, QStringLiteral("PID %1 を開けません: %2")
                           .arg(processId)
                           .arg(windowsErrorMessage(GetLastError()))};
    }
    const BOOL applied = SetPriorityClass(process, priorityClass);
    const DWORD error = applied ? ERROR_SUCCESS : GetLastError();
    CloseHandle(process);
    if (!applied) {
        return {false, QStringLiteral("PID %1 のプロセス優先度を設定できません: %2")
                           .arg(processId)
                           .arg(windowsErrorMessage(error))};
    }
    return {true, QStringLiteral("PID %1 のプロセス優先度を設定しました。").arg(processId)};
#endif
}

AffinityApplyResult resetAffinity(quint32 processId)
{
#ifndef Q_OS_WIN
    Q_UNUSED(processId)
    return {false, QStringLiteral("CPUアフィニティのリセットは Windows でのみ利用できます。")};
#else
    // Windows has no separate \"default affinity\" API.  Giving the process every
    // active logical processor in its normal processor group is the practical reset.
    const WORD group = 0;
    const int threadCount = static_cast<int>(GetActiveProcessorCount(group));
    const int bitsInMask = static_cast<int>(sizeof(KAFFINITY) * CHAR_BIT);
    if (threadCount <= 0 || threadCount > bitsInMask) {
        return {false, QStringLiteral("このPCのCPU構成ではアフィニティをリセットできません。")};
    }
    const KAFFINITY mask = threadCount == bitsInMask
        ? ~static_cast<KAFFINITY>(0)
        : (static_cast<KAFFINITY>(1) << threadCount) - 1;
    const HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(processId));
    if (process == nullptr) {
        return {false, QStringLiteral("PID %1 を開けません: %2")
                           .arg(processId)
                           .arg(windowsErrorMessage(GetLastError()))};
    }
    const BOOL applied = SetProcessAffinityMask(process, static_cast<DWORD_PTR>(mask));
    const DWORD error = applied ? ERROR_SUCCESS : GetLastError();
    CloseHandle(process);
    if (!applied) {
        return {false, QStringLiteral("PID %1 のCPUアフィニティをリセットできません: %2")
                           .arg(processId)
                           .arg(windowsErrorMessage(error))};
    }
    return {true, QStringLiteral("PID %1 のCPUアフィニティをリセットしました。").arg(processId)};
#endif
}

} // namespace ProcessPresets
