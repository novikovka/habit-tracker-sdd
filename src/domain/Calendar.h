#pragma once
#include <QDate>
#include <QSet>

namespace Calendar {
bool supported(const QDate &date);
QDate monday(const QDate &date);
QDate shiftWeek(const QDate &monday, int weeks);
int streak(const QSet<QDate> &dates, const QDate &today);
}
