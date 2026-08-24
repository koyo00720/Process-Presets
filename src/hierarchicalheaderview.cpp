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
    headerRowCount_ = qMax(2, hierarchyDepth_);
    cells_.clear();
    int leafCursor = 0;
    appendCells(roots_, 0, &leafCursor);
    setFixedHeight(headerRowCount_ * rowHeight_);
    updateGeometry();
    viewport()->update();
}

void HierarchicalHeaderView::setFixedColumnLabels(const QString &enabledLabel,
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
                                                  const QString &processMemoryLabel)
{
    enabledLabel_ = enabledLabel;
    applyLabel_ = applyLabel;
    executableLabel_ = executableLabel;
    onceLabel_ = onceLabel;
    periodicLabel_ = periodicLabel;
    resetLabel_ = resetLabel;
    orderLabel_ = orderLabel;
    processPriorityLabel_ = processPriorityLabel;
    pidLabel_ = pidLabel;
    processStartLabel_ = processStartLabel;
    processCpuLabel_ = processCpuLabel;
    processMemoryLabel_ = processMemoryLabel;
    viewport()->update();
}

void HierarchicalHeaderView::setRowHeight(int height)
{
    rowHeight_ = qMax(1, height);
    setFixedHeight(headerRowCount_ * rowHeight_);
    updateGeometry();
    viewport()->update();
}

QSize HierarchicalHeaderView::sizeHint() const
{
    QSize result = QHeaderView::sizeHint();
    result.setHeight(headerRowCount_ * rowHeight_);
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
        cell.rowSpan = node.children.isEmpty() ? headerRowCount_ - level : 1;
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

    // The fixed columns are vertically merged over all header rows, except
    // Apply, which has a two-level header. CPU columns are represented by
    // cells_ below.
    if (count() > 0) {
        drawCell(painter,
                 QRect(sectionViewportPosition(0), 0, sectionSize(0), height()),
                 enabledLabel_);
    }
    if (count() > 1) {
        drawCell(painter,
                 QRect(sectionViewportPosition(1), 0, sectionSize(1), height()),
                 executableLabel_);
    }
    if (count() > 3) {
        const int applyX = sectionViewportPosition(2);
        const int applyRight = sectionViewportPosition(3) + sectionSize(3);
        drawCell(painter, QRect(applyX, 0, applyRight - applyX, rowHeight_), applyLabel_);
        drawCell(painter,
                 QRect(sectionViewportPosition(2), rowHeight_, sectionSize(2),
                       height() - rowHeight_),
                 onceLabel_);
        drawCell(painter,
                 QRect(sectionViewportPosition(3), rowHeight_, sectionSize(3),
                       height() - rowHeight_),
                 periodicLabel_);
    }
    if (count() > 4) {
        drawCell(painter,
                 QRect(sectionViewportPosition(4), 0, sectionSize(4), height()),
                 resetLabel_);
    }
    if (count() > 5) {
        drawCell(painter,
                 QRect(sectionViewportPosition(5), 0, sectionSize(5), height()),
                 orderLabel_);
    }
    if (count() > 6) {
        drawCell(painter,
                 QRect(sectionViewportPosition(6), 0, sectionSize(6), height()),
                 processPriorityLabel_);
    }
    if (count() > 7) {
        drawCell(painter,
                 QRect(sectionViewportPosition(7), 0, sectionSize(7), height()),
                 pidLabel_);
    }
    if (count() > 8) {
        drawCell(painter,
                 QRect(sectionViewportPosition(8), 0, sectionSize(8), height()),
                 processStartLabel_);
    }
    if (count() > 9) {
        drawCell(painter,
                 QRect(sectionViewportPosition(9), 0, sectionSize(9), height()),
                 processCpuLabel_);
    }
    if (count() > 10) {
        drawCell(painter,
                 QRect(sectionViewportPosition(10), 0, sectionSize(10), height()),
                 processMemoryLabel_);
    }

    for (const HeaderCell &cell : cells_) {
        const int firstColumn = 11 + cell.firstLeaf;
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
