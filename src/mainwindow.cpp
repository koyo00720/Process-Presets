#include "mainwindow.h"

#include "cpudiscovery.h"
#include "cpustructuredialog.h"
#include "hierarchicalheaderview.h"
#include "processhelper.h"
#include "processselectiondialog.h"
#include "settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QCloseEvent>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QProxyStyle>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace ProcessPresets {

class PresetTableStyle final : public QProxyStyle {
public:
    PresetTableStyle()
        : QProxyStyle()
    {
    }

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption *option,
                       QPainter *painter,
                       const QWidget *widget = nullptr) const override
    {
        if (option != nullptr
            && (element == PE_PanelItemViewItem || element == PE_PanelItemViewRow)) {
            QStyleOption plainOption(*option);
            plainOption.state &= ~(State_Selected | State_MouseOver | State_HasFocus);
            QProxyStyle::drawPrimitive(element, &plainOption, painter, widget);
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawControl(ControlElement element,
                     const QStyleOption *option,
                     QPainter *painter,
                     const QWidget *widget = nullptr) const override
    {
        if (element == CE_ItemViewItem) {
            if (const auto *viewOption = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
                QStyleOptionViewItem plainOption(*viewOption);
                plainOption.state &= ~(State_Selected | State_MouseOver | State_HasFocus);
                QProxyStyle::drawControl(element, &plainOption, painter, widget);
                return;
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
};

class PresetItemDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // Qt styles commonly render hover and per-cell selection with the
        // same highlight treatment.  Selection is instead drawn once around
        // the whole row by PresetSelectionOverlay below.
        QStyleOptionViewItem plainOption(option);
        plainOption.state &= ~(QStyle::State_Selected
                               | QStyle::State_MouseOver
                               | QStyle::State_HasFocus);
        QStyledItemDelegate::paint(painter, plainOption, index);
    }
};

class PresetSelectionOverlay final : public QWidget {
public:
    explicit PresetSelectionOverlay(QTableWidget *table)
        : QWidget(table->viewport())
        , table_(table)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
        refresh();
    }

    void refresh()
    {
        if (table_ == nullptr) {
            return;
        }
        setGeometry(table_->viewport()->rect());
        raise();
        update();
    }

    void setSeparatorRows(const QSet<int> &rows)
    {
        separatorRows_ = rows;
        refresh();
    }

    void setSeparatorStyle(int thickness, int transparencyPercent)
    {
        separatorLineThickness_ = qBound(1, thickness, 20);
        separatorLineTransparencyPercent_ = qBound(0, transparencyPercent, 100);
        refresh();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (table_ == nullptr || table_->selectionModel() == nullptr) {
            return;
        }
        QPainter painter(this);

        if (!separatorRows_.isEmpty() && table_->columnCount() > 0) {
            const auto *header = table_->horizontalHeader();
            if (header != nullptr) {
                QColor separatorColor = table_->palette().color(QPalette::Text);
                separatorColor.setAlphaF(
                    static_cast<float>(100 - separatorLineTransparencyPercent_) / 100.0F);

                const int firstColumn = 0;
                const int lastColumn = table_->columnCount() - 1;
                const int tableLeft = header->sectionViewportPosition(firstColumn);
                const int tableRight = header->sectionViewportPosition(lastColumn)
                    + header->sectionSize(lastColumn) - 1;
                const int lineLeft = qMax(rect().left(), tableLeft);
                const int lineRight = qMin(rect().right(), tableRight);

                if (lineLeft <= lineRight) {
                    for (const int row : separatorRows_) {
                        if (row < 0 || row >= table_->rowCount() - 1) {
                            continue;
                        }
                        const int nextRowTop = table_->rowViewportPosition(row + 1);
                        const QRect separatorRect(
                            lineLeft,
                            nextRowTop - separatorLineThickness_,
                            lineRight - lineLeft + 1,
                            separatorLineThickness_);
                        const QRect visibleSeparator = separatorRect.intersected(rect());
                        if (!visibleSeparator.isEmpty()) {
                            painter.fillRect(visibleSeparator, separatorColor);
                        }
                    }
                }
            }
        }

        QPen pen(table_->palette().color(QPalette::Highlight));
        pen.setWidth(2);
        painter.setPen(pen);

        QSet<int> selectedRows;
        for (const QModelIndex &index : table_->selectionModel()->selectedRows()) {
            selectedRows.insert(index.row());
        }
        for (const int row : selectedRows) {
            if (row < 0 || row >= table_->rowCount() || table_->columnCount() == 0) {
                continue;
            }
            const auto *header = table_->horizontalHeader();
            if (header == nullptr) {
                continue;
            }

            // The Enabled column represents the preset state itself, so the
            // selection outline begins at the executable column instead.
            constexpr int firstOutlinedColumn = 1;
            const int lastColumn = table_->columnCount() - 1;
            if (lastColumn < firstOutlinedColumn) {
                continue;
            }
            const int rowTop = table_->rowViewportPosition(row);
            const int rowHeight = table_->rowHeight(row);
            if (rowHeight <= 0) {
                continue;
            }

            // sectionViewportPosition() retains the actual column position
            // even after the first/last column has scrolled out of view.
            // This lets us distinguish a clipped row end from an end that is
            // genuinely visible in the viewport.
            const int rowLeft = header->sectionViewportPosition(firstOutlinedColumn);
            const int rowRight = header->sectionViewportPosition(lastColumn)
                                 + header->sectionSize(lastColumn) - 1;
            const QRect fullRowRect(QPoint(rowLeft, rowTop),
                                    QPoint(rowRight, rowTop + rowHeight - 1));
            const QRect viewportBounds = rect().adjusted(1, 1, -2, -2);
            const QRect visibleRowRect = fullRowRect.intersected(viewportBounds);
            if (visibleRowRect.isEmpty()) {
                continue;
            }

            const int borderLeft = rowLeft + 1;
            const int borderRight = rowRight - 2;
            const int borderTop = rowTop + 1;
            const int borderBottom = rowTop + rowHeight - 2;
            const bool leftEndVisible = rowLeft >= rect().left() && rowLeft <= rect().right();
            const bool rightEndVisible = rowRight >= rect().left() && rowRight <= rect().right();
            const int horizontalLeft = qMax(borderLeft, viewportBounds.left());
            const int horizontalRight = qMin(borderRight, viewportBounds.right());
            const int verticalTop = qMax(borderTop, viewportBounds.top());
            const int verticalBottom = qMin(borderBottom, viewportBounds.bottom());

            if (horizontalLeft <= horizontalRight) {
                if (borderTop >= viewportBounds.top() && borderTop <= viewportBounds.bottom()) {
                    painter.drawLine(horizontalLeft, borderTop, horizontalRight, borderTop);
                }
                if (borderBottom >= viewportBounds.top()
                    && borderBottom <= viewportBounds.bottom()) {
                    painter.drawLine(horizontalLeft, borderBottom, horizontalRight, borderBottom);
                }
            }
            if (verticalTop <= verticalBottom) {
                // Draw side borders only if the actual beginning/end of the
                // row is visible.  A horizontally clipped row therefore has
                // no artificial vertical border at the viewport edge.
                if (leftEndVisible
                    && borderLeft >= viewportBounds.left() && borderLeft <= viewportBounds.right()) {
                    painter.drawLine(borderLeft, verticalTop, borderLeft, verticalBottom);
                }
                if (rightEndVisible
                    && borderRight >= viewportBounds.left() && borderRight <= viewportBounds.right()) {
                    painter.drawLine(borderRight, verticalTop, borderRight, verticalBottom);
                }
            }
        }
    }

private:
    QTableWidget *table_ = nullptr;
    QSet<int> separatorRows_;
    int separatorLineThickness_ = 1;
    int separatorLineTransparencyPercent_ = 50;
};

class CpuUsageRowWidget final : public QWidget {
public:
    CpuUsageRowWidget(QTableWidget *table, int firstCpuColumn, QWidget *parent)
        : QWidget(parent)
        , table_(table)
        , firstCpuColumn_(firstCpuColumn)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setCpuLeaves(const QVector<CpuLeaf> &leaves)
    {
        leaves_ = leaves;
        update();
    }

    void setUsagePercentages(const QHash<QString, double> &usagePercentages)
    {
        usagePercentages_ = usagePercentages;
        update();
    }

    void setLabel(const QString &label)
    {
        label_ = label;
        update();
    }

    void setBarWidth(int width)
    {
        barWidth_ = qMax(1, width);
        update();
    }

    void setBarAppearance(const QString &colorName, int transparencyPercent)
    {
        barColor_ = QColor(colorName);
        barTransparencyPercent_ = qBound(0, transparencyPercent, 100);
        update();
    }

    void refresh()
    {
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().brush(QPalette::Window));
        if (table_ == nullptr || table_->horizontalHeader() == nullptr
            || table_->columnCount() <= firstCpuColumn_) {
            return;
        }

        const auto *header = table_->horizontalHeader();
        const QRect tableViewportGeometry = table_->viewport()->geometry();
        const int contentLeft = tableViewportGeometry.left();
        const QRect contentRect(
            contentLeft, 0, tableViewportGeometry.width(), height());
        if (contentRect.isEmpty()) {
            return;
        }
        painter.setClipRect(contentRect);
        painter.fillRect(contentRect, palette().brush(QPalette::Base));

        const int labelLeft = contentLeft + header->sectionViewportPosition(0);
        const int firstCpuLeft = contentLeft
            + header->sectionViewportPosition(firstCpuColumn_);
        const QRect labelRect(labelLeft, 0, firstCpuLeft - labelLeft, height());
        const QRect visibleLabelRect = labelRect.intersected(contentRect);
        if (!visibleLabelRect.isEmpty()) {
            painter.fillRect(visibleLabelRect, palette().brush(QPalette::Base));
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawRect(labelRect.adjusted(0, 0, -1, -1));
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(visibleLabelRect.adjusted(4, 1, -4, -1),
                             Qt::AlignCenter,
                             label_);
        }

        for (int leafIndex = 0; leafIndex < leaves_.size(); ++leafIndex) {
            const int column = firstCpuColumn_ + leafIndex;
            if (column >= table_->columnCount() || table_->isColumnHidden(column)) {
                continue;
            }
            const QRect cellRect(
                contentLeft + header->sectionViewportPosition(column),
                0,
                header->sectionSize(column),
                height());
            if (!cellRect.intersects(contentRect)) {
                continue;
            }

            painter.fillRect(cellRect.intersected(contentRect),
                             palette().brush(QPalette::Base));
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawRect(cellRect.adjusted(0, 0, -1, -1));

            const QString key = cpuKey(leaves_.at(leafIndex).processorGroup,
                                       leaves_.at(leafIndex).logicalProcessor);
            const double usage = usagePercentages_.value(key, -1.0);
            if (usage < 0.0) {
                painter.setPen(palette().color(QPalette::Text));
                painter.drawText(cellRect, Qt::AlignCenter, QStringLiteral("—"));
                continue;
            }

            const QRect graphRect = cellRect.adjusted(3, 3, -3, -3);
            if (!graphRect.isEmpty()) {
                int barHeight = qRound(graphRect.height() * qBound(0.0, usage, 100.0) / 100.0);
                if (usage > 0.0) {
                    barHeight = qMax(1, barHeight);
                }
                const int barWidth = qBound(1, barWidth_, graphRect.width());
                const QRect barRect(
                    graphRect.center().x() - barWidth / 2,
                    graphRect.bottom() - barHeight + 1,
                    barWidth,
                    barHeight);
                QColor fillColor = barColor_.isValid()
                    ? barColor_
                    : palette().color(QPalette::Text);
                fillColor.setAlpha(qRound(
                    255.0 * (100 - barTransparencyPercent_) / 100.0));
                painter.fillRect(barRect, fillColor);
            }
        }
    }

private:
    QTableWidget *table_ = nullptr;
    int firstCpuColumn_ = 0;
    QVector<CpuLeaf> leaves_;
    QHash<QString, double> usagePercentages_;
    QString label_ = QStringLiteral("CPU使用率");
    int barWidth_ = 28;
    QColor barColor_;
    int barTransparencyPercent_ = 0;
};

