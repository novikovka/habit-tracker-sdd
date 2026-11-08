#include "Calendar.h"

bool Calendar::supported(const QDate &date)
{
    return date.isValid() && date.year() >= 1 && date.year() <= 9999;
}

QDate Calendar::monday(const QDate &date)
{
    return supported(date) ? date.addDays(1 - date.dayOfWeek()) : QDate();
}

QDate Calendar::shiftWeek(const QDate &start, int weeks)
{
    if (!supported(start) || start.dayOfWeek() != 1) return {};
    const auto next = start.addDays(qint64(weeks) * 7);
    return supported(next) ? next : QDate();
}

int Calendar::streak(const QSet<QDate> &dates, const QDate &today)
{
    if (!supported(today)) return 0;
    auto date = dates.contains(today) ? today : today.addDays(-1);
    int days = 0;
    while (supported(date) && dates.contains(date)) {
        ++days;
        date = date.addDays(-1);
    }
    return days;
}
