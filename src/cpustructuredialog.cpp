#include "cpustructuredialog.h"

#include "cpudiscovery.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ProcessPresets {
namespace {

std::optional<int> optionalInteger(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const int result = text.trimmed().toInt(&ok);
    return ok ? std::optional<int>(result) : std::nullopt;
}

QTreeWidgetItem *newEditableItem()
{
    auto *item = new QTreeWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
                   | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    return item;
}

} // namespace

CpuStructureDialog::CpuStructureDialog(QVector<CpuStructure> structures,
                                       QString activeStructureId,
                                       QSize initialSize,
                                       QWidget *parent)
    : QDialog(parent)
    , structures_(std::move(structures))
    , activeStructureId_(std::move(activeStructureId))
    , availableThreadsByGroup_(activeProcessorCounts())
{
    setWindowTitle(QStringLiteral("CPU構造の設定"));
    setModal(true);
    setMinimumSize(620, 420);
    resize(initialSize);

    auto *layout = new QVBoxLayout(this);
    auto *profileRow = new QHBoxLayout;
    profileRow->addWidget(new QLabel(QStringLiteral("CPU構造プリセット:"), this));
    profileCombo_ = new QComboBox(this);
    profileRow->addWidget(profileCombo_, 1);
    auto *newProfileButton = new QPushButton(QStringLiteral("新規..."), this);
    auto *renameProfileButton = new QPushButton(QStringLiteral("名前を変更..."), this);
    auto *deleteProfileButton = new QPushButton(QStringLiteral("削除"), this);
    profileRow->addWidget(newProfileButton);
    profileRow->addWidget(renameProfileButton);
    profileRow->addWidget(deleteProfileButton);
    layout->addLayout(profileRow);

    auto *help = new QLabel(
        QStringLiteral("任意の段数でグループを作れます。子を持つ行は見出しグループ、"
                       "子のない行はCPUスレッドです。CPUスレッドには Processor Group と "
                       "Thread No.（どちらも0始まり）を入力してください。行はドラッグ＆ドロップで並べ替え・階層変更できます。"),
        this);
    help->setWordWrap(true);
    layout->addWidget(help);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({QStringLiteral("表示ラベル"),
                            QStringLiteral("Processor Group"),
                            QStringLiteral("Thread No.")});
    tree_->setDragDropMode(QAbstractItemView::InternalMove);
    tree_->setDefaultDropAction(Qt::MoveAction);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(tree_, 1);

    auto *treeButtons = new QHBoxLayout;
    auto *addGroupButton = new QPushButton(QStringLiteral("グループを追加"), this);
    auto *addThreadButton = new QPushButton(QStringLiteral("CPUスレッドを追加"), this);
    auto *removeButton = new QPushButton(QStringLiteral("選択行を削除"), this);
    treeButtons->addWidget(addGroupButton);
    treeButtons->addWidget(addThreadButton);
    treeButtons->addWidget(removeButton);
    treeButtons->addStretch();
    layout->addLayout(treeButtons);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(profileCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (loadingTree_) {
                    return;
                }
                saveCurrentTree();
                activeStructureId_ = profileCombo_->currentData().toString();
                loadCurrentTree();
            });
    connect(newProfileButton, &QPushButton::clicked, this, [this] { createProfile(); });
    connect(renameProfileButton, &QPushButton::clicked, this, [this] { renameProfile(); });
    connect(deleteProfileButton, &QPushButton::clicked, this, [this] { deleteProfile(); });
    connect(addGroupButton, &QPushButton::clicked, this, [this] { addGroupNode(); });
    connect(addThreadButton, &QPushButton::clicked, this, [this] { addThreadNode(); });
    connect(removeButton, &QPushButton::clicked, this, [this] { removeSelectedNode(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildProfileCombo();
    loadCurrentTree();
}

QSize CpuStructureDialog::dialogSize() const
{
    return size();
}

QVector<CpuStructure> CpuStructureDialog::structures() const
{
    return structures_;
}

QString CpuStructureDialog::activeStructureId() const
{
    return activeStructureId_;
}

int CpuStructureDialog::currentStructureIndex() const
{
    for (int index = 0; index < structures_.size(); ++index) {
        if (structures_.at(index).id == activeStructureId_) {
            return index;
        }
    }
    return -1;
}

void CpuStructureDialog::rebuildProfileCombo()
{
    QSignalBlocker blocker(profileCombo_);
    profileCombo_->clear();
    for (const CpuStructure &structure : structures_) {
        profileCombo_->addItem(structure.name, structure.id);
    }

    int activeIndex = profileCombo_->findData(activeStructureId_);
    if (activeIndex < 0 && !structures_.isEmpty()) {
        activeIndex = 0;
        activeStructureId_ = structures_.at(0).id;
    }
    profileCombo_->setCurrentIndex(activeIndex);
}

void CpuStructureDialog::loadCurrentTree()
{
    const int index = currentStructureIndex();
    if (index < 0) {
        return;
    }

    loadingTree_ = true;
    tree_->clear();
    for (const CpuNode &node : structures_.at(index).roots) {
        appendNodeItem(nullptr, node);
    }
    tree_->expandAll();
    if (tree_->topLevelItemCount() > 0) {
        tree_->setCurrentItem(tree_->topLevelItem(0));
    }
    loadingTree_ = false;
}

void CpuStructureDialog::saveCurrentTree()
{
    if (loadingTree_) {
        return;
    }
    const int index = currentStructureIndex();
    if (index >= 0) {
        structures_[index].roots = treeToNodes();
    }
}

void CpuStructureDialog::appendNodeItem(QTreeWidgetItem *parent, const CpuNode &node)
{
    QTreeWidgetItem *item = newEditableItem();
    item->setText(0, node.label);
    if (node.processorGroup) {
        item->setText(1, QString::number(*node.processorGroup));
    }
    if (node.logicalProcessor) {
        item->setText(2, QString::number(*node.logicalProcessor));
    }

    if (parent != nullptr) {
        parent->addChild(item);
    } else {
        tree_->addTopLevelItem(item);
    }
    for (const CpuNode &child : node.children) {
        appendNodeItem(item, child);
    }
}

CpuNode CpuStructureDialog::nodeFromItem(const QTreeWidgetItem *item) const
{
    CpuNode node;
    node.label = item->text(0).trimmed();
    node.processorGroup = optionalInteger(item->text(1));
    node.logicalProcessor = optionalInteger(item->text(2));
    for (int index = 0; index < item->childCount(); ++index) {
        node.children.append(nodeFromItem(item->child(index)));
    }
    return node;
}

QList<CpuNode> CpuStructureDialog::treeToNodes() const
{
    QList<CpuNode> roots;
    for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
        roots.append(nodeFromItem(tree_->topLevelItem(index)));
    }
    return roots;
}

bool CpuStructureDialog::itemRepresentsGroup(const QTreeWidgetItem *item) const
{
    return item != nullptr
        && (item->childCount() > 0
            || (item->text(1).trimmed().isEmpty() && item->text(2).trimmed().isEmpty()));
}

std::pair<int, int> CpuStructureDialog::firstUnusedThread() const
{
    QSet<QString> used;
    for (const CpuLeaf &leaf : collectCpuLeaves(treeToNodes())) {
        used.insert(cpuKey(leaf.processorGroup, leaf.logicalProcessor));
    }
    for (auto group = availableThreadsByGroup_.cbegin(); group != availableThreadsByGroup_.cend(); ++group) {
        for (int thread = 0; thread < group.value(); ++thread) {
            if (!used.contains(cpuKey(group.key(), thread))) {
                return {group.key(), thread};
            }
        }
    }
    return {0, 0};
}

void CpuStructureDialog::addGroupNode()
{
    QTreeWidgetItem *parent = tree_->currentItem();
    if (!itemRepresentsGroup(parent)) {
        parent = nullptr;
    }

    QTreeWidgetItem *item = newEditableItem();
    item->setText(0, QStringLiteral("新しいグループ"));
    if (parent != nullptr) {
        parent->addChild(item);
        parent->setExpanded(true);
    } else {
        tree_->addTopLevelItem(item);
    }
    tree_->setCurrentItem(item);
    tree_->editItem(item, 0);
}

void CpuStructureDialog::addThreadNode()
{
    QTreeWidgetItem *parent = tree_->currentItem();
    if (!itemRepresentsGroup(parent)) {
        parent = nullptr;
    }
    const auto [group, thread] = firstUnusedThread();

    QTreeWidgetItem *item = newEditableItem();
    item->setText(0, QStringLiteral("T%1").arg(thread));
    item->setText(1, QString::number(group));
    item->setText(2, QString::number(thread));
    if (parent != nullptr) {
        parent->addChild(item);
        parent->setExpanded(true);
    } else {
        tree_->addTopLevelItem(item);
    }
    tree_->setCurrentItem(item);
    tree_->editItem(item, 0);
}

void CpuStructureDialog::removeSelectedNode()
{
    const QList<QTreeWidgetItem *> selectedItems = tree_->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QSet<QTreeWidgetItem *> selectedSet(selectedItems.cbegin(), selectedItems.cend());
    QList<QTreeWidgetItem *> rootsToDelete;
    for (QTreeWidgetItem *item : selectedItems) {
        bool ancestorIsSelected = false;
        for (QTreeWidgetItem *ancestor = item->parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
            if (selectedSet.contains(ancestor)) {
                ancestorIsSelected = true;
                break;
            }
        }
        if (!ancestorIsSelected) {
            rootsToDelete.append(item);
        }
    }
    for (QTreeWidgetItem *item : rootsToDelete) {
        delete item;
    }
}

bool CpuStructureDialog::nameIsAvailable(const QString &name, int exceptIndex) const
{
    for (int index = 0; index < structures_.size(); ++index) {
        if (index != exceptIndex
            && structures_.at(index).name.compare(name, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

void CpuStructureDialog::createProfile()
{
    saveCurrentTree();
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("CPU構造プリセットを作成"),
        QStringLiteral("プリセット名:"),
        QLineEdit::Normal,
        QStringLiteral("新しいCPU構造"),
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }
    if (!nameIsAvailable(name)) {
        QMessageBox::warning(this, QStringLiteral("CPU構造の設定"),
                             QStringLiteral("同じ名前のCPU構造プリセットが既にあります。"));
        return;
    }

    CpuStructure structure;
    const int currentIndex = currentStructureIndex();
    if (currentIndex >= 0) {
        structure = structures_.at(currentIndex);
    }
    structure.id = makeId();
    structure.name = name;
    structure.isAutoDetected = false;
    structure.autoDetectedCpuFingerprint.clear();
    structures_.append(structure);
    activeStructureId_ = structure.id;
    rebuildProfileCombo();
    loadCurrentTree();
}

void CpuStructureDialog::renameProfile()
{
    saveCurrentTree();
    const int index = currentStructureIndex();
    if (index < 0) {
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("CPU構造プリセット名を変更"),
        QStringLiteral("プリセット名:"),
        QLineEdit::Normal,
        structures_.at(index).name,
        &accepted).trimmed();
    if (!accepted || name.isEmpty() || name == structures_.at(index).name) {
        return;
    }
    if (!nameIsAvailable(name, index)) {
        QMessageBox::warning(this, QStringLiteral("CPU構造の設定"),
                             QStringLiteral("同じ名前のCPU構造プリセットが既にあります。"));
        return;
    }
    structures_[index].name = name;
    structures_[index].isAutoDetected = false;
    structures_[index].autoDetectedCpuFingerprint.clear();
    rebuildProfileCombo();
}

void CpuStructureDialog::deleteProfile()
{
    saveCurrentTree();
    const int index = currentStructureIndex();
    if (index < 0) {
        return;
    }
    if (structures_.size() <= 1) {
        QMessageBox::information(this, QStringLiteral("CPU構造の設定"),
                                 QStringLiteral("CPU構造プリセットは少なくとも1つ必要です。"));
        return;
    }
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("CPU構造プリセットを削除"),
        QStringLiteral("「%1」を削除しますか？").arg(structures_.at(index).name));
    if (answer != QMessageBox::Yes) {
        return;
    }
    structures_.removeAt(index);
    activeStructureId_ = structures_.at(qMin(index, structures_.size() - 1)).id;
    rebuildProfileCombo();
    loadCurrentTree();
}

void CpuStructureDialog::accept()
{
    saveCurrentTree();
    for (const CpuStructure &structure : structures_) {
        const QString error = validateCpuStructure(structure, availableThreadsByGroup_);
        if (!error.isEmpty()) {
            QMessageBox::warning(this,
                                 QStringLiteral("CPU構造の設定"),
                                 QStringLiteral("「%1」: %2").arg(structure.name, error));
            return;
        }
    }
    QDialog::accept();
}

} // namespace ProcessPresets
