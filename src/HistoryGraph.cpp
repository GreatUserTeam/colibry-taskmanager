#include "HistoryGraph.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace colibry::tm {

HistoryGraph::HistoryGraph(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
}

void HistoryGraph::setCapacity(int samples)
{
    m_capacity = std::max(2, samples);
    while (m_samples.size() > m_capacity)
        m_samples.removeFirst();
    update();
}

void HistoryGraph::addSample(double value)
{
    if (!std::isfinite(value))
        value = 0;
    m_samples.append(std::clamp(value, 0.0, 1.0));
    while (m_samples.size() > m_capacity)
        m_samples.removeFirst();
    update();
}

void HistoryGraph::clear()
{
    m_samples.clear();
    update();
}

void HistoryGraph::setLabel(const QString &text)
{
    if (m_label == text)
        return;
    m_label = text;
    update();
}

void HistoryGraph::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.fillRect(rect(), palette().color(QPalette::Base));

    // Grid: quarters, dashed.
    QColor grid = palette().color(QPalette::Mid);
    grid.setAlpha(120);
    p.setPen(QPen(grid, 1, Qt::DashLine));
    for (int i = 1; i < 4; ++i) {
        const double y = r.top() + r.height() * i / 4.0;
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }

    // Curve, right-aligned so that a young graph grows in from the right edge.
    if (!m_samples.isEmpty()) {
        const double step = r.width() / (m_capacity - 1);
        const int n = static_cast<int>(m_samples.size());

        QPainterPath line;
        for (int k = 0; k < n; ++k) {
            const double x = r.right() - (n - 1 - k) * step;
            const double y = r.bottom() - m_samples.at(k) * r.height();
            if (k == 0)
                line.moveTo(x, y);
            else
                line.lineTo(x, y);
        }

        const QColor accent = palette().color(QPalette::Highlight);
        QPainterPath area = line;
        area.lineTo(r.right(), r.bottom());
        area.lineTo(r.right() - (n - 1) * step, r.bottom());
        area.closeSubpath();

        QColor fill = accent;
        fill.setAlpha(70);
        p.fillPath(area, fill);
        p.setPen(QPen(accent, 1.6));
        p.drawPath(line);
    }

    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);

    if (!m_label.isEmpty()) {
        p.setPen(palette().color(QPalette::Text));
        p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, m_label);
    }
}

} // namespace colibry::tm
