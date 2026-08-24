#pragma once

#include "models.h"

#include <QHeaderView>

class QPaintEvent;

namespace ProcessPresets {

class HierarchicalHeaderView final : public QHeaderView {
public:
    explicit HierarchicalHeaderView(QWidget *parent = nullptr);

    void setHierarchy(const QList<CpuNode> &roots);
    void setFixedColumnLabels(const QString &enabledLabel,
                              const QString &executableLabel,
                              const QString &applyLabel,
                              const QString &onceLabel,
                              const QString &periodicLabel,
                              const QString &resetLabel,
                              const QString &orderLabel,
                              const QString &processPriorityLabel,
                              const QString &pidLabel,
                              const QString &processStartLabel,
                              const QString &processCpuLabel,
                              const QString &processMemoryLabel);
    void setRowHeight(int height);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct HeaderCell {
        int level = 0;
        int rowSpan = 1;
        int firstLeaf = 0;
        int leafCount = 1;
        bool isLeaf = false;
        QString label;
    };

    int leavesIn(const CpuNode &node) const;
    void appendCells(const QList<CpuNode> &nodes, int level, int *leafCursor);
    void drawCell(QPainter &painter,
                  const QRect &rect,
                  const QString &label,
                  int horizontalPadding = 0) const;

    QList<CpuNode> roots_;
    QVector<HeaderCell> cells_;
    int hierarchyDepth_ = 1;
    int headerRowCount_ = 2;
    int rowHeight_ = 26;
    QString enabledLabel_ = QStringLiteral("有効");
    QString executableLabel_ = QStringLiteral(".exe");
    QString applyLabel_ = QStringLiteral("適用");
    QString onceLabel_ = QStringLiteral("1回");
    QString periodicLabel_ = QStringLiteral("定期");
    QString resetLabel_ = QStringLiteral("リセット");
    QString orderLabel_ = QStringLiteral("順番");
    QString processPriorityLabel_ = QStringLiteral("優先度");
    QString pidLabel_ = QStringLiteral("PID");
    QString processStartLabel_ = QStringLiteral("開始");
    QString processCpuLabel_ = QStringLiteral("CPU");
    QString processMemoryLabel_ = QStringLiteral("メモリ");
};

} // namespace ProcessPresets
