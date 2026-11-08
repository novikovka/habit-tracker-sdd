#pragma once
#include "domain/Statistics.h"
#include <QWidget>

class BarChart final : public QWidget {
    Q_OBJECT
public:
    explicit BarChart(QWidget *parent=nullptr);
    void setDays(const QVector<DayCount> &days);
    const QVector<DayCount> &days() const { return days_; }
protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *event) override;
private:
    QRectF plotRect() const;
    QVector<DayCount> days_;
};