namespace {

constexpr int kEnabledColumn = 0;
constexpr int kExecutableColumn = 1;
constexpr int kApplyOnceColumn = 2;
constexpr int kApplyPeriodicColumn = 3;
constexpr int kResetColumn = 4;
constexpr int kOrderColumn = 5;
constexpr int kProcessPriorityColumn = 6;
constexpr int kPidColumn = 7;
constexpr int kProcessStartColumn = 8;
constexpr int kProcessCpuColumn = 9;
constexpr int kProcessMemoryColumn = 10;
constexpr int kFirstCpuColumn = 11;

QString executableKey(const QString &executableName)
{
    return QFileInfo(executableName.trimmed()).fileName().toCaseFolded();
}

QString processStartTimeText(const QDateTime &startTime, const UiSettings &settings)
{
    if (!startTime.isValid()) {
        return QStringLiteral("—");
    }
    return startTime.toString(usesEnglish(settings)
                                  ? QStringLiteral("yyyy-MM-dd HH:mm:ss")
                                  : QStringLiteral("yyyy/MM/dd HH:mm:ss"));
}

QString memoryUsageText(quint64 bytes)
{
    constexpr double bytesPerMegabyte = 1024.0 * 1024.0;
    constexpr double bytesPerGigabyte = 1024.0 * bytesPerMegabyte;
    if (bytes >= static_cast<quint64>(bytesPerGigabyte)) {
        return QStringLiteral("%1 GB").arg(bytes / bytesPerGigabyte, 0, 'f', 2);
    }
    return QStringLiteral("%1 MB").arg(bytes / bytesPerMegabyte, 0, 'f', 1);
}

QString processPriorityText(const UiSettings &settings, ProcessPriorityClass priority)
{
    const bool english = usesEnglish(settings);
    switch (priority) {
    case ProcessPriorityClass::Idle:
        return english ? QStringLiteral("Low") : QStringLiteral("低");
    case ProcessPriorityClass::BelowNormal:
        return english ? QStringLiteral("Below normal") : QStringLiteral("通常以下");
    case ProcessPriorityClass::Normal:
        return english ? QStringLiteral("Normal") : QStringLiteral("通常");
    case ProcessPriorityClass::AboveNormal:
        return english ? QStringLiteral("Above normal") : QStringLiteral("通常以上");
    case ProcessPriorityClass::High:
        return english ? QStringLiteral("High") : QStringLiteral("高");
    case ProcessPriorityClass::Realtime:
        return english ? QStringLiteral("Realtime") : QStringLiteral("リアルタイム");
    }
    return english ? QStringLiteral("Normal") : QStringLiteral("通常");
}

} // namespace

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("Process Presets v%1").arg(QCoreApplication::applicationVersion()));
    setMinimumSize(560, 280);

    uiSettings_ = repository_.loadUiSettings();
    resize(uiSettings_.mainWindowSize);
    systemPalette_ = qApp->palette();
    applyUiSettings();
    createMenus();

    presetTable_ = new QTableWidget(this);
    presetTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    presetTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    presetTable_->setEditTriggers(QAbstractItemView::DoubleClicked
                                  | QAbstractItemView::SelectedClicked
                                  | QAbstractItemView::EditKeyPressed);
    presetTable_->setAlternatingRowColors(true);
    presetTable_->setShowGrid(true);
    // ScrollPerItem snaps to column boundaries.  Per-pixel scrolling keeps
    // the usual scrollbar behavior without forcing an impractically small
    // one-pixel step.
    presetTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    presetTable_->verticalHeader()->setVisible(false);
    presetTable_->verticalHeader()->setMinimumSectionSize(1);
    presetTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    presetTable_->verticalHeader()->setDefaultSectionSize(uiSettings_.tableRowHeight);
    presetTable_->setCornerButtonEnabled(false);
    // The platform style paints a selection accent independently of the
    // delegate.  Suppress that per-cell treatment at the table-style level;
    // PresetSelectionOverlay remains the sole selection indicator.
    presetTable_->setStyle(new PresetTableStyle);
    presetTable_->setItemDelegate(new PresetItemDelegate(presetTable_));
    presetTable_->viewport()->installEventFilter(this);
    presetTable_->viewport()->setContextMenuPolicy(Qt::DefaultContextMenu);

    hierarchyHeader_ = new HierarchicalHeaderView(presetTable_);
    presetTable_->setHorizontalHeader(hierarchyHeader_);
    hierarchyHeader_->setSectionResizeMode(QHeaderView::Fixed);
    hierarchyHeader_->setStretchLastSection(false);
    hierarchyHeader_->setSectionsMovable(false);
    auto *centralPanel = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(centralPanel);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(4);
    centralLayout->addWidget(presetTable_, 1);

    cpuUsageRow_ = new CpuUsageRowWidget(presetTable_, kFirstCpuColumn, centralPanel);
    centralLayout->addWidget(cpuUsageRow_);

    auto *moveLayout = new QHBoxLayout;
    moveLayout->setContentsMargins(6, 0, 6, 4);
    resetAllAssignmentsButton_ = new QPushButton(centralPanel);
    moveLayout->addWidget(resetAllAssignmentsButton_);
    moveLayout->addStretch();
    moveUpButton_ = new QToolButton(centralPanel);
    moveUpButton_->setText(QStringLiteral("▲"));
    moveDownButton_ = new QToolButton(centralPanel);
    moveDownButton_->setText(QStringLiteral("▼"));
    moveLayout->addWidget(moveUpButton_);
    moveLayout->addWidget(moveDownButton_);
    centralLayout->addLayout(moveLayout);
    setCentralWidget(centralPanel);
    applyUiSettings();

    selectionOverlay_ = new PresetSelectionOverlay(presetTable_);
    selectionOverlay_->setSeparatorStyle(uiSettings_.separatorLineThickness,
                                         uiSettings_.separatorLineTransparencyPercent);

    connect(presetTable_, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem *item) { handleTableItemChanged(item); });
    connect(presetTable_, &QTableWidget::itemSelectionChanged, this,
            [this] {
                updateMoveButtons();
                selectionOverlay_->refresh();
            });
    connect(presetTable_->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { selectionOverlay_->refresh(); });
    connect(presetTable_->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) {
                selectionOverlay_->refresh();
                cpuUsageRow_->refresh();
            });
    connect(hierarchyHeader_, &QHeaderView::sectionResized, this,
            [this](int, int, int) { cpuUsageRow_->refresh(); });
    connect(hierarchyHeader_, &QHeaderView::geometriesChanged, this,
            [this] { cpuUsageRow_->refresh(); });
    connect(moveUpButton_, &QToolButton::clicked, this, [this] { moveSelectedPreset(-1); });
    connect(moveDownButton_, &QToolButton::clicked, this, [this] { moveSelectedPreset(1); });
    connect(resetAllAssignmentsButton_, &QPushButton::clicked, this,
            [this] { resetAllProcessOrderAssignments(); });

    periodicApplyTimer_ = new QTimer(this);
    connect(periodicApplyTimer_, &QTimer::timeout, this, [this] { applyPeriodicPresets(); });
    performanceClock_.start();
    performanceTimer_ = new QTimer(this);
    connect(performanceTimer_, &QTimer::timeout, this, [this] { updatePerformanceMetrics(); });

    loadSettings();
    rebuildCpuStructureSelector();
    rebuildPresetTable();
    presetTable_->clearSelection();
    presetTable_->setCurrentItem(nullptr);
    updateMoveButtons();
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
    if (presetTable_ != nullptr && event->type() == QEvent::ContextMenu) {
        const auto *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        QPoint viewportPosition;
        if (watched == presetTable_->viewport()) {
            viewportPosition = contextMenuEvent->pos();
        } else if (auto *cellWidget = qobject_cast<QWidget *>(watched)) {
            viewportPosition = presetTable_->viewport()->mapFromGlobal(
                cellWidget->mapToGlobal(contextMenuEvent->pos()));
        }

        if (presetTable_->indexAt(viewportPosition).isValid()) {
            showPresetContextMenu(viewportPosition);
            return true;
        }
    }

    if (presetTable_ != nullptr && watched == presetTable_->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (!presetTable_->indexAt(mouseEvent->position().toPoint()).isValid()) {
                presetTable_->clearSelection();
                presetTable_->setCurrentItem(nullptr);
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            const auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QModelIndex clickedIndex = presetTable_->indexAt(
                mouseEvent->position().toPoint());
            if (mouseEvent->button() == Qt::LeftButton
                && clickedIndex.isValid()
                && clickedIndex.column() == kEnabledColumn
                && clickedIndex.row() >= 0 && clickedIndex.row() < presets_.size()) {
                const QString groupId = presets_.at(clickedIndex.row()).groupId;
                if (!groupId.isEmpty()) {
                    // The enabled checkbox can rebuild the table while it is
                    // being toggled.  Select the group after that update has
                    // completed so every member remains selected.
                    QTimer::singleShot(0, this, [this, groupId] {
                        if (presetTable_ == nullptr || presetTable_->selectionModel() == nullptr) {
                            return;
                        }
                        const QVector<int> groupRows = groupedRowsForId(groupId);
                        if (groupRows.size() < 2) {
                            return;
                        }
                        for (const int row : groupRows) {
                            presetTable_->selectionModel()->select(
                                presetTable_->model()->index(row, kExecutableColumn),
                                QItemSelectionModel::Select | QItemSelectionModel::Rows);
                        }
                        presetTable_->selectionModel()->setCurrentIndex(
                            presetTable_->model()->index(groupRows.first(), kExecutableColumn),
                            QItemSelectionModel::NoUpdate);
                    });
                }
            }
        } else if (event->type() == QEvent::Resize && selectionOverlay_ != nullptr) {
            selectionOverlay_->refresh();
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
        presetTable_->verticalHeader()->setDefaultSectionSize(uiSettings_.tableRowHeight);
        if (hierarchyHeader_ != nullptr) {
            hierarchyHeader_->setFont(tableFont);
            hierarchyHeader_->setRowHeight(qMax(26, QFontMetrics(tableFont).height() + 8));
        }
        if (cpuUsageRow_ != nullptr) {
            cpuUsageRow_->setFont(tableFont);
        }
    }
    if (selectionOverlay_ != nullptr) {
        selectionOverlay_->setSeparatorStyle(uiSettings_.separatorLineThickness,
                                             uiSettings_.separatorLineTransparencyPercent);
    }
    if (cpuUsageRow_ != nullptr) {
        cpuUsageRow_->setFixedHeight(uiSettings_.cpuUsageRowHeight);
        cpuUsageRow_->setBarWidth(uiSettings_.cpuUsageBarWidth);
        cpuUsageRow_->setBarAppearance(uiSettings_.cpuUsageBarColor,
                                       uiSettings_.cpuUsageBarTransparencyPercent);
        cpuUsageRow_->setVisible(uiSettings_.showCpuUsageRow);
        cpuUsageRow_->refresh();
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
            text(QStringLiteral("有効"), QStringLiteral("Enabled")),
            QStringLiteral(".exe"),
            text(QStringLiteral("適用"), QStringLiteral("Apply")),
            text(QStringLiteral("1回"), QStringLiteral("Once")),
            text(QStringLiteral("定期"), QStringLiteral("Periodic")),
            text(QStringLiteral("リセット"), QStringLiteral("Reset")),
            text(QStringLiteral("順番"), QStringLiteral("Order")),
            text(QStringLiteral("優先度"), QStringLiteral("Priority")),
            QStringLiteral("PID"),
            text(QStringLiteral("開始"), QStringLiteral("Start")),
            QStringLiteral("CPU"),
            text(QStringLiteral("メモリ"), QStringLiteral("Memory")));
    }
    if (presetTable_ != nullptr && presetTable_->columnCount() >= kFirstCpuColumn) {
        presetTable_->horizontalHeaderItem(kEnabledColumn)->setText(
            text(QStringLiteral("有効"), QStringLiteral("Enabled")));
        presetTable_->horizontalHeaderItem(kExecutableColumn)->setText(QStringLiteral(".exe"));
        presetTable_->horizontalHeaderItem(kApplyOnceColumn)->setText(
            text(QStringLiteral("1回"), QStringLiteral("Once")));
        presetTable_->horizontalHeaderItem(kApplyPeriodicColumn)->setText(
            text(QStringLiteral("定期"), QStringLiteral("Periodic")));
        presetTable_->horizontalHeaderItem(kResetColumn)->setText(
            text(QStringLiteral("リセット"), QStringLiteral("Reset")));
        presetTable_->horizontalHeaderItem(kOrderColumn)->setText(
            text(QStringLiteral("順番"), QStringLiteral("Order")));
        presetTable_->horizontalHeaderItem(kProcessPriorityColumn)->setText(
            text(QStringLiteral("優先度"), QStringLiteral("Priority")));
        presetTable_->horizontalHeaderItem(kPidColumn)->setText(QStringLiteral("PID"));
        presetTable_->horizontalHeaderItem(kProcessStartColumn)->setText(
            text(QStringLiteral("開始"), QStringLiteral("Start")));
        presetTable_->horizontalHeaderItem(kProcessCpuColumn)->setText(QStringLiteral("CPU"));
        presetTable_->horizontalHeaderItem(kProcessMemoryColumn)->setText(
            text(QStringLiteral("メモリ"), QStringLiteral("Memory")));
    }
    if (cpuUsageRow_ != nullptr) {
        cpuUsageRow_->setLabel(text(QStringLiteral("CPU使用率"), QStringLiteral("CPU Usage")));
    }
    if (moveUpButton_ != nullptr) {
        moveUpButton_->setToolTip(text(QStringLiteral("選択したプリセットを上へ移動"),
                                       QStringLiteral("Move selected preset up")));
        moveDownButton_->setToolTip(text(QStringLiteral("選択したプリセットを下へ移動"),
                                         QStringLiteral("Move selected preset down")));
    }
    if (resetAllAssignmentsButton_ != nullptr) {
        resetAllAssignmentsButton_->setText(
            text(QStringLiteral("全ての適用状態をリセット"),
                 QStringLiteral("Reset All Applied States")));
        resetAllAssignmentsButton_->setToolTip(text(
            QStringLiteral("順番のPID割り当てと、実際の担当状態を全てクリア"),
            QStringLiteral("Clear all order-to-PID mappings and actual ownership")));
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
    processOrderAssignments_ = repository_.loadProcessOrderAssignments();
    processPresetOwnerships_ = repository_.loadProcessPresetOwnerships();
    normalizeGroups();
    if (pruneProcessOrderAssignments()) {
        saveProcessOrderAssignments();
    }
    if (pruneProcessPresetOwnerships()) {
        saveProcessPresetOwnerships();
    }
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
    const bool assignmentsChanged = pruneProcessOrderAssignments();
    const bool ownershipsChanged = pruneProcessPresetOwnerships();

    presetTable_->clearSpans();
    presetTable_->clearContents();
    presetTable_->setRowCount(presets_.size());
    presetTable_->setColumnCount(kFirstCpuColumn + activeLeaves_.size());
    presetTable_->setHorizontalHeaderItem(
        kEnabledColumn, new QTableWidgetItem(text(QStringLiteral("有効"), QStringLiteral("Enabled"))));
    presetTable_->setHorizontalHeaderItem(
        kExecutableColumn, new QTableWidgetItem(QStringLiteral(".exe")));
    presetTable_->setHorizontalHeaderItem(
        kApplyOnceColumn, new QTableWidgetItem(text(QStringLiteral("1回"), QStringLiteral("Once"))));
    presetTable_->setHorizontalHeaderItem(
        kApplyPeriodicColumn,
        new QTableWidgetItem(text(QStringLiteral("定期"), QStringLiteral("Periodic"))));
    presetTable_->setHorizontalHeaderItem(
        kResetColumn, new QTableWidgetItem(text(QStringLiteral("リセット"), QStringLiteral("Reset"))));
    presetTable_->setHorizontalHeaderItem(
        kOrderColumn, new QTableWidgetItem(text(QStringLiteral("順番"), QStringLiteral("Order"))));
    presetTable_->setHorizontalHeaderItem(
        kProcessPriorityColumn,
        new QTableWidgetItem(text(QStringLiteral("優先度"), QStringLiteral("Priority"))));
    presetTable_->setHorizontalHeaderItem(
        kPidColumn, new QTableWidgetItem(QStringLiteral("PID")));
    presetTable_->setHorizontalHeaderItem(
        kProcessStartColumn,
        new QTableWidgetItem(text(QStringLiteral("開始"), QStringLiteral("Start"))));
    presetTable_->setHorizontalHeaderItem(
        kProcessCpuColumn, new QTableWidgetItem(QStringLiteral("CPU")));
    presetTable_->setHorizontalHeaderItem(
        kProcessMemoryColumn,
        new QTableWidgetItem(text(QStringLiteral("メモリ"), QStringLiteral("Memory"))));
    for (int column = 0; column < activeLeaves_.size(); ++column) {
        presetTable_->setHorizontalHeaderItem(
            kFirstCpuColumn + column, new QTableWidgetItem(activeLeaves_.at(column).label));
    }

    hierarchyHeader_->setHierarchy(activeCpuStructure().roots);
    cpuUsageRow_->setCpuLeaves(activeLeaves_);
    presetTable_->setColumnWidth(kEnabledColumn, uiSettings_.enabledColumnWidth);
    presetTable_->setColumnWidth(kExecutableColumn, uiSettings_.executableColumnWidth);
    presetTable_->setColumnWidth(kApplyOnceColumn, uiSettings_.applyOnceColumnWidth);
    presetTable_->setColumnWidth(kApplyPeriodicColumn, uiSettings_.periodicColumnWidth);
    presetTable_->setColumnWidth(kResetColumn, uiSettings_.resetColumnWidth);
    presetTable_->setColumnWidth(kOrderColumn, uiSettings_.orderColumnWidth);
    presetTable_->setColumnWidth(kProcessPriorityColumn, uiSettings_.processPriorityColumnWidth);
    presetTable_->setColumnWidth(kPidColumn, uiSettings_.pidColumnWidth);
    presetTable_->setColumnWidth(kProcessStartColumn, uiSettings_.processStartColumnWidth);
    presetTable_->setColumnWidth(kProcessCpuColumn, uiSettings_.processCpuColumnWidth);
    presetTable_->setColumnWidth(kProcessMemoryColumn, uiSettings_.processMemoryColumnWidth);
    presetTable_->setColumnHidden(kProcessStartColumn, !uiSettings_.showProcessStartColumn);
    presetTable_->setColumnHidden(kProcessCpuColumn, !uiSettings_.showProcessCpuColumn);
    presetTable_->setColumnHidden(kProcessMemoryColumn, !uiSettings_.showProcessMemoryColumn);
    for (int column = 0; column < activeLeaves_.size(); ++column) {
        presetTable_->setColumnWidth(kFirstCpuColumn + column, uiSettings_.cpuColumnWidth);
    }

    const auto installCellContextMenu = [this](QWidget *widget) {
        widget->setContextMenuPolicy(Qt::DefaultContextMenu);
        widget->installEventFilter(this);
    };

    bool normalizedOrders = false;
    for (int row = 0; row < presets_.size(); ++row) {
        ProcessPreset &preset = presets_[row];
        const int matchingPresetCount = matchingPresetCountForExecutable(preset.executableName);
        if (!preset.applyToAll && preset.order > matchingPresetCount) {
            preset.order = 1;
            normalizedOrders = true;
        }

        auto *enabledItem = new QTableWidgetItem;
        enabledItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        enabledItem->setTextAlignment(Qt::AlignCenter);
        enabledItem->setCheckState(preset.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setToolTip(text(QStringLiteral("このプリセットを有効にする"),
                                     QStringLiteral("Enable this preset")));
        presetTable_->setItem(row, kEnabledColumn, enabledItem);

        const bool orderConflict = executableHasOrderConflict(preset.executableName);
        const bool applyAllowed = preset.enabled && !orderConflict;
        auto *applyButton = new QToolButton(presetTable_);
        applyButton->setText(text(QStringLiteral("適用"), QStringLiteral("Apply")));
        applyButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        applyButton->setToolTip(text(QStringLiteral("このプリセットを見つかったプロセスへ適用します"),
                                     QStringLiteral("Apply this preset to the selected process.")));
        applyButton->setEnabled(applyAllowed);
        connect(applyButton, &QToolButton::clicked, this,
                [this, presetId = preset.id] { applyPreset(presetId); });
        presetTable_->setCellWidget(row, kApplyOnceColumn, applyButton);
        installCellContextMenu(applyButton);

        auto *periodicCheckBox = new QCheckBox(presetTable_);
        periodicCheckBox->setChecked(preset.periodicApplyEnabled);
        // A disabled preset must not be applied, but its periodic setting can
        // still be prepared before the preset is enabled again.
        periodicCheckBox->setEnabled(!orderConflict);
        periodicCheckBox->setToolTip(text(QStringLiteral("設定した間隔で自動的に適用します"),
                                          QStringLiteral("Apply automatically at the configured interval.")));
        connect(periodicCheckBox, &QCheckBox::toggled, this,
                [this, presetId = preset.id](bool checked) {
                    const int index = presetIndexById(presetId);
                    if (index < 0) {
                        return;
                    }
                    presets_[index].periodicApplyEnabled = checked;
                    savePresets();
                    updatePeriodicTimer();
                });
        presetTable_->setCellWidget(row, kApplyPeriodicColumn, periodicCheckBox);
        installCellContextMenu(periodicCheckBox);

        auto *resetButton = new QToolButton(presetTable_);
        resetButton->setText(text(QStringLiteral("リセット"), QStringLiteral("Reset")));
        resetButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        resetButton->setToolTip(text(QStringLiteral("対象プロセスのCPUアフィニティを全CPUに戻します"),
                                     QStringLiteral("Reset the target process affinity to all CPUs.")));
        connect(resetButton, &QToolButton::clicked, this,
                [this, presetId = preset.id] { resetPreset(presetId); });
        presetTable_->setCellWidget(row, kResetColumn, resetButton);
        installCellContextMenu(resetButton);

        auto *orderCombo = new QComboBox(presetTable_);
        for (int order = 1; order <= matchingPresetCount; ++order) {
            orderCombo->addItem(QString::number(order), order);
        }
        orderCombo->addItem(text(QStringLiteral("全て"), QStringLiteral("All")), -1);
        orderCombo->setCurrentIndex(orderCombo->findData(
            preset.applyToAll ? -1 : preset.order));
        orderCombo->setToolTip(text(
            QStringLiteral("保持されている順番番号で適用対象を選択"),
            QStringLiteral("Choose the target using its retained order number.")));
        connect(orderCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this, presetId = preset.id, orderCombo](int) {
                    const int index = presetIndexById(presetId);
                    if (index < 0) {
                        return;
                    }
                    const int value = orderCombo->currentData().toInt();
                    presets_[index].applyToAll = value == -1;
                    if (value > 0) {
                        presets_[index].order = value;
                    }
                    savePresets();
                    rebuildPresetTable();
                    presetTable_->selectRow(index);
                    presetTable_->setCurrentCell(index, kExecutableColumn);
                });
        presetTable_->setCellWidget(row, kOrderColumn, orderCombo);
        installCellContextMenu(orderCombo);

        auto *processPriorityCombo = new QComboBox(presetTable_);
        for (const ProcessPriorityClass priority : {ProcessPriorityClass::Realtime,
                                                    ProcessPriorityClass::High,
                                                    ProcessPriorityClass::AboveNormal,
                                                    ProcessPriorityClass::Normal,
                                                    ProcessPriorityClass::BelowNormal,
                                                    ProcessPriorityClass::Idle}) {
            processPriorityCombo->addItem(processPriorityText(uiSettings_, priority),
                                          static_cast<int>(priority));
        }
        processPriorityCombo->setCurrentIndex(
            processPriorityCombo->findData(static_cast<int>(preset.processPriority)));
        processPriorityCombo->setToolTip(text(
            QStringLiteral("タスクマネージャーのプロセス優先度を選択"),
            QStringLiteral("Choose the Task Manager process priority.")));
        connect(processPriorityCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this, presetId = preset.id, processPriorityCombo](int) {
                    const int index = presetIndexById(presetId);
                    if (index < 0) {
                        return;
                    }
                    presets_[index].processPriority = static_cast<ProcessPriorityClass>(
                        processPriorityCombo->currentData().toInt());
                    savePresets();
                });
        presetTable_->setCellWidget(row, kProcessPriorityColumn, processPriorityCombo);
        installCellContextMenu(processPriorityCombo);

        const QVector<quint32> ownedProcessIds = processIdsOwnedByPreset(preset.id);
        QStringList ownedProcessIdTexts;
        for (const quint32 processId : ownedProcessIds) {
            ownedProcessIdTexts.append(QString::number(processId));
        }
        const QString ownedProcessIdText = ownedProcessIdTexts.isEmpty()
            ? QStringLiteral("—")
            : ownedProcessIdTexts.join(QStringLiteral(", "));
        auto *pidItem = new QTableWidgetItem(ownedProcessIdText);
        pidItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        pidItem->setTextAlignment(Qt::AlignCenter);
        if (ownedProcessIds.isEmpty()) {
            pidItem->setToolTip(text(
                QStringLiteral("この行が実際に担当中のPIDはありません。"),
                QStringLiteral("This row does not currently own any PID.")));
        } else {
            pidItem->setToolTip(text(
                QStringLiteral("この行が最後に適用成功し、現在担当中のPID: %1"),
                QStringLiteral("PIDs last successfully applied by this row: %1"))
                                    .arg(ownedProcessIdTexts.join(QStringLiteral(", "))));
        }
        presetTable_->setItem(row, kPidColumn, pidItem);

        for (const int informationColumn : {
                 kProcessStartColumn, kProcessCpuColumn, kProcessMemoryColumn}) {
            auto *informationItem = new QTableWidgetItem;
            informationItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            informationItem->setTextAlignment(Qt::AlignCenter);
            presetTable_->setItem(row, informationColumn, informationItem);
        }

        auto *exeItem = new QTableWidgetItem(preset.executableName);
        exeItem->setToolTip(QStringLiteral("対象.exe名（例: renderer.exe）"));
        presetTable_->setItem(row, kExecutableColumn, exeItem);

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
            presetTable_->setItem(row, kFirstCpuColumn + column, cpuItem);
        }
    }

    rebuildingTable_ = false;
    applyGroupSpans();
    QSet<int> separatorRows;
    for (int row = 0; row + 1 < presets_.size(); ++row) {
        const QString currentGroupId = presets_.at(row).groupId;
        const QString nextGroupId = presets_.at(row + 1).groupId;
        const bool insideSameGroup = !currentGroupId.isEmpty()
            && currentGroupId == nextGroupId;
        if (!insideSameGroup) {
            separatorRows.insert(row);
        }
    }
    selectionOverlay_->setSeparatorRows(separatorRows);
    if (normalizedOrders) {
        savePresets();
    }
    if (assignmentsChanged) {
        saveProcessOrderAssignments();
    }
    if (ownershipsChanged) {
        saveProcessPresetOwnerships();
    }
    resetAllAssignmentsButton_->setEnabled(
        !processOrderAssignments_.isEmpty() || !processPresetOwnerships_.isEmpty());
    updateAssignedPidCells();
    updatePeriodicTimer();
    updatePerformanceTimer();
    updatePerformanceMetrics();
    updateMoveButtons();
    selectionOverlay_->refresh();
    cpuUsageRow_->refresh();
}

