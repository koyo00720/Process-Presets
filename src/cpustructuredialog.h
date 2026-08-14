#pragma once

#include "models.h"

#include <QDialog>
#include <QMap>

class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;

namespace ProcessPresets {

class CpuStructureDialog final : public QDialog {
public:
    CpuStructureDialog(QVector<CpuStructure> structures,
                       QString activeStructureId,
                       QSize initialSize,
                       QWidget *parent = nullptr);

    QVector<CpuStructure> structures() const;
    QString activeStructureId() const;
    QSize dialogSize() const;

protected:
    void accept() override;

private:
    int currentStructureIndex() const;
    void rebuildProfileCombo();
    void loadCurrentTree();
    void saveCurrentTree();
    void appendNodeItem(QTreeWidgetItem *parent, const CpuNode &node);
    CpuNode nodeFromItem(const QTreeWidgetItem *item) const;
    QList<CpuNode> treeToNodes() const;
    bool itemRepresentsGroup(const QTreeWidgetItem *item) const;
    std::pair<int, int> firstUnusedThread() const;
    void addGroupNode();
    void addThreadNode();
    void removeSelectedNode();
    void createProfile();
    void renameProfile();
    void deleteProfile();
    bool nameIsAvailable(const QString &name, int exceptIndex = -1) const;

    QVector<CpuStructure> structures_;
    QString activeStructureId_;
    QMap<int, int> availableThreadsByGroup_;
    QComboBox *profileCombo_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    bool loadingTree_ = false;
};

} // namespace ProcessPresets
