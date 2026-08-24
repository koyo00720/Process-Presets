#include "settingsdialog.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ProcessPresets {

SettingsDialog::SettingsDialog(const UiSettings &settings,
                               std::function<void()> deleteCustomCpuStructures,
                               std::function<void(const UiSettings &)> applySettings,
                               QWidget *parent)
    : QDialog(parent)
    , initialSettings_(settings)
    , cpuUsageBarColor_(settings.cpuUsageBarColor)
    , deleteCustomCpuStructures_(std::move(deleteCustomCpuStructures))
    , applySettings_(std::move(applySettings))
{
    const auto text = [this](const QString &japanese, const QString &english) {
        return localizedText(initialSettings_, japanese, english);
    };

    setWindowTitle(text(QStringLiteral("設定"), QStringLiteral("Settings")));
    setModal(true);
    setMinimumSize(360, 300);
    resize(settings.settingsDialogSize);

    auto *layout = new QVBoxLayout(this);
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *content = new QWidget(scrollArea);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 12, 12, 12);

    auto *themeGroup = new QGroupBox(
        text(QStringLiteral("テーマ"), QStringLiteral("Theme")), content);
    auto *themeGroupLayout = new QVBoxLayout(themeGroup);
    systemThemeButton_ = new QRadioButton(
        text(QStringLiteral("システム設定に従う"), QStringLiteral("Use system setting")), themeGroup);
    lightThemeButton_ = new QRadioButton(text(QStringLiteral("ライト"), QStringLiteral("Light")), themeGroup);
    darkThemeButton_ = new QRadioButton(text(QStringLiteral("ダーク"), QStringLiteral("Dark")), themeGroup);
    themeGroupLayout->addWidget(systemThemeButton_);
    themeGroupLayout->addWidget(lightThemeButton_);
    themeGroupLayout->addWidget(darkThemeButton_);
    contentLayout->addWidget(themeGroup);

    auto *languageGroup = new QGroupBox(
        text(QStringLiteral("言語"), QStringLiteral("Language")), content);
    auto *languageGroupLayout = new QVBoxLayout(languageGroup);
    systemLanguageButton_ = new QRadioButton(
        text(QStringLiteral("システムの言語"), QStringLiteral("System language")), languageGroup);
    japaneseLanguageButton_ = new QRadioButton(QStringLiteral("日本語"), languageGroup);
    englishLanguageButton_ = new QRadioButton(QStringLiteral("English"), languageGroup);
    languageGroupLayout->addWidget(systemLanguageButton_);
    languageGroupLayout->addWidget(japaneseLanguageButton_);
    languageGroupLayout->addWidget(englishLanguageButton_);
    contentLayout->addWidget(languageGroup);

    auto *displayGroup = new QGroupBox(
        text(QStringLiteral("表示"), QStringLiteral("Display")), content);
    auto *displayLayout = new QFormLayout(displayGroup);
    cpuColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    enabledColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    executableColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    applyOnceColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    periodicColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    resetColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    orderColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    processPriorityColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    pidColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    processStartColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    processCpuColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    processMemoryColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    showProcessStartColumnCheckBox_ = new QCheckBox(displayGroup);
    showProcessCpuColumnCheckBox_ = new QCheckBox(displayGroup);
    showProcessMemoryColumnCheckBox_ = new QCheckBox(displayGroup);
    showCpuUsageRowCheckBox_ = new QCheckBox(displayGroup);
    tableFontPointSizeSpinBox_ = new QSpinBox(displayGroup);
    tableRowHeightSpinBox_ = new QSpinBox(displayGroup);
    cpuUsageRowHeightSpinBox_ = new QSpinBox(displayGroup);
    cpuUsageBarWidthSpinBox_ = new QSpinBox(displayGroup);
    cpuUsageBarColorButton_ = new QPushButton(displayGroup);
    cpuUsageBarTransparencySpinBox_ = new QSpinBox(displayGroup);
    separatorLineThicknessSpinBox_ = new QSpinBox(displayGroup);
    separatorLineTransparencySpinBox_ = new QSpinBox(displayGroup);
    performanceUpdateIntervalSpinBox_ = new QSpinBox(displayGroup);
    periodicApplyIntervalSpinBox_ = new QSpinBox(displayGroup);
    for (QSpinBox *spinBox : {enabledColumnWidthSpinBox_, executableColumnWidthSpinBox_,
                              applyOnceColumnWidthSpinBox_, periodicColumnWidthSpinBox_,
                              resetColumnWidthSpinBox_, orderColumnWidthSpinBox_,
                              processPriorityColumnWidthSpinBox_, pidColumnWidthSpinBox_,
                              processStartColumnWidthSpinBox_, processCpuColumnWidthSpinBox_,
                              processMemoryColumnWidthSpinBox_}) {
        spinBox->setRange(1, 600);
        spinBox->setSuffix(QStringLiteral(" px"));
    }
    cpuColumnWidthSpinBox_->setRange(1, 600);
    cpuColumnWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    cpuColumnWidthSpinBox_->setValue(settings.cpuColumnWidth);
    enabledColumnWidthSpinBox_->setValue(settings.enabledColumnWidth);
    executableColumnWidthSpinBox_->setValue(settings.executableColumnWidth);
    applyOnceColumnWidthSpinBox_->setValue(settings.applyOnceColumnWidth);
    periodicColumnWidthSpinBox_->setValue(settings.periodicColumnWidth);
    resetColumnWidthSpinBox_->setValue(settings.resetColumnWidth);
    orderColumnWidthSpinBox_->setValue(settings.orderColumnWidth);
    processPriorityColumnWidthSpinBox_->setValue(settings.processPriorityColumnWidth);
    pidColumnWidthSpinBox_->setValue(settings.pidColumnWidth);
    processStartColumnWidthSpinBox_->setValue(settings.processStartColumnWidth);
    processCpuColumnWidthSpinBox_->setValue(settings.processCpuColumnWidth);
    processMemoryColumnWidthSpinBox_->setValue(settings.processMemoryColumnWidth);
    showProcessStartColumnCheckBox_->setChecked(settings.showProcessStartColumn);
    showProcessCpuColumnCheckBox_->setChecked(settings.showProcessCpuColumn);
    showProcessMemoryColumnCheckBox_->setChecked(settings.showProcessMemoryColumn);
    showCpuUsageRowCheckBox_->setChecked(settings.showCpuUsageRow);
    tableFontPointSizeSpinBox_->setRange(6, 48);
    tableFontPointSizeSpinBox_->setSuffix(QStringLiteral(" pt"));
    tableFontPointSizeSpinBox_->setValue(settings.tableFontPointSize);
    tableRowHeightSpinBox_->setRange(1, 600);
    tableRowHeightSpinBox_->setSuffix(QStringLiteral(" px"));
    tableRowHeightSpinBox_->setValue(settings.tableRowHeight);
    cpuUsageRowHeightSpinBox_->setRange(20, 300);
    cpuUsageRowHeightSpinBox_->setSuffix(QStringLiteral(" px"));
    cpuUsageRowHeightSpinBox_->setValue(settings.cpuUsageRowHeight);
    cpuUsageBarWidthSpinBox_->setRange(1, 600);
    cpuUsageBarWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    cpuUsageBarWidthSpinBox_->setValue(settings.cpuUsageBarWidth);
    updateCpuUsageBarColorButton();
    connect(cpuUsageBarColorButton_, &QPushButton::clicked, this, [this, text] {
        QColor initialColor(cpuUsageBarColor_);
        if (!initialColor.isValid()) {
            initialColor = palette().color(QPalette::Text);
        }
        const QColor selectedColor = QColorDialog::getColor(
            initialColor,
            this,
            text(QStringLiteral("CPU使用率の棒の色"),
                 QStringLiteral("CPU usage bar color")));
        if (!selectedColor.isValid()) {
            return;
        }
        cpuUsageBarColor_ = selectedColor.name(QColor::HexRgb);
        updateCpuUsageBarColorButton();
    });
    cpuUsageBarTransparencySpinBox_->setRange(0, 100);
    cpuUsageBarTransparencySpinBox_->setSuffix(QStringLiteral(" %"));
    cpuUsageBarTransparencySpinBox_->setValue(settings.cpuUsageBarTransparencyPercent);
    cpuUsageBarTransparencySpinBox_->setToolTip(text(
        QStringLiteral("0%で不透明、100%で完全に透明になります。"),
        QStringLiteral("0% is opaque; 100% is fully transparent.")));
    separatorLineThicknessSpinBox_->setRange(1, 20);
    separatorLineThicknessSpinBox_->setSuffix(QStringLiteral(" px"));
    separatorLineThicknessSpinBox_->setValue(settings.separatorLineThickness);
    separatorLineTransparencySpinBox_->setRange(0, 100);
    separatorLineTransparencySpinBox_->setSuffix(QStringLiteral(" %"));
    separatorLineTransparencySpinBox_->setValue(settings.separatorLineTransparencyPercent);
    separatorLineTransparencySpinBox_->setToolTip(text(
        QStringLiteral("0%で不透明、100%で完全に透明になります。"),
        QStringLiteral("0% is opaque; 100% is fully transparent.")));
    performanceUpdateIntervalSpinBox_->setRange(1, 3600);
    performanceUpdateIntervalSpinBox_->setSuffix(QStringLiteral(" s"));
    performanceUpdateIntervalSpinBox_->setValue(settings.performanceUpdateIntervalSeconds);
    periodicApplyIntervalSpinBox_->setRange(1, 3600);
    periodicApplyIntervalSpinBox_->setSuffix(QStringLiteral(" s"));
    periodicApplyIntervalSpinBox_->setValue(settings.periodicApplyIntervalSeconds);
    displayLayout->addRow(
        text(QStringLiteral("有効列:"), QStringLiteral("Enabled column:")), enabledColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral(".exe列:"), QStringLiteral(".exe column:")), executableColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("適用・1回列:"), QStringLiteral("Apply / Once column:")),
        applyOnceColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("適用・定期列:"), QStringLiteral("Apply / Periodic column:")),
        periodicColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("リセット列:"), QStringLiteral("Reset column:")), resetColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("順番列:"), QStringLiteral("Order column:")), orderColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("優先度列:"), QStringLiteral("Process priority column:")),
        processPriorityColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("PID列:"), QStringLiteral("PID column:")),
        pidColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("開始列:"), QStringLiteral("Start column:")),
        processStartColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU列:"), QStringLiteral("CPU column:")),
        processCpuColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("メモリ列:"), QStringLiteral("Memory column:")),
        processMemoryColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU構造の最下層列:"), QStringLiteral("Lowest CPU hierarchy column:")),
        cpuColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("開始列を表示:"), QStringLiteral("Show Start column:")),
        showProcessStartColumnCheckBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU列を表示:"), QStringLiteral("Show CPU column:")),
        showProcessCpuColumnCheckBox_);
    displayLayout->addRow(
        text(QStringLiteral("メモリ列を表示:"), QStringLiteral("Show Memory column:")),
        showProcessMemoryColumnCheckBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU使用率行を表示:"), QStringLiteral("Show CPU usage row:")),
        showCpuUsageRowCheckBox_);
    displayLayout->addRow(
        text(QStringLiteral("表の文字サイズ:"), QStringLiteral("Table font size:")),
        tableFontPointSizeSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("表の行の高さ:"), QStringLiteral("Table row height:")),
        tableRowHeightSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU使用率行の高さ:"), QStringLiteral("CPU usage row height:")),
        cpuUsageRowHeightSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU使用率の棒の幅:"), QStringLiteral("CPU usage bar width:")),
        cpuUsageBarWidthSpinBox_);
    auto *cpuUsageBarColorControl = new QWidget(displayGroup);
    auto *cpuUsageBarColorLayout = new QHBoxLayout(cpuUsageBarColorControl);
    cpuUsageBarColorLayout->setContentsMargins(0, 0, 0, 0);
    cpuUsageBarColorLayout->addWidget(cpuUsageBarColorButton_, 1);
    auto *automaticCpuUsageBarColorButton = new QPushButton(
        text(QStringLiteral("自動"), QStringLiteral("Automatic")),
        cpuUsageBarColorControl);
    automaticCpuUsageBarColorButton->setToolTip(text(
        QStringLiteral("テーマの文字色を使用します。"),
        QStringLiteral("Use the theme's text color.")));
    connect(automaticCpuUsageBarColorButton, &QPushButton::clicked, this, [this] {
        cpuUsageBarColor_.clear();
        updateCpuUsageBarColorButton();
    });
    cpuUsageBarColorLayout->addWidget(automaticCpuUsageBarColorButton);
    displayLayout->addRow(
        text(QStringLiteral("CPU使用率の棒の色:"), QStringLiteral("CPU usage bar color:")),
        cpuUsageBarColorControl);
    displayLayout->addRow(
        text(QStringLiteral("CPU使用率の棒の透明度:"),
             QStringLiteral("CPU usage bar transparency:")),
        cpuUsageBarTransparencySpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("区切り線の太さ:"), QStringLiteral("Separator thickness:")),
        separatorLineThicknessSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("区切り線の透明度:"), QStringLiteral("Separator transparency:")),
        separatorLineTransparencySpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU・メモリ・使用率行の更新間隔:"),
             QStringLiteral("CPU / memory / usage row update interval:")),
        performanceUpdateIntervalSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("定期適用の間隔:"), QStringLiteral("Periodic apply interval:")),
        periodicApplyIntervalSpinBox_);
    contentLayout->addWidget(displayGroup);

    auto *resetGroup = new QGroupBox(
        text(QStringLiteral("リセット"), QStringLiteral("Reset")), content);
    auto *resetLayout = new QFormLayout(resetGroup);
    const UiSettings defaults;
    const auto confirmReset = [this, text](const QString &item) {
        return QMessageBox::question(
                   this,
                   text(QStringLiteral("リセットの確認"), QStringLiteral("Confirm reset")),
                   text(QStringLiteral("%1 を初期値に戻しますか？").arg(item),
                        QStringLiteral("Reset %1 to its default value?").arg(item)))
            == QMessageBox::Yes;
    };
    const auto resetDisplay = [this, defaults] {
        enabledColumnWidthSpinBox_->setValue(defaults.enabledColumnWidth);
        executableColumnWidthSpinBox_->setValue(defaults.executableColumnWidth);
        applyOnceColumnWidthSpinBox_->setValue(defaults.applyOnceColumnWidth);
        periodicColumnWidthSpinBox_->setValue(defaults.periodicColumnWidth);
        resetColumnWidthSpinBox_->setValue(defaults.resetColumnWidth);
        orderColumnWidthSpinBox_->setValue(defaults.orderColumnWidth);
        processPriorityColumnWidthSpinBox_->setValue(defaults.processPriorityColumnWidth);
        pidColumnWidthSpinBox_->setValue(defaults.pidColumnWidth);
        processStartColumnWidthSpinBox_->setValue(defaults.processStartColumnWidth);
        processCpuColumnWidthSpinBox_->setValue(defaults.processCpuColumnWidth);
        processMemoryColumnWidthSpinBox_->setValue(defaults.processMemoryColumnWidth);
        cpuColumnWidthSpinBox_->setValue(defaults.cpuColumnWidth);
        tableFontPointSizeSpinBox_->setValue(defaults.tableFontPointSize);
        tableRowHeightSpinBox_->setValue(defaults.tableRowHeight);
        cpuUsageRowHeightSpinBox_->setValue(defaults.cpuUsageRowHeight);
        cpuUsageBarWidthSpinBox_->setValue(defaults.cpuUsageBarWidth);
        cpuUsageBarColor_ = defaults.cpuUsageBarColor;
        updateCpuUsageBarColorButton();
        cpuUsageBarTransparencySpinBox_->setValue(
            defaults.cpuUsageBarTransparencyPercent);
        performanceUpdateIntervalSpinBox_->setValue(defaults.performanceUpdateIntervalSeconds);
        periodicApplyIntervalSpinBox_->setValue(defaults.periodicApplyIntervalSeconds);
    };
    auto *resetDisplayButton = new QPushButton(
        text(QStringLiteral("列幅と文字サイズ"),
             QStringLiteral("Column widths and font size")), resetGroup);
    auto *deleteCustomCpuStructuresButton = new QPushButton(
        text(QStringLiteral("カスタムCPU構造を削除"),
             QStringLiteral("Delete custom CPU structures")), resetGroup);
    resetLayout->addRow(resetDisplayButton);
    resetLayout->addRow(deleteCustomCpuStructuresButton);
    connect(resetDisplayButton, &QPushButton::clicked, this, [confirmReset, resetDisplay, text] {
        if (confirmReset(text(QStringLiteral("列幅と文字サイズ"),
                              QStringLiteral("column widths and font size")))) {
            resetDisplay();
        }
    });
    connect(deleteCustomCpuStructuresButton, &QPushButton::clicked, this,
            [this, confirmReset, deleteCustomCpuStructuresButton, text] {
                if (!confirmReset(text(QStringLiteral("カスタムCPU構造"),
                                       QStringLiteral("custom CPU structures")))) {
                    return;
                }
                deleteCustomCpuStructuresRequested_ = true;
                if (deleteCustomCpuStructures_) {
                    deleteCustomCpuStructures_();
                }
                deleteCustomCpuStructuresButton->setEnabled(false);
            });
    contentLayout->addWidget(resetGroup);
    contentLayout->addStretch();
    scrollArea->setWidget(content);
    layout->addWidget(scrollArea, 1);

    systemThemeButton_->setChecked(settings.themeMode == ThemeMode::System);
    lightThemeButton_->setChecked(settings.themeMode == ThemeMode::Light);
    darkThemeButton_->setChecked(settings.themeMode == ThemeMode::Dark);
    systemLanguageButton_->setChecked(settings.languageMode == LanguageMode::System);
    japaneseLanguageButton_->setChecked(settings.languageMode == LanguageMode::Japanese);
    englishLanguageButton_->setChecked(settings.languageMode == LanguageMode::English);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
        this);
    auto *applyButton = buttons->button(QDialogButtonBox::Apply);
    applyButton->setText(text(QStringLiteral("適用"), QStringLiteral("Apply")));
    connect(applyButton, &QPushButton::clicked, this, [this] {
        if (applySettings_) {
            applySettings_(this->settings());
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::updateCpuUsageBarColorButton()
{
    const QColor color(cpuUsageBarColor_);
    if (!color.isValid()) {
        cpuUsageBarColorButton_->setText(localizedText(
            initialSettings_,
            QStringLiteral("自動（文字色）"),
            QStringLiteral("Automatic (text color)")));
        cpuUsageBarColorButton_->setStyleSheet(QString());
        return;
    }

    const QColor buttonTextColor = color.lightness() < 128
        ? QColor(Qt::white)
        : QColor(Qt::black);
    cpuUsageBarColorButton_->setText(color.name(QColor::HexRgb).toUpper());
    cpuUsageBarColorButton_->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: %2; }")
            .arg(color.name(QColor::HexRgb), buttonTextColor.name(QColor::HexRgb)));
}

UiSettings SettingsDialog::settings() const
{
    UiSettings result = initialSettings_;
    result.themeMode = systemThemeButton_->isChecked()
        ? ThemeMode::System
        : (lightThemeButton_->isChecked() ? ThemeMode::Light : ThemeMode::Dark);
    result.languageMode = systemLanguageButton_->isChecked()
        ? LanguageMode::System
        : (japaneseLanguageButton_->isChecked() ? LanguageMode::Japanese : LanguageMode::English);
    result.cpuColumnWidth = cpuColumnWidthSpinBox_->value();
    result.enabledColumnWidth = enabledColumnWidthSpinBox_->value();
    result.executableColumnWidth = executableColumnWidthSpinBox_->value();
    result.applyOnceColumnWidth = applyOnceColumnWidthSpinBox_->value();
    result.periodicColumnWidth = periodicColumnWidthSpinBox_->value();
    result.resetColumnWidth = resetColumnWidthSpinBox_->value();
    result.orderColumnWidth = orderColumnWidthSpinBox_->value();
    result.processPriorityColumnWidth = processPriorityColumnWidthSpinBox_->value();
    result.pidColumnWidth = pidColumnWidthSpinBox_->value();
    result.processStartColumnWidth = processStartColumnWidthSpinBox_->value();
    result.processCpuColumnWidth = processCpuColumnWidthSpinBox_->value();
    result.processMemoryColumnWidth = processMemoryColumnWidthSpinBox_->value();
    result.showProcessStartColumn = showProcessStartColumnCheckBox_->isChecked();
    result.showProcessCpuColumn = showProcessCpuColumnCheckBox_->isChecked();
    result.showProcessMemoryColumn = showProcessMemoryColumnCheckBox_->isChecked();
    result.showCpuUsageRow = showCpuUsageRowCheckBox_->isChecked();
    result.tableFontPointSize = tableFontPointSizeSpinBox_->value();
    result.tableRowHeight = tableRowHeightSpinBox_->value();
    result.cpuUsageRowHeight = cpuUsageRowHeightSpinBox_->value();
    result.cpuUsageBarWidth = cpuUsageBarWidthSpinBox_->value();
    result.cpuUsageBarColor = cpuUsageBarColor_;
    result.cpuUsageBarTransparencyPercent = cpuUsageBarTransparencySpinBox_->value();
    result.separatorLineThickness = separatorLineThicknessSpinBox_->value();
    result.separatorLineTransparencyPercent = separatorLineTransparencySpinBox_->value();
    result.performanceUpdateIntervalSeconds = performanceUpdateIntervalSpinBox_->value();
    result.periodicApplyIntervalSeconds = periodicApplyIntervalSpinBox_->value();
    result.settingsDialogSize = size();
    return result;
}

bool SettingsDialog::deleteCustomCpuStructuresRequested() const
{
    return deleteCustomCpuStructuresRequested_;
}

} // namespace ProcessPresets