void MainWindow::savePresets()
{
    repository_.saveProcessPresets(presets_);
}

void MainWindow::saveProcessOrderAssignments()
{
    repository_.saveProcessOrderAssignments(processOrderAssignments_);
}

void MainWindow::saveProcessPresetOwnerships()
{
    repository_.saveProcessPresetOwnerships(processPresetOwnerships_);
}

int MainWindow::matchingPresetCountForExecutable(const QString &executableName) const
{
    const QString key = executableKey(executableName);
    const int count = static_cast<int>(std::count_if(
        presets_.cbegin(), presets_.cend(), [&key](const ProcessPreset &preset) {
            return executableKey(preset.executableName) == key;
        }));
    return qMax(1, count);
}

quint32 MainWindow::assignedProcessId(const QString &executableName, int order) const
{
    const QString key = executableKey(executableName);
    const auto assignment = std::find_if(
        processOrderAssignments_.cbegin(), processOrderAssignments_.cend(),
        [&key, order](const ProcessOrderAssignment &candidate) {
            return candidate.order == order
                && executableKey(candidate.executableName) == key;
        });
    return assignment == processOrderAssignments_.cend() ? 0 : assignment->processId;
}

QVector<quint32> MainWindow::processIdsOwnedByPreset(const QString &presetId) const
{
    QSet<quint32> processIds;
    for (const ProcessPresetOwnership &ownership : processPresetOwnerships_) {
        if (ownership.presetId == presetId && ownership.processId != 0) {
            processIds.insert(ownership.processId);
        }
    }

    QVector<quint32> result;
    result.reserve(processIds.size());
    for (const quint32 processId : processIds) {
        result.append(processId);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool MainWindow::recordProcessPresetOwnership(const QString &executableName,
                                              quint32 processId,
                                              const QString &presetId)
{
    const QString key = executableKey(executableName);
    if (key.isEmpty() || processId == 0 || presetId.isEmpty()) {
        return false;
    }

    int matchingOwnerships = 0;
    bool alreadyOwnedByPreset = false;
    for (const ProcessPresetOwnership &ownership : processPresetOwnerships_) {
        if (ownership.processId == processId
            && executableKey(ownership.executableName) == key) {
            ++matchingOwnerships;
            alreadyOwnedByPreset = ownership.presetId == presetId;
        }
    }
    if (matchingOwnerships == 1 && alreadyOwnedByPreset) {
        return false;
    }

    for (int index = processPresetOwnerships_.size() - 1; index >= 0; --index) {
        const ProcessPresetOwnership &ownership = processPresetOwnerships_.at(index);
        if (ownership.processId == processId
            && executableKey(ownership.executableName) == key) {
            processPresetOwnerships_.removeAt(index);
        }
    }

    ProcessPresetOwnership ownership;
    ownership.executableName = QFileInfo(executableName).fileName();
    ownership.processId = processId;
    ownership.presetId = presetId;
    processPresetOwnerships_.append(ownership);
    return true;
}

bool MainWindow::clearProcessPresetOwnership(const QString &executableName, quint32 processId)
{
    const QString key = executableKey(executableName);
    bool changed = false;
    for (int index = processPresetOwnerships_.size() - 1; index >= 0; --index) {
        const ProcessPresetOwnership &ownership = processPresetOwnerships_.at(index);
        if (ownership.processId == processId
            && executableKey(ownership.executableName) == key) {
            processPresetOwnerships_.removeAt(index);
            changed = true;
        }
    }
    return changed;
}

bool MainWindow::reconcileProcessOrderAssignments(
    const QString &executableName,
    const QVector<ProcessInfo> &processes,
    int maximumOrder,
    int orderToAssign)
{
    const QString key = executableKey(executableName);
    if (key.isEmpty()) {
        return false;
    }

    QVector<ProcessInfo> sortedProcesses = processes;
    std::sort(sortedProcesses.begin(), sortedProcesses.end(),
              [](const ProcessInfo &left, const ProcessInfo &right) {
                  return left.processId < right.processId;
              });

    QSet<quint32> liveProcessIds;
    for (const ProcessInfo &process : sortedProcesses) {
        if (process.processId != 0) {
            liveProcessIds.insert(process.processId);
        }
    }

    QSet<int> usedOrders;
    QSet<quint32> usedProcessIds;
    QVector<ProcessOrderAssignment> reconciled;
    reconciled.reserve(processOrderAssignments_.size() + 1);
    bool changed = false;

    for (const ProcessOrderAssignment &assignment : processOrderAssignments_) {
        if (executableKey(assignment.executableName) != key) {
            reconciled.append(assignment);
            continue;
        }

        const bool valid = assignment.order >= 1
            && assignment.order <= maximumOrder
            && liveProcessIds.contains(assignment.processId)
            && !usedOrders.contains(assignment.order)
            && !usedProcessIds.contains(assignment.processId);
        if (!valid) {
            changed = true;
            continue;
        }
        usedOrders.insert(assignment.order);
        usedProcessIds.insert(assignment.processId);
        reconciled.append(assignment);
    }

    processOrderAssignments_ = reconciled;

    if (orderToAssign >= 1 && orderToAssign <= maximumOrder
        && !usedOrders.contains(orderToAssign)) {
        QVector<int> unassignedOrders;
        for (int order = 1; order <= maximumOrder; ++order) {
            if (!usedOrders.contains(order)) {
                unassignedOrders.append(order);
            }
        }

        QVector<ProcessInfo> unassignedProcesses;
        for (const ProcessInfo &process : sortedProcesses) {
            if (process.processId != 0 && !usedProcessIds.contains(process.processId)) {
                unassignedProcesses.append(process);
            }
        }

        const int assignmentIndex = unassignedOrders.indexOf(orderToAssign);
        if (assignmentIndex >= 0 && assignmentIndex < unassignedProcesses.size()) {
            ProcessOrderAssignment assignment;
            assignment.executableName = QFileInfo(executableName).fileName();
            assignment.order = orderToAssign;
            assignment.processId = unassignedProcesses.at(assignmentIndex).processId;
            processOrderAssignments_.append(assignment);
            changed = true;
        }
    }

    return changed;
}

bool MainWindow::pruneProcessOrderAssignments()
{
    QMap<QString, QString> executableNames;
    QMap<QString, int> maximumOrders;
    for (const ProcessPreset &preset : presets_) {
        const QString key = executableKey(preset.executableName);
        if (key.isEmpty()) {
            continue;
        }
        if (!executableNames.contains(key)) {
            executableNames.insert(key, QFileInfo(preset.executableName).fileName());
        }
        ++maximumOrders[key];
    }

    bool changed = false;
    QVector<ProcessOrderAssignment> retained;
    retained.reserve(processOrderAssignments_.size());
    for (const ProcessOrderAssignment &assignment : processOrderAssignments_) {
        if (executableNames.contains(executableKey(assignment.executableName))) {
            retained.append(assignment);
        } else {
            changed = true;
        }
    }
    processOrderAssignments_ = retained;

    for (auto iterator = executableNames.cbegin(); iterator != executableNames.cend(); ++iterator) {
        const QVector<ProcessInfo> processes = findProcessesByExecutable(iterator.value());
        changed = reconcileProcessOrderAssignments(
                      iterator.value(), processes, maximumOrders.value(iterator.key()), 0)
            || changed;
    }
    return changed;
}

bool MainWindow::pruneProcessPresetOwnerships()
{
    QSet<QString> presetIds;
    for (const ProcessPreset &preset : presets_) {
        if (!preset.id.isEmpty()) {
            presetIds.insert(preset.id);
        }
    }

    QMap<QString, QString> executableNames;
    for (const ProcessPresetOwnership &ownership : processPresetOwnerships_) {
        const QString key = executableKey(ownership.executableName);
        if (!key.isEmpty() && !executableNames.contains(key)) {
            executableNames.insert(key, QFileInfo(ownership.executableName).fileName());
        }
    }

    QMap<QString, QSet<quint32>> liveProcessIds;
    for (auto iterator = executableNames.cbegin(); iterator != executableNames.cend(); ++iterator) {
        QSet<quint32> processIds;
        for (const ProcessInfo &process : findProcessesByExecutable(iterator.value())) {
            if (process.processId != 0) {
                processIds.insert(process.processId);
            }
        }
        liveProcessIds.insert(iterator.key(), processIds);
    }

    QSet<QString> retainedTargets;
    QVector<ProcessPresetOwnership> retained;
    retained.reserve(processPresetOwnerships_.size());
    bool changed = false;
    for (auto iterator = processPresetOwnerships_.crbegin();
         iterator != processPresetOwnerships_.crend(); ++iterator) {
        const QString key = executableKey(iterator->executableName);
        const QString targetKey = key + QChar(0x1f) + QString::number(iterator->processId);
        const bool valid = presetIds.contains(iterator->presetId)
            && liveProcessIds.value(key).contains(iterator->processId)
            && !retainedTargets.contains(targetKey);
        if (!valid) {
            changed = true;
            continue;
        }
        retainedTargets.insert(targetKey);
        retained.append(*iterator);
    }
    std::reverse(retained.begin(), retained.end());
    processPresetOwnerships_ = retained;
    return changed;
}

void MainWindow::updateAssignedPidCells()
{
    if (presetTable_ == nullptr || presetTable_->columnCount() <= kProcessMemoryColumn) {
        return;
    }

    const QSignalBlocker blocker(presetTable_);
    const auto informationItem = [this](int row, int column) {
        QTableWidgetItem *item = presetTable_->item(row, column);
        if (item == nullptr) {
            item = new QTableWidgetItem;
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setTextAlignment(Qt::AlignCenter);
            presetTable_->setItem(row, column, item);
        }
        return item;
    };

    for (int row = 0; row < presets_.size() && row < presetTable_->rowCount(); ++row) {
        const ProcessPreset &preset = presets_.at(row);
        const QVector<quint32> ownedProcessIds = processIdsOwnedByPreset(preset.id);
        QStringList ownedProcessIdTexts;
        for (const quint32 processId : ownedProcessIds) {
            ownedProcessIdTexts.append(QString::number(processId));
        }
        QTableWidgetItem *item = presetTable_->item(row, kPidColumn);
        if (item == nullptr) {
            item = new QTableWidgetItem;
            presetTable_->setItem(row, kPidColumn, item);
        }
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setTextAlignment(Qt::AlignCenter);
        item->setText(ownedProcessIdTexts.isEmpty()
                          ? QStringLiteral("—")
                          : ownedProcessIdTexts.join(QStringLiteral(", ")));
        if (ownedProcessIds.isEmpty()) {
            item->setToolTip(text(
                QStringLiteral("この行が実際に担当中のPIDはありません。"),
                QStringLiteral("This row does not currently own any PID.")));
        } else {
            item->setToolTip(text(
                QStringLiteral("この行が最後に適用成功し、現在担当中のPID: %1"),
                QStringLiteral("PIDs last successfully applied by this row: %1"))
                                     .arg(ownedProcessIdTexts.join(QStringLiteral(", "))));
        }

        QTableWidgetItem *startItem = informationItem(row, kProcessStartColumn);
        QTableWidgetItem *cpuItem = informationItem(row, kProcessCpuColumn);
        QTableWidgetItem *memoryItem = informationItem(row, kProcessMemoryColumn);
        if (ownedProcessIds.size() != 1) {
            for (QTableWidgetItem *information : {startItem, cpuItem, memoryItem}) {
                information->setText(QString());
                information->setToolTip(QString());
                information->setData(Qt::UserRole, QVariant());
            }
            continue;
        }

        const quint32 processId = ownedProcessIds.first();
        const ProcessPerformanceSnapshot snapshot = queryProcessPerformance(processId);
        startItem->setData(Qt::UserRole, processId);
        startItem->setText(snapshot.timingAvailable
                               ? processStartTimeText(snapshot.startTime, uiSettings_)
                               : QStringLiteral("—"));
        startItem->setToolTip(text(
            QStringLiteral("PID %1 のプロセス開始時間").arg(processId),
            QStringLiteral("Process start time for PID %1").arg(processId)));

        for (QTableWidgetItem *performanceItem : {cpuItem, memoryItem}) {
            if (performanceItem->data(Qt::UserRole).toUInt() != processId) {
                performanceItem->setText(QString());
                performanceItem->setToolTip(QString());
            }
            performanceItem->setData(Qt::UserRole, processId);
        }
    }
    if (resetAllAssignmentsButton_ != nullptr) {
        resetAllAssignmentsButton_->setEnabled(
            !processOrderAssignments_.isEmpty() || !processPresetOwnerships_.isEmpty());
    }
}

void MainWindow::resetAllProcessOrderAssignments()
{
    if (processOrderAssignments_.isEmpty() && processPresetOwnerships_.isEmpty()) {
        return;
    }
    processOrderAssignments_.clear();
    processPresetOwnerships_.clear();
    saveProcessOrderAssignments();
    saveProcessPresetOwnerships();
    updateAssignedPidCells();
    statusBar()->showMessage(
        text(QStringLiteral("全ての担当PIDをクリアしました。"),
             QStringLiteral("All assigned PIDs were cleared.")),
        4000);
}

QVector<ProcessInfo> MainWindow::targetProcessesForPreset(
    const ProcessPreset &preset,
    const QString &executableName,
    QString *errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    const auto setError = [errorMessage](const QString &message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
    };

    if (pruneProcessPresetOwnerships()) {
        saveProcessPresetOwnerships();
        updateAssignedPidCells();
    }

    QVector<ProcessInfo> processes = findProcessesByExecutable(executableName);
    std::sort(processes.begin(), processes.end(),
              [](const ProcessInfo &left, const ProcessInfo &right) {
                  return left.processId < right.processId;
              });

    const int maximumOrder = matchingPresetCountForExecutable(executableName);
    const int orderToAssign = preset.applyToAll ? 0 : preset.order;
    const bool assignmentsChanged = reconcileProcessOrderAssignments(
        executableName, processes, maximumOrder, orderToAssign);
    if (assignmentsChanged) {
        saveProcessOrderAssignments();
        updateAssignedPidCells();
    }

    if (processes.isEmpty()) {
        setError(text(QStringLiteral("%1 の実行中プロセスが見つかりません。"),
                      QStringLiteral("No running process was found for %1."))
                     .arg(executableName));
        return {};
    }
    if (preset.applyToAll) {
        return processes;
    }

    const quint32 processId = assignedProcessId(executableName, preset.order);
    const auto process = std::find_if(
        processes.cbegin(), processes.cend(), [processId](const ProcessInfo &candidate) {
            return candidate.processId == processId;
        });
    if (processId == 0 || process == processes.cend()) {
        setError(text(
            QStringLiteral("%1 の順番 %2 に担当PIDを割り当てられないため、適用しませんでした。"),
            QStringLiteral("Order %2 for %1 has no PID available, so it was not applied."))
                         .arg(executableName)
                         .arg(preset.order));
        return {};
    }
    return {*process};
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
    const QVector<int> rows = selectedPresetRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("プリセットを削除"),
                                 QStringLiteral("削除するプリセットの行を選択してください。"));
        return;
    }

    deletePresetRows(rows);
}

