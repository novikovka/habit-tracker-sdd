#pragma once
#include "HabitRepository.h"
#include <QDate>
#include <QHash>
#include <QSet>

struct TrackingSnapshot {
    QVector<Habit> habits;
    QHash<qint64, QSet<QDate>> completions;
};

class CompletionRepository final {
public:
    explicit CompletionRepository(Database &database) : database_(database) {}
    bool snapshot(TrackingSnapshot &result);
    bool setCompleted(qint64 habitId, const QDate &date, bool completed, const QDate &today);
    QString error() const { return error_; }
private:
    Database &database_;
    QString error_;
};
