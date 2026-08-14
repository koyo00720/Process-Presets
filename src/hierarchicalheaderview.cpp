#include "hierarchicalheaderview.h"

#include <QPainter>
#include <QPaintEvent>

namespace ProcessPresets {

HierarchicalHeaderView::HierarchicalHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    setSectionsClickable(false);
    setHighlightSections(false);
    setDefaultAlignment(Qt::AlignCenter);
    setMinimumSectionSize(1);
    setDefaultSectionSize(62);
    setFixedHeight(rowHeight_);
}

void HierarchicalHeaderView::setHierarchy(const QList<CpuNode> &roots)
{
    roots_ = roots;
    hierarchyDepth_ = hierarchyDepth(roots_);
    cells_.clear();
    int leafCursor = 0;
    appendCells(roots_, 0, &leafCursor);
    setFixedHeight(hierarchyDepth_ * rowHeight_);
    updateGeometry();
    viewport()->update();
}

void HierarchicalHeaderView::setFixedColumnLabels(const QString &applyLabel,
                                                  const QString &executableLabel)
{
    applyLabel_ = applyLabel;
    executableLabel_ = executableLabel;
    viewport()->update();
}

void HierarchicalHeaderView::setRowHeight(int height)
{
    rowHeight_ = qMax(1, height);
    setFixedHeight(hierarchyDepth_ * rowHeight_);
    updateGeometry();
    viewport()->update();
}

QSize HierarchicalHeaderView::sizeHint() const
{
    QSize result = QHeaderView::sizeHint();
    result.setHeight(hierarchyDepth_ * rowHeight_);
    return result;
}

int HierarchicalHeaderView::leavesIn(const CpuNode &node) const
{
    if (node.children.isEmpty()) {
        return 1;
    }
    int result = 0;
    for (const CpuNode &child : node.children) {
        result += leavesIn(child);
    }
    return result;
}

void HierarchicalHeaderView::appendCells(const QList<CpuNode> &nodes,
                                         int level,
                                         int *leafCursor)
{
    for (const CpuNode &node : nodes) {
        const int firstLeaf = *leafCursor;
        const int leafCount = leavesIn(node);
        HeaderCell cell;
        cell.level = level;
        cell.rowSpan = node.children.isEmpty() ? hierarchyDepth_ - level : 1;
        cell.firstLeaf = firstLeaf;
        cell.leafCount = leafCount;
        cell.isLeaf = node.children.isEmpty();
        cell.label = node.label;
        cells_.append(cell);

        if (node.children.isEmpty()) {
            ++(*leafCursor);
        } else {
            appendCells(node.children, level + 1, leafCursor);
        }
    }
}

void HierarchicalHeaderView::drawCell(QPainter &painter,
                                      const QRect &rect,
                                      const QString &label,
                                      int horizontalPadding) const
{
    if (rect.isEmpty()) {
        return;
    }

    painter.fillRect(rect, palette().brush(QPalette::Button));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(rect.adjusted(0, 0, -1, -1));
    painter.setPen(palette().color(QPalette::ButtonText));
    painter.drawText(rect.adjusted(horizontalPadding, 2, -horizontalPadding, -2),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     label);
}

void HierarchicalHeaderView::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    painter.setClipRegion(event->region());
    painter.fillRect(event->rect(), palette().brush(QPalette::Button));

    // The first two columns are ordinary table columns, vertically merged over
    // all hierarchy rows.  CPU columns are represented by cells_ below.
    if (count() > 0) {
        drawCell(painter,
                 QRect(sectionViewportPosition(0), 0, sectionSize(0), height()),
                 applyLabel_);
    }
    if (count() > 1) {
        drawCell(painter,
                 QRect(sectionViewportPosition(1), 0, sectionSize(1), height()),
                 executableLabel_);
    }

    for (const HeaderCell &cell : cells_) {
        const int firstColumn = 2 + cell.firstLeaf;
        if (firstColumn >= count()) {
            continue;
        }
        const int lastColumn = qMin(count() - 1, firstColumn + cell.leafCount - 1);
        const int x = sectionViewportPosition(firstColumn);
        const int right = sectionViewportPosition(lastColumn) + sectionSize(lastColumn);
        drawCell(painter,
                 QRect(x, cell.level * rowHeight_, right - x, cell.rowSpan * rowHeight_),
                 cell.label,
                 0);
    }
}

} // namespace ProcessPresets
