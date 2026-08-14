#include "settingsdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ProcessPresets {

SettingsDialog::SettingsDialog(const UiSettings &settings,
                               std::function<void()> deleteCustomCpuStructures,
                               QWidget *parent)
    : QDialog(parent)
    , initialSettings_(settings)
    , deleteCustomCpuStructures_(std::move(deleteCustomCpuStructures))
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
    applyColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    executableColumnWidthSpinBox_ = new QSpinBox(displayGroup);
    tableFontPointSizeSpinBox_ = new QSpinBox(displayGroup);
    for (QSpinBox *spinBox : {applyColumnWidthSpinBox_, executableColumnWidthSpinBox_}) {
        spinBox->setRange(1, 600);
        spinBox->setSuffix(QStringLiteral(" px"));
    }
    cpuColumnWidthSpinBox_->setRange(1, 600);
    cpuColumnWidthSpinBox_->setSuffix(QStringLiteral(" px"));
    cpuColumnWidthSpinBox_->setValue(settings.cpuColumnWidth);
    applyColumnWidthSpinBox_->setValue(settings.applyColumnWidth);
    executableColumnWidthSpinBox_->setValue(settings.executableColumnWidth);
    tableFontPointSizeSpinBox_->setRange(6, 48);
    tableFontPointSizeSpinBox_->setSuffix(QStringLiteral(" pt"));
    tableFontPointSizeSpinBox_->setValue(settings.tableFontPointSize);
    displayLayout->addRow(
        text(QStringLiteral("適用列:"), QStringLiteral("Apply column:")), applyColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral(".exe列:"), QStringLiteral(".exe column:")), executableColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("CPU構造の最下層列:"), QStringLiteral("Lowest CPU hierarchy column:")),
        cpuColumnWidthSpinBox_);
    displayLayout->addRow(
        text(QStringLiteral("表の文字サイズ:"), QStringLiteral("Table font size:")),
        tableFontPointSizeSpinBox_);
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
        applyColumnWidthSpinBox_->setValue(defaults.applyColumnWidth);
        executableColumnWidthSpinBox_->setValue(defaults.executableColumnWidth);
        cpuColumnWidthSpinBox_->setValue(defaults.cpuColumnWidth);
        tableFontPointSizeSpinBox_->setValue(defaults.tableFontPointSize);
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

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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
    result.applyColumnWidth = applyColumnWidthSpinBox_->value();
    result.executableColumnWidth = executableColumnWidthSpinBox_->value();
    result.tableFontPointSize = tableFontPointSizeSpinBox_->value();
    result.settingsDialogSize = size();
    return result;
}

bool SettingsDialog::deleteCustomCpuStructuresRequested() const
{
    return deleteCustomCpuStructuresRequested_;
}

} // namespace ProcessPresets
