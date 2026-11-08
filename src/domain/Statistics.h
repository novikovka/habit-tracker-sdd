#pragma once
#include "data/CompletionRepository.h"

struct DayCount { QDate date; int count = 0; };
struct PeriodStatistics { QVector<DayCount> days; int total = 0; };
struct HabitStatistics { qint64 id; QString name; int streak; int days7; int days30; };
struct StatisticsData {
    QDate today;
    PeriodStatistics seven;
    PeriodStatistics thirty;
    QVector<HabitStatistics> habits;
};

namespace Statistics {
StatisticsData calculate(const TrackingSnapshot &snapshot, const QDate &today);
}