void MainWindow::deletePresetRows(const QVector<int> &rows)
{
    QVector<int> validRows;
    for (const int row : rows) {
        if (row >= 0 && row < presets_.size() && !validRows.contains(row)) {
            validRows.append(row);
        }
    }
    std::sort(validRows.begin(), validRows.end());
    if (validRows.isEmpty()) {
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("プリセットを削除"),
        validRows.size() == 1
            ? QStringLiteral("「%1」を削除しますか？").arg(presets_.at(validRows.first()).executableName)
            : QStringLiteral("選択した %1 件のプリセットを削除しますか？").arg(validRows.size()));
    if (answer != QMessageBox::Yes) {
        return;
    }

    for (auto iterator = validRows.crbegin(); iterator != validRows.crend(); ++iterator) {
        presets_.removeAt(*iterator);
    }
    normalizeGroups();
    savePresets();
    rebuildPresetTable();
    statusBar()->showMessage(QStringLiteral("プリセットを削除して保存しました。"), 4000);
}

void MainWindow::duplicatePreset(int row)
{
    if (row < 0 || row >= presets_.size()) {
        return;
    }
    ProcessPreset copy = presets_.at(row);
    copy.id = makeId();
    int insertionRow = row + 1;
    if (!copy.groupId.isEmpty()) {
        insertionRow = groupedRowsForId(copy.groupId).last() + 1;
    }
    copy.groupId.clear();
    presets_.insert(insertionRow, copy);
    normalizeGroups();
    savePresets();
    rebuildPresetTable();
    presetTable_->selectRow(insertionRow);
    presetTable_->setCurrentCell(insertionRow, kExecutableColumn);
    statusBar()->showMessage(text(QStringLiteral("プリセットを複製して保存しました。"),
                                  QStringLiteral("Preset duplicated and saved.")), 4000);
}

