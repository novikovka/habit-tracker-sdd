#pragma once
#include "Database.h"
#include <QVector>

struct Habit { qint64 id; QString name; };

class HabitRepository final {
public:
    explicit HabitRepository(Database &database) : database_(database) {}
    static bool normalizeName(const QString &input, QString &name, QString &error);
    bool list(QVector<Habit> &habits);
    bool create(const QString &name, qint64 *createdId = nullptr);
    bool rename(qint64 id, const QString &name);
    bool remove(qint64 id);
    QString error() const { return error_; }
private:
    bool available();
    Database &database_;
    QString error_;
};
