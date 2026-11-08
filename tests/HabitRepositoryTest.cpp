#include "data/HabitRepository.h"
#include <QDate>
#include <QScopeGuard>
#include <QSqlQuery>
#include <QtTest>

class HabitRepositoryTest : public QObject {
    Q_OBJECT
private slots:
    void validation_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<QString>("normalized");
        QTest::newRow("empty") << QString() << false << QString();
        QTest::newRow("spaces") << QStringLiteral(" \t\n ") << false << QString();
        QTest::newRow("unicode-space") << QString(QChar(0x2003)) << false << QString();
        QTest::newRow("cyrillic") << QStringLiteral("  Чтение  ") << true << QStringLiteral("Чтение");
        QTest::newRow("limit") << QString(100, QChar('a')) << true << QString(100, QChar('a'));
        QTest::newRow("too-long") << QString(101, QChar('a')) << false << QString();
        QTest::newRow("multiline") << QStringLiteral("one\ntwo") << false << QString();
        QTest::newRow("paragraph") << (QStringLiteral("one") + QChar(0x2029) + "two") << false << QString();
        QTest::newRow("null") << (QStringLiteral("one") + QChar(0) + "two") << false << QString();
    }
    void validation()
    {
        QFETCH(QString, input);
        QFETCH(bool, valid);
        QFETCH(QString, normalized);
        QString name, error;
        QCOMPARE(HabitRepository::normalizeName(input, name, error), valid);
        QCOMPARE(error.isEmpty(), valid);
        if (valid) QCOMPARE(name, normalized);
    }
    void closedConnection()
    {
        Database db;
        HabitRepository repo(db);
        QVector<Habit> untouched{{17, "sentinel"}};
        QVERIFY(!repo.list(untouched));
        QCOMPARE(untouched.front().id, 17);
        QVERIFY(!repo.create("valid"));
        QVERIFY(!repo.error().isEmpty());
        QVERIFY(!repo.rename(17, "valid"));
        QVERIFY(!repo.remove(17));
    }
    void integrationCrudAndPersistence()
    {
        if (qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS") != "1") QSKIP("Integration not requested");
        DatabaseConfig config;
        QString error;
        QVERIFY2(DatabaseConfig::load(DatabaseConfig::defaultPath(), config, error), qUtf8Printable(error));
        config.database = Database::expectedName(Database::Purpose::IntegrationTest);
        Database db(Database::Purpose::IntegrationTest);
        QVERIFY2(db.open(config), qUtf8Printable(db.error()));
        HabitRepository repo(db);
        QVector<qint64> ids;
        const auto cleanup = qScopeGuard([&] { for (const auto id : ids) repo.remove(id); });
        qint64 first = 0, duplicate = 0, literal = 0;
        QVERIFY(repo.create(QStringLiteral("  Чтение  "), &first)); ids.append(first);
        QVERIFY(repo.create(QStringLiteral("Чтение"), &duplicate)); ids.append(duplicate);
        QVERIFY(first != duplicate);
        const QString sqlText = QStringLiteral("x'); DROP TABLE habits; --");
        QVERIFY(repo.create(sqlText, &literal)); ids.append(literal);
        QVERIFY(!repo.create(QStringLiteral("   ")));
        QVERIFY(!repo.rename(first, QString(101, QChar('x'))));
        {
            QSqlQuery query(db.connection());
            query.prepare("INSERT INTO public.habit_completions(habit_id, completed_on) VALUES (?, ?)");
            query.addBindValue(first);
            query.addBindValue(QDate(2024, 2, 29));
            QVERIFY(query.exec());
        }
        QVERIFY(repo.rename(first, QStringLiteral("  Прогулка  ")));
        QVector<Habit> habits;
        QVERIFY(repo.list(habits));
        QVector<qint64> ordered;
        for (const auto &habit : habits) if (ids.contains(habit.id)) ordered.append(habit.id);
        QCOMPARE(ordered, ids);
        {
            Database reopened(Database::Purpose::IntegrationTest);
            QVERIFY(reopened.open(config));
            HabitRepository other(reopened);
            QVector<Habit> saved;
            QVERIFY(other.list(saved));
            bool renamed = false, literalPreserved = false;
            for (const auto &habit : saved) {
                if (habit.id == first) renamed = habit.name == QStringLiteral("Прогулка");
                if (habit.id == literal) literalPreserved = habit.name == sqlText;
            }
            QVERIFY(renamed && literalPreserved);
            QSqlQuery query(reopened.connection());
            query.prepare("SELECT count(*) FROM public.habit_completions WHERE habit_id = ?");
            query.addBindValue(first);
            QVERIFY(query.exec()); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 1);
        }
        QVERIFY(repo.remove(first)); ids.removeAll(first);
        {
            QSqlQuery query(db.connection());
            query.prepare("SELECT count(*) FROM public.habit_completions WHERE habit_id = ?");
            query.addBindValue(first);
            QVERIFY(query.exec()); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 0);
        }
        QVERIFY(!repo.rename(first, "gone"));
        QVERIFY(!repo.remove(first));
    }
};
QTEST_GUILESS_MAIN(HabitRepositoryTest)
#include "HabitRepositoryTest.moc"
