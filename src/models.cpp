#include "models.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>
#include <functional>

namespace ProcessPresets {
namespace {

std::optional<int> optionalInteger(const QJsonObject &json, const QString &key)
{
    const QJsonValue value = json.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    return value.toInt();
}

QJsonArray nodesToJson(const QList<CpuNode> &nodes)
{
    QJsonArray result;
    for (const CpuNode &node : nodes) {
        result.append(node.toJson());
    }
    return result;
}

QList<CpuNode> nodesFromJson(const QJsonArray &json)
{
    QList<CpuNode> result;
    for (const QJsonValue &value : json) {
        if (value.isObject()) {
            result.append(CpuNode::fromJson(value.toObject()));
        }
    }
    return result;
}

void appendLeaves(const QList<CpuNode> &nodes,
                  QStringList ancestors,
                  QVector<CpuLeaf> *result)
{
    for (const CpuNode &node : nodes) {
        QStringList path = ancestors;
        path.append(node.label);
        if (!node.children.isEmpty()) {
            appendLeaves(node.children, path, result);
            continue;
        }

        // Malformed structures are rejected by validateCpuStructure().  This
        // guard keeps a manually edited .conf file from crashing the UI.
        if (!node.processorGroup || !node.logicalProcessor) {
            continue;
        }

        CpuLeaf leaf;
        leaf.label = node.label;
        leaf.processorGroup = *node.processorGroup;
        leaf.logicalProcessor = *node.logicalProcessor;
        leaf.path = path;
        result->append(leaf);
    }
}

int nodeDepth(const CpuNode &node)
{
    int childDepth = 0;
    for (const CpuNode &child : node.children) {
        childDepth = std::max(childDepth, nodeDepth(child));
    }
    return 1 + childDepth;
}

} // namespace

QString makeId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString cpuKey(int processorGroup, int logicalProcessor)
{
    return QStringLiteral("%1:%2").arg(processorGroup).arg(logicalProcessor);
}

QJsonObject CpuNode::toJson() const
{
    QJsonObject result;
    result.insert(QStringLiteral("label"), label);
    if (processorGroup) {
        result.insert(QStringLiteral("processorGroup"), *processorGroup);
    }
    if (logicalProcessor) {
        result.insert(QStringLiteral("logicalProcessor"), *logicalProcessor);
    }
    result.insert(QStringLiteral("children"), nodesToJson(children));
    return result;
}

CpuNode CpuNode::fromJson(const QJsonObject &json)
{
    CpuNode result;
    result.label = json.value(QStringLiteral("label")).toString();
    result.processorGroup = optionalInteger(json, QStringLiteral("processorGroup"));
    result.logicalProcessor = optionalInteger(json, QStringLiteral("logicalProcessor"));
    result.children = nodesFromJson(json.value(QStringLiteral("children")).toArray());
    return result;
}

QJsonObject CpuStructure::toJson() const
{
    QJsonObject result;
    result.insert(QStringLiteral("id"), id);
    result.insert(QStringLiteral("name"), name);
    result.insert(QStringLiteral("isAutoDetected"), isAutoDetected);
    result.insert(QStringLiteral("autoDetectedCpuFingerprint"), autoDetectedCpuFingerprint);
    result.insert(QStringLiteral("roots"), nodesToJson(roots));
    return result;
}

CpuStructure CpuStructure::fromJson(const QJsonObject &json)
{
    CpuStructure result;
    result.id = json.value(QStringLiteral("id")).toString();
    if (result.id.isEmpty()) {
        result.id = makeId();
    }
    result.name = json.value(QStringLiteral("name")).toString();
    const bool hasAutoDetectedFlag = json.contains(QStringLiteral("isAutoDetected"));
    const bool isAutomaticName = result.name == QStringLiteral("現在のPC（自動検出）");
    result.isAutoDetected = isAutomaticName && (hasAutoDetectedFlag
        ? json.value(QStringLiteral("isAutoDetected")).toBool()
        : true);
    result.autoDetectedCpuFingerprint = json.value(
        QStringLiteral("autoDetectedCpuFingerprint")).toString();
    result.roots = nodesFromJson(json.value(QStringLiteral("roots")).toArray());
    return result;
}

QJsonObject ProcessPreset::toJson() const
{
    QJsonObject result;
    result.insert(QStringLiteral("id"), id);
    result.insert(QStringLiteral("executableName"), executableName);

    QStringList keys = selectedCpuKeys.values();
    std::sort(keys.begin(), keys.end());
    QJsonArray selected;
    for (const QString &key : keys) {
        selected.append(key);
    }
    result.insert(QStringLiteral("selectedCpuKeys"), selected);
    return result;
}

ProcessPreset ProcessPreset::fromJson(const QJsonObject &json)
{
    ProcessPreset result;
    result.id = json.value(QStringLiteral("id")).toString();
    if (result.id.isEmpty()) {
        result.id = makeId();
    }
    result.executableName = json.value(QStringLiteral("executableName")).toString();
    for (const QJsonValue &value : json.value(QStringLiteral("selectedCpuKeys")).toArray()) {
        if (value.isString()) {
            result.selectedCpuKeys.insert(value.toString());
        }
    }
    return result;
}

QVector<CpuLeaf> collectCpuLeaves(const QList<CpuNode> &roots)
{
    QVector<CpuLeaf> result;
    appendLeaves(roots, {}, &result);
    return result;
}

int hierarchyDepth(const QList<CpuNode> &roots)
{
    int result = 1;
    for (const CpuNode &node : roots) {
        result = std::max(result, nodeDepth(node));
    }
    return result;
}

QString validateCpuStructure(const CpuStructure &structure,
                             const QMap<int, int> &availableThreadsByGroup)
{
    if (structure.name.trimmed().isEmpty()) {
        return QStringLiteral("CPU構造プリセット名を入力してください。");
    }
    if (structure.roots.isEmpty()) {
        return QStringLiteral("CPU構造には少なくとも1つのCPUスレッドを設定してください。");
    }

    QSet<QString> assignedProcessors;
    int leafCount = 0;

    std::function<QString(const CpuNode &, const QString &)> validateNode;
    validateNode = [&](const CpuNode &node, const QString &location) -> QString {
        const QString displayName = node.label.trimmed().isEmpty()
            ? location
            : node.label;
        if (node.label.trimmed().isEmpty()) {
            return QStringLiteral("%1 の表示ラベルを入力してください。").arg(location);
        }

        if (!node.children.isEmpty()) {
            if (node.processorGroup || node.logicalProcessor) {
                return QStringLiteral("%1 は子要素を持つため、Processor Group と Thread No. を空欄にしてください。")
                    .arg(displayName);
            }
            for (int index = 0; index < node.children.size(); ++index) {
                const QString error = validateNode(
                    node.children.at(index),
                    QStringLiteral("%1 / %2").arg(displayName).arg(index + 1));
                if (!error.isEmpty()) {
                    return error;
                }
            }
            return {};
        }

        if (!node.processorGroup || !node.logicalProcessor) {
            return QStringLiteral("CPUスレッド %1 には Processor Group と Thread No. の両方を入力してください。")
                .arg(displayName);
        }
        const int group = *node.processorGroup;
        const int thread = *node.logicalProcessor;
        if (group < 0 || thread < 0) {
            return QStringLiteral("CPUスレッド %1 の番号は0以上にしてください。").arg(displayName);
        }
        const auto groupIt = availableThreadsByGroup.constFind(group);
        if (groupIt == availableThreadsByGroup.cend()) {
            return QStringLiteral("CPUスレッド %1 の Processor Group %2 はこのPCに存在しません。")
                .arg(displayName)
                .arg(group);
        }
        if (thread >= groupIt.value()) {
            return QStringLiteral("CPUスレッド %1 の Thread No. %2 は Processor Group %3 の範囲外です（0～%4）。")
                .arg(displayName)
                .arg(thread)
                .arg(group)
                .arg(groupIt.value() - 1);
        }
        const QString key = cpuKey(group, thread);
        if (assignedProcessors.contains(key)) {
            return QStringLiteral("Processor Group %1 / Thread No. %2 が重複しています。")
                .arg(group)
                .arg(thread);
        }
        assignedProcessors.insert(key);
        ++leafCount;
        return {};
    };

    for (int index = 0; index < structure.roots.size(); ++index) {
        const QString error = validateNode(
            structure.roots.at(index), QStringLiteral("最上位ノード %1").arg(index + 1));
        if (!error.isEmpty()) {
            return error;
        }
    }

    if (leafCount == 0) {
        return QStringLiteral("CPU構造には少なくとも1つのCPUスレッドを設定してください。");
    }
    return {};
}

} // namespace ProcessPresets
