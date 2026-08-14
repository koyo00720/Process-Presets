#pragma once

#include "models.h"

#include <QHeaderView>

class QPaintEvent;

namespace ProcessPresets {

class HierarchicalHeaderView final : public QHeaderView {
public:
    explicit HierarchicalHeaderView(QWidget *parent = nullptr);

    void setHierarchy(const QList<CpuNode> &roots);
    void setFixedColumnLabels(const QString &applyLabel, const QString &executableLabel);
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
    int rowHeight_ = 26;
    QString applyLabel_ = QStringLiteral("適用");
    QString executableLabel_ = QStringLiteral(".exe");
};

} // namespace ProcessPresets
