#pragma once
#include "domain/Statistics.h"
#include <QWidget>

class BarChart;
class QLabel;
class QTableWidget;
class QScrollArea;

class StatisticsPage final : public QWidget {
    Q_OBJECT
public:
    explicit StatisticsPage(QWidget *parent=nullptr);
    void setData(const StatisticsData &data);
    void showError(const QString &error);
signals:
    void backRequested();
    void retryRequested();
private:
    QWidget *body_;
    QScrollArea *scroll_;
    QWidget *errorPanel_;
    BarChart *sevenChart_;
    BarChart *thirtyChart_;
    QLabel *total7_;
    QLabel *total30_;
    QLabel *range7_;
    QLabel *range30_;
    QLabel *cutoff_;
    QLabel *error_;
    QLabel *empty_;
    QTableWidget *table_;
};
