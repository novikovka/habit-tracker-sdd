#include "CompletionRepository.h"
#include "domain/Calendar.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool CompletionRepository::snapshot(TrackingSnapshot &result)
{
    error_.clear();
    auto connection = database_.connection();
    if (!database_.isOpen() || !connection.transaction()) {
        error_ = QStringLiteral("Не удалось загрузить отметки. Повторите подключение.");
        return false;
    }
    TrackingSnapshot data;
    bool ok = false;
    {
        HabitRepository habits(database_);
        QSqlQuery query(connection);
        ok = query.exec("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ READ ONLY") && habits.list(data.habits);
        if (ok) ok = query.exec("SELECT habit_id, completed_on FROM public.habit_completions");
        while (ok && query.next()) {
            const auto date = query.value(1).toDate();
            if (!Calendar::supported(date)) { ok = false; break; }
            data.completions[query.value(0).toLongLong()].insert(date);
        }
        if (query.lastError().isValid()) ok = false;
    }
    if (!ok || !connection.commit()) {
        connection.rollback();
        error_ = QStringLiteral("Не удалось прочитать привычки и отметки. Повторите подключение.");
        return false;
    }
    result = data;
    return true;
}

bool CompletionRepository::setCompleted(qint64 habitId, const QDate &date, bool completed, const QDate &today)
{
    error_.clear();
    if (!Calendar::supported(date) || !Calendar::supported(today) || date > today) {
        error_ = QStringLiteral("Отметку можно изменить только за наступивший день в допустимом диапазоне дат.");
        return false;
    }
    if (!database_.isOpen()) {
        error_ = QStringLiteral("Нет подключения к базе данных. Повторите подключение.");
        return false;
    }
    QSqlQuery query(database_.connection());
    query.prepare(completed
        ? "INSERT INTO public.habit_completions(habit_id, completed_on) VALUES (?, ?) ON CONFLICT DO NOTHING"
        : "DELETE FROM public.habit_completions WHERE habit_id = ? AND completed_on = ?");
    query.addBindValue(habitId);
    query.addBindValue(date);
    if (!query.exec()) {
        error_ = QStringLiteral("Не удалось подтвердить изменение отметки. Обновите данные и попробуйте ещё раз.");
        return false;
    }
    return true;
}
