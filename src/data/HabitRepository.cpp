#include "HabitRepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

bool HabitRepository::normalizeName(const QString &input, QString &name, QString &error)
{
    name = input.trimmed();
    error.clear();
    if (name.isEmpty()) error = QStringLiteral("Введите название привычки.");
    else if (name.size() > 100) error = QStringLiteral("Название должно содержать не более 100 символов.");
    else if (name.contains('\n') || name.contains('\r') || name.contains(QChar(0))
             || name.contains(QChar(0x2028)) || name.contains(QChar(0x2029)))
        error = QStringLiteral("Название должно быть одной строкой.");
    return error.isEmpty();
}

bool HabitRepository::available()
{
    error_.clear();
    if (database_.isOpen()) return true;
    error_ = QStringLiteral("Нет подключения к базе данных. Повторите подключение.");
    return false;
}

bool HabitRepository::list(QVector<Habit> &habits)
{
    if (!available()) return false;
    QSqlQuery query(database_.connection());
    if (!query.exec("SELECT id, name FROM public.habits ORDER BY created_at, id")) {
        error_ = QStringLiteral("Не удалось загрузить привычки. Повторите подключение.");
        return false;
    }
    QVector<Habit> result;
    while (query.next()) result.append({query.value(0).toLongLong(), query.value(1).toString()});
    if (query.lastError().isValid()) {
        error_ = QStringLiteral("Не удалось прочитать список привычек. Повторите подключение.");
        return false;
    }
    habits = result;
    return true;
}

bool HabitRepository::create(const QString &input, qint64 *createdId)
{
    QString name;
    if (!normalizeName(input, name, error_) || !available()) return false;
    QSqlQuery query(database_.connection());
    query.prepare("INSERT INTO public.habits(name) VALUES (?) RETURNING id");
    query.addBindValue(name);
    if (!query.exec() || !query.next()) {
        error_ = QStringLiteral("Не удалось подтвердить сохранение привычки. Проверьте подключение.");
        return false;
    }
    if (createdId) *createdId = query.value(0).toLongLong();
    return true;
}

bool HabitRepository::rename(qint64 id, const QString &input)
{
    QString name;
    if (!normalizeName(input, name, error_) || !available()) return false;
    QSqlQuery query(database_.connection());
    query.prepare("UPDATE public.habits SET name = ? WHERE id = ?");
    query.addBindValue(name);
    query.addBindValue(id);
    if (!query.exec()) {
        error_ = QStringLiteral("Не удалось подтвердить переименование. Проверьте подключение.");
        return false;
    }
    if (query.numRowsAffected() != 1) {
        error_ = QStringLiteral("Привычка уже удалена. Обновите список.");
        return false;
    }
    return true;
}

bool HabitRepository::remove(qint64 id)
{
    if (!available()) return false;
    QSqlQuery query(database_.connection());
    query.prepare("DELETE FROM public.habits WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec()) {
        error_ = QStringLiteral("Не удалось подтвердить удаление. Проверьте подключение и обновите список.");
        return false;
    }
    if (query.numRowsAffected() != 1) {
        error_ = QStringLiteral("Привычка уже удалена. Обновите список.");
        return false;
    }
    return true;
}
