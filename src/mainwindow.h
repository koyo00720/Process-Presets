#pragma once

#include "models.h"
#include "settingsrepository.h"

#include <QMainWindow>
#include <QPalette>

class QAction;
class QComboBox;
class QMenu;
class QTableWidget;
class QTableWidgetItem;
class QCloseEvent;
class QEvent;
class QTimer;
class QToolButton;

namespace ProcessPresets {

class HierarchicalHeaderView;
class PresetSelectionOverlay;

class MainWindow final : public QMainWindow {
public:
    MainWindow();

private:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void createMenus();
    void rebuildCpuStructureSelector();
    void loadSettings();
    void rebuildPresetTable();
    void createPreset();
    void deleteSelectedPreset();
    void deletePresetRows(const QVector<int> &rows);
    void duplicatePreset(int row);
    void showPresetContextMenu(const QPoint &position);
    void groupSelectedPresets();
    void ungroupSelectedPresets();
    void editCpuStructures();
    void openSettings();
    void resetCustomCpuStructures();
    void applyUiSettings();
    void retranslateUi();
    QString text(const QString &japanese, const QString &english) const;
    void savePresets();
    void handleTableItemChanged(QTableWidgetItem *item);
    void applyPreset(const QString &presetId, bool periodic = false);
    void resetPreset(const QString &presetId);
    void moveSelectedPreset(int offset);
    void updateMoveButtons();
    void updatePeriodicTimer();
    void applyPeriodicPresets();
    bool isApplyAllowedFor(const ProcessPreset &preset) const;
    bool executableHasOrderConflict(const QString &executableName) const;
    QVector<int> selectedPresetRows() const;
    QVector<int> groupedRowsForId(const QString &groupId) const;
    void normalizeGroups();
    void applyGroupSpans();
    const CpuStructure &activeCpuStructure() const;
    int presetIndexById(const QString &id) const;
    QVector<CpuLeaf> selectedLeavesFor(const ProcessPreset &preset) const;

    SettingsRepository repository_;
    UiSettings uiSettings_;
    QPalette systemPalette_;
    QVector<CpuStructure> cpuStructures_;
    QString activeCpuStructureId_;
    QVector<ProcessPreset> presets_;
    QVector<CpuLeaf> activeLeaves_;
    QTableWidget *presetTable_ = nullptr;
    HierarchicalHeaderView *hierarchyHeader_ = nullptr;
    QMenu *presetMenu_ = nullptr;
    QAction *newPresetAction_ = nullptr;
    QAction *deletePresetAction_ = nullptr;
    QAction *cpuStructureAction_ = nullptr;
    QAction *settingsAction_ = nullptr;
    QComboBox *cpuStructureCombo_ = nullptr;
    QToolButton *moveUpButton_ = nullptr;
    QToolButton *moveDownButton_ = nullptr;
    QTimer *periodicApplyTimer_ = nullptr;
    PresetSelectionOverlay *selectionOverlay_ = nullptr;
    bool rebuildingTable_ = false;
};

} // namespace ProcessPresets
