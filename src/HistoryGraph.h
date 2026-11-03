#pragma once

#include <QVector>
#include <QWidget>

namespace colibry::tm {

/// A small filled line graph of the last samples, newest on the right. Values are 0..1.
class HistoryGraph : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryGraph(QWidget *parent = nullptr);

    void setCapacity(int samples);
    int capacity() const { return m_capacity; }

    /// Adds a value (clamped to 0..1) and drops the oldest one when the graph is full.
    void addSample(double value);
    void clear();
    const QVector<double> &samples() const { return m_samples; }

    void setLabel(const QString &text);
    QString label() const { return m_label; }

    QSize minimumSizeHint() const override { return {160, 90}; }
    QSize sizeHint() const override { return {420, 140}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_samples;
    int m_capacity = 60;
    QString m_label;
};

} // namespace colibry::tm
