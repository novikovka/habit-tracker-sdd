#include "Statistics.h"
#include "Calendar.h"

static PeriodStatistics makePeriod(const QDate &today, int days)
{
    PeriodStatistics result;
    auto start = today.addDays(1-days);
    if (!Calendar::supported(start)) start = QDate(1,1,1);
    for (auto date=start; date<=today; date=date.addDays(1)) result.days.append({date,0});
    return result;
}

StatisticsData Statistics::calculate(const TrackingSnapshot &snapshot, const QDate &today)
{
    StatisticsData result;
    result.today = today;
    if (!Calendar::supported(today)) return result;
    result.seven = makePeriod(today,7);
    result.thirty = makePeriod(today,30);
    for (const auto &habit : snapshot.habits) {
        const auto dates = snapshot.completions.value(habit.id);
        HabitStatistics row{habit.id,habit.name,Calendar::streak(dates,today),0,0};
        for (const auto date : dates) {
            if (date>today || date<result.thirty.days.front().date) continue;
            ++row.days30;
            ++result.thirty.total;
            ++result.thirty.days[int(result.thirty.days.front().date.daysTo(date))].count;
            if (date>=result.seven.days.front().date) {
                ++row.days7;
                ++result.seven.total;
                ++result.seven.days[int(result.seven.days.front().date.daysTo(date))].count;
            }
        }
        result.habits.append(row);
    }
    return result;
}