void MainWindow::showPresetContextMenu(const QPoint &position)
{
    const QModelIndex clickedIndex = presetTable_->indexAt(position);
    if (!clickedIndex.isValid()) {
        return;
    }
    const int clickedRow = clickedIndex.row();
    QVector<int> rows = selectedPresetRows();
    if (!rows.contains(clickedRow)) {
        presetTable_->clearSelection();
        presetTable_->selectRow(clickedRow);
        presetTable_->setCurrentCell(clickedRow, kExecutableColumn);
        rows = {clickedRow};
    }

    QMenu menu(this);
    QAction *duplicateAction = menu.addAction(text(QStringLiteral("複製"), QStringLiteral("Duplicate")));
    QAction *deleteAction = menu.addAction(text(QStringLiteral("削除"), QStringLiteral("Delete")));
    QAction *groupAction = nullptr;
    QAction *ungroupAction = nullptr;
    if (rows.size() >= 2) {
        menu.addSeparator();
        groupAction = menu.addAction(text(QStringLiteral("グループ化"), QStringLiteral("Group")));
    }
    const bool hasGroupedRow = std::any_of(rows.cbegin(), rows.cend(), [this](int row) {
        return row >= 0 && row < presets_.size() && !presets_.at(row).groupId.isEmpty();
    });
    if (hasGroupedRow) {
        if (groupAction == nullptr) {
            menu.addSeparator();
        }
        ungroupAction = menu.addAction(text(QStringLiteral("グループを解除"), QStringLiteral("Ungroup")));
    }

    QAction *chosenAction = menu.exec(presetTable_->viewport()->mapToGlobal(position));
    if (chosenAction == duplicateAction) {
        duplicatePreset(clickedRow);
    } else if (chosenAction == deleteAction) {
        deletePresetRows(rows);
    } else if (groupAction != nullptr && chosenAction == groupAction) {
        groupSelectedPresets();
    } else if (ungroupAction != nullptr && chosenAction == ungroupAction) {
        ungroupSelectedPresets();
    }
}

