#include "mainwindow.h"

#include "cpudiscovery.h"
#include "cpustructuredialog.h"
#include "hierarchicalheaderview.h"
#include "processhelper.h"
#include "processselectiondialog.h"
#include "settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>

#include <algorithm>

namespace ProcessPresets {

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("Process Presets v%1").arg(QCoreApplication::applicationVersion()));
    setMinimumSize(760, 360);

    uiSettings_ = repository_.loadUiSettings();
    resize(uiSettings_.mainWindowSize);
    systemPalette_ = qApp->palette();
    applyUiSettings();
    createMenus();

    presetTable_ = new QTableWidget(this);
    presetTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    presetTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    presetTable_->setEditTriggers(QAbstractItemView::DoubleClicked
                                  | QAbstractItemView::SelectedClicked
                                  | QAbstractItemView::EditKeyPressed);
    presetTable_->setAlternatingRowColors(true);
    presetTable_->setShowGrid(true);
    presetTable_->verticalHeader()->setVisible(false);
    presetTable_->verticalHeader()->setDefaultSectionSize(34);
    presetTable_->setCornerButtonEnabled(false);
    presetTable_->viewport()->installEventFilter(this);

    hierarchyHeader_ = new HierarchicalHeaderView(presetTable_);
    presetTable_->setHorizontalHeader(hierarchyHeader_);
    hierarchyHeader_->setSectionResizeMode(QHeaderView::Fixed);
    hierarchyHeader_->setStretchLastSection(false);
    hierarchyHeader_->setSectionsMovable(false);
    setCentralWidget(presetTable_);
    applyUiSettings();

    connect(presetTable_, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *item) { handleTableItemChanged(item); });

    loadSettings();
    rebuildCpuStructureSelector();
    rebuildPresetTable();
    presetTable_->clearSelection();
    presetTable_->setCurrentItem(nullptr);
    retranslateUi();
    statusBar()->showMessage(
        text(QStringLiteral("設定ファイル: %1"), QStringLiteral("Settings file: %1"))
            .arg(repository_.configFilePath()),
        12000);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    uiSettings_.mainWindowSize = size();
    repository_.saveUiSettings(uiSettings_);
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (presetTable_ != nullptr && watched == presetTable_->viewport()
        && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!presetTable_->indexAt(mouseEvent->position().toPoint()).isValid()) {
            presetTable_->clearSelection();
            presetTable_->setCurrentItem(nullptr);
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::createMenus()
{
    presetMenu_ = menuBar()->addMenu(QString());
    newPresetAction_ = presetMenu_->addAction(QString());
    newPresetAction_->setShortcut(QKeySequence::New);
    deletePresetAction_ = presetMenu_->addAction(QString());
    deletePresetAction_->setShortcut(QKeySequence::Delete);

    cpuStructureAction_ = menuBar()->addAction(QString());

    // This direct action occupies the former "ファイル" position. The
    // separate toolbar is intentionally omitted so the table starts below the menu bar.
    settingsAction_ = menuBar()->addAction(QString());

    connect(newPresetAction_, &QAction::triggered, this, [this] { createPreset(); });
    connect(deletePresetAction_, &QAction::triggered, this, [this] { deleteSelectedPreset(); });
    connect(cpuStructureAction_, &QAction::triggered, this, [this] { editCpuStructures(); });
    connect(settingsAction_, &QAction::triggered, this, [this] { openSettings(); });

    cpuStructureCombo_ = new QComboBox(menuBar());
    cpuStructureCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    menuBar()->setCornerWidget(cpuStructureCombo_, Qt::TopRightCorner);
    connect(cpuStructureCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
                const QString id = cpuStructureCombo_->currentData().toString();
                if (id.isEmpty() || id == activeCpuStructureId_) {
                    return;
                }
                activeCpuStructureId_ = id;
                repository_.saveCpuStructures(cpuStructures_, activeCpuStructureId_);
                rebuildPresetTable();
                presetTable_->clearSelection();
                presetTable_->setCurrentItem(nullptr);
            });
    retranslateUi();
}

void MainWindow::rebuildCpuStructureSelector()
{
    if (cpuStructureCombo_ == nullptr) {
        return;
    }
    const QSignalBlocker blocker(cpuStructureCombo_);
    cpuStructureCombo_->clear();
    for (const CpuStructure &structure : cpuStructures_) {
        cpuStructureCombo_->addItem(structure.name, structure.id);
    }
    cpuStructureCombo_->setCurrentIndex(cpuStructureCombo_->findData(activeCpuStructureId_));
}

QString MainWindow::text(const QString &japanese, const QString &english) const
{
    return localizedText(uiSettings_, japanese, english);
}