void MainWindow::groupSelectedPresets()
{
    QSet<int> rowSet;
    for (const int row : selectedPresetRows()) {
        rowSet.insert(row);
        const QString groupId = presets_.at(row).groupId;
        if (!groupId.isEmpty()) {
            for (const int groupedRow : groupedRowsForId(groupId)) {
                rowSet.insert(groupedRow);
            }
        }
    }
    QVector<int> rows;
    rows.reserve(rowSet.size());
    for (const int row : rowSet) {
        rows.append(row);
    }
    std::sort(rows.begin(), rows.end());
    if (rows.size() < 2) {
        return;
    }

    const int insertionRow = rows.first();
    const bool groupEnabled = presets_.at(insertionRow).enabled;
    QVector<ProcessPreset> groupedPresets;
    groupedPresets.reserve(rows.size());
    for (const int row : rows) {
        groupedPresets.append(presets_.at(row));
    }
    for (auto iterator = rows.crbegin(); iterator != rows.crend(); ++iterator) {
        presets_.removeAt(*iterator);
    }
    const QString groupId = makeId();
    for (int index = 0; index < groupedPresets.size(); ++index) {
        ProcessPreset preset = groupedPresets.at(index);
        preset.groupId = groupId;
        preset.enabled = groupEnabled;
        presets_.insert(insertionRow + index, preset);
    }

    normalizeGroups();
    savePresets();
    rebuildPresetTable();
    presetTable_->clearSelection();
    for (int row = insertionRow; row < insertionRow + groupedPresets.size(); ++row) {
        presetTable_->selectionModel()->select(
            presetTable_->model()->index(row, kExecutableColumn),
            QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    presetTable_->setCurrentCell(insertionRow, kExecutableColumn);
    statusBar()->showMessage(text(QStringLiteral("選択したプリセットをグループ化しました。"),
                                  QStringLiteral("Selected presets were grouped.")), 4000);
}

void MainWindow::ungroupSelectedPresets()
{
    QSet<QString> groupIds;
    for (const int row : selectedPresetRows()) {
        const QString groupId = presets_.at(row).groupId;
        if (!groupId.isEmpty()) {
            groupIds.insert(groupId);
        }
    }
    if (groupIds.isEmpty()) {
        return;
    }
    for (ProcessPreset &preset : presets_) {
        if (groupIds.contains(preset.groupId)) {
            preset.groupId.clear();
        }
    }
    savePresets();
    rebuildPresetTable();
    statusBar()->showMessage(text(QStringLiteral("グループを解除しました。"),
                                  QStringLiteral("Group was removed.")), 4000);
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
    const auto applySettings = [this](const UiSettings &settings) {
        uiSettings_ = settings;
        repository_.saveUiSettings(uiSettings_);
        applyUiSettings();
        retranslateUi();
        rebuildPresetTable();
        statusBar()->showMessage(
            text(QStringLiteral("設定を適用しました。"),
                 QStringLiteral("Settings applied.")),
            4000);
    };
    SettingsDialog dialog(uiSettings_,
                          [this] { resetCustomCpuStructures(); },
                          applySettings,
                          this);
    const int dialogResult = dialog.exec();
    uiSettings_.settingsDialogSize = dialog.size();
    repository_.saveUiSettings(uiSettings_);
    if (dialogResult != QDialog::Accepted) {
        return;
    }
    applySettings(dialog.settings());
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
    bool rebuild = false;
    if (column == kEnabledColumn) {
        const bool enabled = item->checkState() == Qt::Checked;
        if (preset.groupId.isEmpty()) {
            preset.enabled = enabled;
        } else {
            for (ProcessPreset &groupedPreset : presets_) {
                if (groupedPreset.groupId == preset.groupId) {
                    groupedPreset.enabled = enabled;
                }
            }
        }
        rebuild = true;
    } else if (column == kExecutableColumn) {
        preset.executableName = QFileInfo(item->text().trimmed()).fileName();
        rebuild = true;
    } else if (column >= kFirstCpuColumn && column - kFirstCpuColumn < activeLeaves_.size()) {
        const CpuLeaf &leaf = activeLeaves_.at(column - kFirstCpuColumn);
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
    if (rebuild) {
        rebuildPresetTable();
        presetTable_->selectRow(row);
        presetTable_->setCurrentCell(row, kExecutableColumn);
    }
}

bool MainWindow::executableHasOrderConflict(const QString &executableName) const
{
    const QString key = executableKey(executableName);
    if (key.isEmpty()) {
        return false;
    }

    int allCount = 0;
    QMap<int, int> orderCounts;
    for (const ProcessPreset &preset : presets_) {
        if (!preset.enabled || executableKey(preset.executableName) != key) {
            continue;
        }
        if (preset.applyToAll) {
            ++allCount;
        } else {
            ++orderCounts[preset.order];
        }
    }
    if (allCount > 1 || (allCount > 0 && !orderCounts.isEmpty())) {
        return true;
    }
    return std::any_of(orderCounts.cbegin(), orderCounts.cend(),
                       [](int count) { return count > 1; });
}

bool MainWindow::isApplyAllowedFor(const ProcessPreset &preset) const
{
    return preset.enabled && !executableHasOrderConflict(preset.executableName);
}

QVector<int> MainWindow::selectedPresetRows() const
{
    QSet<int> rowSet;
    if (presetTable_ != nullptr && presetTable_->selectionModel() != nullptr) {
        for (const QModelIndex &index : presetTable_->selectionModel()->selectedRows()) {
            if (index.row() >= 0 && index.row() < presets_.size()) {
                rowSet.insert(index.row());
            }
        }
    }
    if (rowSet.isEmpty() && presetTable_ != nullptr) {
        const int row = presetTable_->currentRow();
        if (row >= 0 && row < presets_.size()) {
            rowSet.insert(row);
        }
    }
    QVector<int> result;
    result.reserve(rowSet.size());
    for (const int row : rowSet) {
        result.append(row);
    }
    std::sort(result.begin(), result.end());
    return result;
}

QVector<int> MainWindow::groupedRowsForId(const QString &groupId) const
{
    QVector<int> rows;
    if (groupId.isEmpty()) {
        return rows;
    }
    for (int row = 0; row < presets_.size(); ++row) {
        if (presets_.at(row).groupId == groupId) {
            rows.append(row);
        }
    }
    return rows;
}

void MainWindow::normalizeGroups()
{
    QMap<QString, QVector<int>> rowsByGroup;
    for (int row = 0; row < presets_.size(); ++row) {
        if (!presets_.at(row).groupId.isEmpty()) {
            rowsByGroup[presets_.at(row).groupId].append(row);
        }
    }
    for (auto iterator = rowsByGroup.cbegin(); iterator != rowsByGroup.cend(); ++iterator) {
        const QVector<int> rows = iterator.value();
        const bool contiguous = std::adjacent_find(
                                    rows.cbegin(), rows.cend(), [](int left, int right) {
                                        return right != left + 1;
                                    }) == rows.cend();
        if (rows.size() < 2 || !contiguous) {
            for (const int row : rows) {
                presets_[row].groupId.clear();
            }
            continue;
        }
        const bool enabled = presets_.at(rows.first()).enabled;
        for (const int row : rows) {
            presets_[row].enabled = enabled;
        }
    }
}

void MainWindow::applyGroupSpans()
{
    presetTable_->clearSpans();
    int row = 0;
    while (row < presets_.size()) {
        const QString groupId = presets_.at(row).groupId;
        if (groupId.isEmpty()) {
            ++row;
            continue;
        }
        int end = row + 1;
        while (end < presets_.size() && presets_.at(end).groupId == groupId) {
            ++end;
        }
        if (end - row >= 2) {
            presetTable_->setSpan(row, kEnabledColumn, end - row, 1);
        }
        row = end;
    }
}

void MainWindow::moveSelectedPreset(int offset)
{
    const int row = presetTable_->currentRow();
    if (row < 0 || row >= presets_.size()) {
        return;
    }
    const QString groupId = presets_.at(row).groupId;
    int destination = row + offset;
    if (!groupId.isEmpty()) {
        const QVector<int> groupedRows = groupedRowsForId(groupId);
        const int first = groupedRows.first();
        const int last = groupedRows.last();
        if (offset < 0) {
            if (first == 0) {
                return;
            }
            ProcessPreset previous = presets_.takeAt(first - 1);
            presets_.insert(last, previous);
            destination = first - 1;
        } else {
            if (last >= presets_.size() - 1) {
                return;
            }
            ProcessPreset next = presets_.takeAt(last + 1);
            presets_.insert(first, next);
            destination = last + 1;
        }
    } else {
        if (destination < 0 || destination >= presets_.size()) {
            return;
        }
        presets_.swapItemsAt(row, destination);
    }
    savePresets();
    rebuildPresetTable();
    presetTable_->selectRow(destination);
    presetTable_->setCurrentCell(destination, kExecutableColumn);
    statusBar()->showMessage(text(QStringLiteral("プリセットの並び順を保存しました。"),
                                  QStringLiteral("Preset order saved.")), 2500);
}

void MainWindow::updateMoveButtons()
{
    if (moveUpButton_ == nullptr || moveDownButton_ == nullptr || presetTable_ == nullptr) {
        return;
    }
    const int row = presetTable_->currentRow();
    if (row < 0 || row >= presets_.size()) {
        moveUpButton_->setEnabled(false);
        moveDownButton_->setEnabled(false);
        return;
    }
    const QString groupId = presets_.at(row).groupId;
    const QVector<int> groupRows = groupedRowsForId(groupId);
    const int first = groupRows.isEmpty() ? row : groupRows.first();
    const int last = groupRows.isEmpty() ? row : groupRows.last();
    moveUpButton_->setEnabled(first > 0);
    moveDownButton_->setEnabled(last < presets_.size() - 1);
}

void MainWindow::updatePeriodicTimer()
{
    if (periodicApplyTimer_ == nullptr) {
        return;
    }
    bool hasPeriodicPreset = false;
    for (const ProcessPreset &preset : presets_) {
        if (preset.periodicApplyEnabled && isApplyAllowedFor(preset)) {
            hasPeriodicPreset = true;
            break;
        }
    }
    if (!hasPeriodicPreset) {
        periodicApplyTimer_->stop();
        return;
    }
    periodicApplyTimer_->start(uiSettings_.periodicApplyIntervalSeconds * 1000);
}

void MainWindow::applyPeriodicPresets()
{
    for (const ProcessPreset &preset : presets_) {
        if (preset.periodicApplyEnabled && isApplyAllowedFor(preset)) {
            applyPreset(preset.id, true);
        }
    }
}

void MainWindow::updatePerformanceTimer()
{
    if (performanceTimer_ == nullptr) {
        return;
    }
    const bool needsUpdates = uiSettings_.showProcessCpuColumn
        || uiSettings_.showProcessMemoryColumn
        || uiSettings_.showCpuUsageRow;
    if (!needsUpdates) {
        performanceTimer_->stop();
    } else {
        performanceTimer_->start(uiSettings_.performanceUpdateIntervalSeconds * 1000);
    }

    if (!uiSettings_.showProcessCpuColumn) {
        processCpuSamples_.clear();
    }
    if (!uiSettings_.showCpuUsageRow) {
        processorCpuSamples_.clear();
        processorUsagePercentages_.clear();
        if (cpuUsageRow_ != nullptr) {
            cpuUsageRow_->setUsagePercentages({});
        }
    }
}

void MainWindow::updatePerformanceMetrics()
{
    if (presetTable_ == nullptr) {
        return;
    }

    QSet<quint32> singleOwnedProcessIds;
    for (const ProcessPreset &preset : presets_) {
        const QVector<quint32> ownedProcessIds = processIdsOwnedByPreset(preset.id);
        if (ownedProcessIds.size() == 1) {
            singleOwnedProcessIds.insert(ownedProcessIds.first());
        }
    }

    QHash<quint32, ProcessPerformanceSnapshot> processSnapshots;
    if (uiSettings_.showProcessCpuColumn || uiSettings_.showProcessMemoryColumn) {
        for (const quint32 processId : singleOwnedProcessIds) {
            processSnapshots.insert(processId, queryProcessPerformance(processId));
        }
    }

    QHash<quint32, double> processCpuPercentages;
    const qint64 timestampMilliseconds = performanceClock_.elapsed();
    const int processorCount = activeLogicalProcessorCount();
    if (uiSettings_.showProcessCpuColumn) {
        for (auto iterator = processSnapshots.cbegin(); iterator != processSnapshots.cend(); ++iterator) {
            const quint32 processId = iterator.key();
            const ProcessPerformanceSnapshot &snapshot = iterator.value();
            if (!snapshot.timingAvailable) {
                processCpuSamples_.remove(processId);
                continue;
            }

            const auto previous = processCpuSamples_.constFind(processId);
            if (previous != processCpuSamples_.cend()
                && timestampMilliseconds > previous->timestampMilliseconds
                && snapshot.processorTime100Nanoseconds
                       >= previous->processorTime100Nanoseconds) {
                const quint64 processorDelta = snapshot.processorTime100Nanoseconds
                    - previous->processorTime100Nanoseconds;
                const double availableTime =
                    static_cast<double>(timestampMilliseconds - previous->timestampMilliseconds)
                    * 10000.0 * processorCount;
                if (availableTime > 0.0) {
                    processCpuPercentages.insert(
                        processId,
                        qBound(0.0, processorDelta * 100.0 / availableTime, 100.0));
                }
            }
            processCpuSamples_.insert(
                processId,
                {snapshot.processorTime100Nanoseconds, timestampMilliseconds});
        }
    }

    for (auto iterator = processCpuSamples_.begin(); iterator != processCpuSamples_.end();) {
        if (!singleOwnedProcessIds.contains(iterator.key())) {
            iterator = processCpuSamples_.erase(iterator);
        } else {
            ++iterator;
        }
    }

    {
        const QSignalBlocker blocker(presetTable_);
        for (int row = 0; row < presets_.size() && row < presetTable_->rowCount(); ++row) {
            const QVector<quint32> ownedProcessIds = processIdsOwnedByPreset(presets_.at(row).id);
            QTableWidgetItem *cpuItem = presetTable_->item(row, kProcessCpuColumn);
            QTableWidgetItem *memoryItem = presetTable_->item(row, kProcessMemoryColumn);
            if (cpuItem == nullptr || memoryItem == nullptr) {
                continue;
            }
            if (ownedProcessIds.size() != 1) {
                cpuItem->setText(QString());
                memoryItem->setText(QString());
                continue;
            }

            const quint32 processId = ownedProcessIds.first();
            const ProcessPerformanceSnapshot snapshot = processSnapshots.value(processId);
            if (uiSettings_.showProcessCpuColumn) {
                cpuItem->setText(processCpuPercentages.contains(processId)
                                     ? QStringLiteral("%1 %")
                                           .arg(processCpuPercentages.value(processId), 0, 'f', 1)
                                     : QStringLiteral("—"));
                cpuItem->setToolTip(text(
                    QStringLiteral("PID %1 のCPU使用率").arg(processId),
                    QStringLiteral("CPU usage for PID %1").arg(processId)));
            }
            if (uiSettings_.showProcessMemoryColumn) {
                memoryItem->setText(snapshot.memoryAvailable
                                        ? memoryUsageText(snapshot.workingSetBytes)
                                        : QStringLiteral("—"));
                memoryItem->setToolTip(text(
                    QStringLiteral("PID %1 のワーキングセット").arg(processId),
                    QStringLiteral("Working set for PID %1").arg(processId)));
            }
        }
    }

    if (uiSettings_.showCpuUsageRow && cpuUsageRow_ != nullptr) {
        const QVector<ProcessorPerformanceSnapshot> processorSnapshots =
            queryProcessorPerformance();
        QSet<QString> currentProcessorKeys;
        for (const ProcessorPerformanceSnapshot &snapshot : processorSnapshots) {
            const QString key = cpuKey(snapshot.processorGroup, snapshot.logicalProcessor);
            currentProcessorKeys.insert(key);
            const auto previous = processorCpuSamples_.constFind(key);
            if (previous != processorCpuSamples_.cend()
                && snapshot.totalTime100Nanoseconds > previous->totalTime100Nanoseconds
                && snapshot.idleTime100Nanoseconds >= previous->idleTime100Nanoseconds) {
                const quint64 totalDelta = snapshot.totalTime100Nanoseconds
                    - previous->totalTime100Nanoseconds;
                const quint64 idleDelta = snapshot.idleTime100Nanoseconds
                    - previous->idleTime100Nanoseconds;
                const quint64 busyDelta = totalDelta > idleDelta ? totalDelta - idleDelta : 0;
                processorUsagePercentages_.insert(
                    key,
                    qBound(0.0, busyDelta * 100.0 / totalDelta, 100.0));
            } else {
                processorUsagePercentages_.remove(key);
            }
            processorCpuSamples_.insert(
                key,
                {snapshot.idleTime100Nanoseconds, snapshot.totalTime100Nanoseconds});
        }

        for (auto iterator = processorCpuSamples_.begin(); iterator != processorCpuSamples_.end();) {
            if (!currentProcessorKeys.contains(iterator.key())) {
                processorUsagePercentages_.remove(iterator.key());
                iterator = processorCpuSamples_.erase(iterator);
            } else {
                ++iterator;
            }
        }
        cpuUsageRow_->setUsagePercentages(processorUsagePercentages_);
    }
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

void MainWindow::applyPreset(const QString &presetId, bool periodic)
{
    const int presetIndex = presetIndexById(presetId);
    if (presetIndex < 0) {
        return;
    }
    const ProcessPreset &preset = presets_.at(presetIndex);
    if (!isApplyAllowedFor(preset)) {
        if (!periodic) {
            statusBar()->showMessage(
                text(QStringLiteral("このプリセットは無効、または同じ.exeの順番設定が競合しているため適用できません。"),
                     QStringLiteral("This preset is disabled or has a conflicting order setting.")),
                6000);
        }
        return;
    }
    const QString executableName = QFileInfo(preset.executableName.trimmed()).fileName();
    if (executableName.isEmpty()) {
        statusBar()->showMessage(text(QStringLiteral("対象.exe名を入力してください。"),
                                      QStringLiteral("Enter a target .exe name.")), 6000);
        return;
    }

    const QVector<CpuLeaf> selectedLeaves = selectedLeavesFor(preset);
    if (selectedLeaves.isEmpty()) {
        statusBar()->showMessage(
            text(QStringLiteral("適用するCPUスレッドを少なくとも1つ選択してください。"),
                 QStringLiteral("Select at least one CPU thread to apply.")), 6000);
        return;
    }

    QString errorMessage;
    const QVector<ProcessInfo> processes = targetProcessesForPreset(
        preset, executableName, &errorMessage);
    if (!errorMessage.isEmpty()) {
        statusBar()->showMessage(errorMessage, 8000);
        return;
    }

    QStringList messages;
    bool ownershipsChanged = false;
    for (const ProcessInfo &process : processes) {
        const AffinityApplyResult affinityResult = applyAffinity(
            process.processId, selectedLeaves);
        const AffinityApplyResult priorityResult = applyProcessPriority(
            process.processId, preset.processPriority);
        messages.append(affinityResult.message);
        messages.append(priorityResult.message);
        if (affinityResult.success) {
            ownershipsChanged = recordProcessPresetOwnership(
                                    executableName, process.processId, preset.id)
                || ownershipsChanged;
        }
    }
    if (ownershipsChanged) {
        saveProcessPresetOwnerships();
        updateAssignedPidCells();
    }
    statusBar()->showMessage(messages.join(QStringLiteral("  ")), 8000);
}

void MainWindow::resetPreset(const QString &presetId)
{
    const int presetIndex = presetIndexById(presetId);
    if (presetIndex < 0) {
        return;
    }
    const ProcessPreset &preset = presets_.at(presetIndex);
    const QString executableName = QFileInfo(preset.executableName.trimmed()).fileName();
    if (executableName.isEmpty()) {
        statusBar()->showMessage(text(QStringLiteral("対象.exe名を入力してください。"),
                                      QStringLiteral("Enter a target .exe name.")), 6000);
        return;
    }

    if (pruneProcessPresetOwnerships()) {
        saveProcessPresetOwnerships();
        updateAssignedPidCells();
    }

    QVector<ProcessPresetOwnership> ownedProcesses;
    for (const ProcessPresetOwnership &ownership : processPresetOwnerships_) {
        if (ownership.presetId == preset.id) {
            ownedProcesses.append(ownership);
        }
    }
    if (!ownedProcesses.isEmpty()) {
        QStringList messages;
        bool ownershipsChanged = false;
        for (const ProcessPresetOwnership &ownership : ownedProcesses) {
            const AffinityApplyResult resetResult = resetAffinity(ownership.processId);
            messages.append(resetResult.message);
            if (resetResult.success) {
                ownershipsChanged = clearProcessPresetOwnership(
                                        ownership.executableName, ownership.processId)
                    || ownershipsChanged;
            }
        }
        if (ownershipsChanged) {
            saveProcessPresetOwnerships();
            updateAssignedPidCells();
        }
        statusBar()->showMessage(messages.join(QStringLiteral("  ")), 8000);
        return;
    }

    QString errorMessage;
    const QVector<ProcessInfo> processes = targetProcessesForPreset(
        preset, executableName, &errorMessage);
    if (!errorMessage.isEmpty()) {
        statusBar()->showMessage(errorMessage, 8000);
        return;
    }
    QStringList messages;
    bool ownershipsChanged = false;
    for (const ProcessInfo &process : processes) {
        const AffinityApplyResult resetResult = resetAffinity(process.processId);
        messages.append(resetResult.message);
        if (resetResult.success) {
            ownershipsChanged = clearProcessPresetOwnership(
                                    executableName, process.processId)
                || ownershipsChanged;
        }
    }
    if (ownershipsChanged) {
        saveProcessPresetOwnerships();
        updateAssignedPidCells();
    }
    statusBar()->showMessage(messages.join(QStringLiteral("  ")), 8000);
}

} // namespace ProcessPresets