void MainWindow::applyUiSettings()
{
    QPalette palette = systemPalette_;
    if (uiSettings_.themeMode == ThemeMode::Light) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#f4f4f4")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#202020")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f0f4f8")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#202020")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#f4f4f4")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#202020")));
    } else if (uiSettings_.themeMode == ThemeMode::Dark) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#202124")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e8eaed")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#292a2d")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#34363a")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#292a2d")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#e8eaed")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#e8eaed")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#303134")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e8eaed")));
        palette.setColor(QPalette::Link, QColor(QStringLiteral("#8ab4f8")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3c6eaf")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    }
    qApp->setPalette(palette);

    if (presetTable_ != nullptr) {
        QFont tableFont = qApp->font();
        tableFont.setPointSize(uiSettings_.tableFontPointSize);
        presetTable_->setFont(tableFont);
        presetTable_->verticalHeader()->setDefaultSectionSize(
            qMax(34, QFontMetrics(tableFont).height() + 10));
        if (hierarchyHeader_ != nullptr) {
            hierarchyHeader_->setFont(tableFont);
            hierarchyHeader_->setRowHeight(qMax(26, QFontMetrics(tableFont).height() + 8));
        }
    }
}

void MainWindow::retranslateUi()
{
    if (presetMenu_ != nullptr) {
        presetMenu_->setTitle(text(QStringLiteral("プリセット"), QStringLiteral("Presets")));
        newPresetAction_->setText(text(QStringLiteral("新規"), QStringLiteral("New")));
        deletePresetAction_->setText(text(QStringLiteral("削除"), QStringLiteral("Delete")));
    }
    if (cpuStructureAction_ != nullptr) {
        cpuStructureAction_->setText(text(QStringLiteral("CPU構造"), QStringLiteral("CPU Structure")));
    }
    if (settingsAction_ != nullptr) {
        settingsAction_->setText(text(QStringLiteral("設定"), QStringLiteral("Settings")));
    }
    if (cpuStructureCombo_ != nullptr) {
        cpuStructureCombo_->setToolTip(
            text(QStringLiteral("使用するCPU構造を選択"), QStringLiteral("Select CPU structure")));
    }
    if (hierarchyHeader_ != nullptr) {
        hierarchyHeader_->setFixedColumnLabels(
            text(QStringLiteral("適用"), QStringLiteral("Apply")), QStringLiteral(".exe"));
    }
    if (presetTable_ != nullptr && presetTable_->columnCount() >= 2) {
        presetTable_->horizontalHeaderItem(0)->setText(text(QStringLiteral("適用"), QStringLiteral("Apply")));
        presetTable_->horizontalHeaderItem(1)->setText(QStringLiteral(".exe"));
    }
}

void MainWindow::loadSettings()
{
    cpuStructures_ = repository_.loadCpuStructures();
    activeCpuStructureId_ = repository_.loadActiveCpuStructureId();
    const QMap<int, int> availableThreads = activeProcessorCounts();

    const auto activeIterator = std::find_if(
        cpuStructures_.cbegin(), cpuStructures_.cend(), [this](const CpuStructure &structure) {
            return structure.id == activeCpuStructureId_;
        });
    const bool activeIsAutoDetected = activeIterator != cpuStructures_.cend()
        && activeIterator->isAutoDetected;
    const bool activeIsValid = activeIterator != cpuStructures_.cend()
        && validateCpuStructure(*activeIterator, availableThreads).isEmpty();
    if (activeIsAutoDetected
        && activeIterator->autoDetectedCpuFingerprint != currentCpuFingerprint()) {
        CpuStructure automatic = createDetectedCpuStructure();
        automatic.id = activeIterator->id;
        const int index = static_cast<int>(std::distance(cpuStructures_.cbegin(), activeIterator));
        cpuStructures_[index] = automatic;
        repository_.saveCpuStructures(cpuStructures_, activeCpuStructureId_);
    } else if (!activeIsValid) {
        const CpuStructure automatic = createDetectedCpuStructure();
        cpuStructures_.append(automatic);
        activeCpuStructureId_ = automatic.id;
        repository_.saveCpuStructures(cpuStructures_, activeCpuStructureId_);
    }

    presets_ = repository_.loadProcessPresets();
}

const CpuStructure &MainWindow::activeCpuStructure() const
{
    const auto iterator = std::find_if(
        cpuStructures_.cbegin(), cpuStructures_.cend(), [this](const CpuStructure &structure) {
            return structure.id == activeCpuStructureId_;
        });
    Q_ASSERT(iterator != cpuStructures_.cend());
    return *iterator;
}

void MainWindow::rebuildPresetTable()
{
    rebuildingTable_ = true;
    activeLeaves_ = collectCpuLeaves(activeCpuStructure().roots);

    presetTable_->clearContents();
    presetTable_->setRowCount(presets_.size());
    presetTable_->setColumnCount(2 + activeLeaves_.size());
    presetTable_->setHorizontalHeaderItem(
        0, new QTableWidgetItem(text(QStringLiteral("適用"), QStringLiteral("Apply"))));
    presetTable_->setHorizontalHeaderItem(1, new QTableWidgetItem(QStringLiteral(".exe")));
    for (int column = 0; column < activeLeaves_.size(); ++column) {
        presetTable_->setHorizontalHeaderItem(
            2 + column, new QTableWidgetItem(activeLeaves_.at(column).label));
    }

    hierarchyHeader_->setHierarchy(activeCpuStructure().roots);
    presetTable_->setColumnWidth(0, uiSettings_.applyColumnWidth);
    presetTable_->setColumnWidth(1, uiSettings_.executableColumnWidth);
    for (int column = 0; column < activeLeaves_.size(); ++column) {
        presetTable_->setColumnWidth(2 + column, uiSettings_.cpuColumnWidth);
    }

    for (int row = 0; row < presets_.size(); ++row) {
        const ProcessPreset &preset = presets_.at(row);
        auto *applyButton = new QToolButton(presetTable_);
        applyButton->setText(QStringLiteral("適用"));
        applyButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        applyButton->setToolTip(QStringLiteral("このプリセットを見つかったプロセスへ適用します"));
        connect(applyButton, &QToolButton::clicked, this,
                [this, presetId = preset.id] { applyPreset(presetId); });
        presetTable_->setCellWidget(row, 0, applyButton);

        auto *exeItem = new QTableWidgetItem(preset.executableName);
        exeItem->setToolTip(QStringLiteral("対象.exe名（例: renderer.exe）"));
        presetTable_->setItem(row, 1, exeItem);

        for (int column = 0; column < activeLeaves_.size(); ++column) {
            const CpuLeaf &leaf = activeLeaves_.at(column);
            auto *cpuItem = new QTableWidgetItem;
            cpuItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            cpuItem->setTextAlignment(Qt::AlignCenter);
            cpuItem->setCheckState(preset.selectedCpuKeys.contains(
                                       cpuKey(leaf.processorGroup, leaf.logicalProcessor))
                                       ? Qt::Checked
                                       : Qt::Unchecked);
            cpuItem->setToolTip(
                QStringLiteral("Processor Group %1 / Thread No. %2")
                    .arg(leaf.processorGroup)
                    .arg(leaf.logicalProcessor));
            presetTable_->setItem(row, 2 + column, cpuItem);
        }
    }

    rebuildingTable_ = false;
}

void MainWindow::savePresets()
{
    repository_.saveProcessPresets(presets_);
}

void MainWindow::createPreset()
{
    bool accepted = false;
    QString executableName = QInputDialog::getText(
        this,
        QStringLiteral("プリセットを追加"),
        QStringLiteral("対象.exe名:"),
        QLineEdit::Normal,
        QStringLiteral("example.exe"),
        &accepted).trimmed();
    if (!accepted || executableName.isEmpty()) {
        return;
    }

    executableName = QFileInfo(executableName).fileName();
    if (!executableName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        executableName.append(QStringLiteral(".exe"));
    }

    ProcessPreset preset;
    preset.id = makeId();
    preset.executableName = executableName;
    presets_.append(preset);
    savePresets(); // New/Delete are intentionally persisted immediately.
    rebuildPresetTable();
    const int row = presets_.size() - 1;
    presetTable_->selectRow(row);
    presetTable_->setCurrentCell(row, 1);
    statusBar()->showMessage(QStringLiteral("プリセットを保存しました。"), 4000);
}

void MainWindow::deleteSelectedPreset()
{
    const int row = presetTable_->currentRow();
    if (row < 0 || row >= presets_.size()) {
        QMessageBox::information(this, QStringLiteral("プリセットを削除"),
                                 QStringLiteral("削除するプリセットの行を選択してください。"));
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("プリセットを削除"),
        QStringLiteral("「%1」を削除しますか？").arg(presets_.at(row).executableName));
    if (answer != QMessageBox::Yes) {
        return;
    }

    presets_.removeAt(row);
    savePresets();
    rebuildPresetTable();
    statusBar()->showMessage(QStringLiteral("プリセットを削除して保存しました。"), 4000);
}

void MainWindow::editCpuStructures()
{
    CpuStructureDialog dialog(cpuStructures_, activeCpuStructureId_, uiSettings_.cpuStructureDialogSize, this);
    const int dialogResult = dialog.exec();
    uiSettings_.cpuStructureDialogSize = dialog.dialogSize();
    repository_.saveUiSettings(uiSettings_);
    if (dialogResult != QDialog::Accepted) {
        return;
    }
    cpuStructures_ = dialog.structures();
    activeCpuStructureId_ = dialog.activeStructureId();
    repository_.saveCpuStructures(cpuStructures_, activeCpuStructureId_);
    rebuildCpuStructureSelector();
    rebuildPresetTable();
    statusBar()->showMessage(QStringLiteral("CPU構造プリセットを保存しました。"), 4000);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(uiSettings_, [this] { resetCustomCpuStructures(); }, this);
    const int dialogResult = dialog.exec();
    uiSettings_.settingsDialogSize = dialog.size();
    repository_.saveUiSettings(uiSettings_);
    if (dialogResult != QDialog::Accepted) {
        return;
    }
    uiSettings_ = dialog.settings();
    repository_.saveUiSettings(uiSettings_);
    applyUiSettings();
    retranslateUi();
    rebuildPresetTable();
    statusBar()->showMessage(
        text(QStringLiteral("設定を保存しました。"), QStringLiteral("Settings saved.")), 4000);
}

void MainWindow::resetCustomCpuStructures()
{
    const CpuStructure automatic = createDetectedCpuStructure();
    cpuStructures_ = {automatic};
    activeCpuStructureId_ = automatic.id;
    repository_.saveCpuStructures(cpuStructures_, activeCpuStructureId_);
    rebuildCpuStructureSelector();
    rebuildPresetTable();
    statusBar()->showMessage(
        text(QStringLiteral("カスタムCPU構造を削除しました。"),
             QStringLiteral("Custom CPU structures were deleted.")), 4000);
}

void MainWindow::handleTableItemChanged(QTableWidgetItem *item)
{
    if (rebuildingTable_ || item == nullptr) {
        return;
    }
    const int row = item->row();
    const int column = item->column();
    if (row < 0 || row >= presets_.size()) {
        return;
    }

    ProcessPreset &preset = presets_[row];
    if (column == 1) {
        preset.executableName = QFileInfo(item->text().trimmed()).fileName();
    } else if (column >= 2 && column - 2 < activeLeaves_.size()) {
        const CpuLeaf &leaf = activeLeaves_.at(column - 2);
        const QString key = cpuKey(leaf.processorGroup, leaf.logicalProcessor);
        if (item->checkState() == Qt::Checked) {
            preset.selectedCpuKeys.insert(key);
        } else {
            preset.selectedCpuKeys.remove(key);
        }
    } else {
        return;
    }
    savePresets();
    statusBar()->showMessage(QStringLiteral("変更を保存しました。"), 2500);
}

int MainWindow::presetIndexById(const QString &id) const
{
    for (int index = 0; index < presets_.size(); ++index) {
        if (presets_.at(index).id == id) {
            return index;
        }
    }
    return -1;
}

QVector<CpuLeaf> MainWindow::selectedLeavesFor(const ProcessPreset &preset) const
{
    QVector<CpuLeaf> selected;
    for (const CpuLeaf &leaf : activeLeaves_) {
        if (preset.selectedCpuKeys.contains(cpuKey(leaf.processorGroup, leaf.logicalProcessor))) {
            selected.append(leaf);
        }
    }
    return selected;
}

void MainWindow::applyPreset(const QString &presetId)
{
    const int presetIndex = presetIndexById(presetId);
    if (presetIndex < 0) {
        return;
    }
    const ProcessPreset &preset = presets_.at(presetIndex);
    const QString executableName = QFileInfo(preset.executableName.trimmed()).fileName();
    if (executableName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("CPUアフィニティを適用"),
                             QStringLiteral("対象.exe名を入力してください。"));
        return;
    }

    const QVector<CpuLeaf> selectedLeaves = selectedLeavesFor(preset);
    if (selectedLeaves.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("CPUアフィニティを適用"),
                             QStringLiteral("適用するCPUスレッドを少なくとも1つ選択してください。"));
        return;
    }

    const QVector<ProcessInfo> processes = findProcessesByExecutable(executableName);
    if (processes.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("CPUアフィニティを適用"),
                                 QStringLiteral("%1 の実行中プロセスは見つかりませんでした。")
                                     .arg(executableName));
        return;
    }

    quint32 processId = processes.first().processId;
    if (processes.size() > 1) {
        ProcessSelectionDialog dialog(processes, uiSettings_.processSelectionDialogSize, this);
        const int dialogResult = dialog.exec();
        uiSettings_.processSelectionDialogSize = dialog.dialogSize();
        repository_.saveUiSettings(uiSettings_);
        if (dialogResult != QDialog::Accepted) {
            return;
        }
        const std::optional<quint32> selected = dialog.selectedProcessId();
        if (!selected) {
            return;
        }
        processId = *selected;
    }

    const AffinityApplyResult result = applyAffinity(processId, selectedLeaves);
    if (!result.success) {
        QMessageBox::warning(this, QStringLiteral("CPUアフィニティを適用"), result.message);
        return;
    }
    statusBar()->showMessage(result.message, 6000);
    QMessageBox::information(this, QStringLiteral("CPUアフィニティを適用"), result.message);
}

} // namespace ProcessPresets
