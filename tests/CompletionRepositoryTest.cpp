#include "data/CompletionRepository.h"
#include "domain/Calendar.h"
#include <QScopeGuard>
#include <QSqlQuery>
#include <QtTest>

class CompletionRepositoryTest : public QObject {
    Q_OBJECT
private slots:
    void rejectsUnavailableAndInvalid()
    {
        Database db;
        CompletionRepository marks(db);
        const QDate today(2026,11,6);
        QVERIFY(!marks.setCompleted(1,today.addDays(1),true,today));
        QVERIFY(!marks.setCompleted(1,QDate(),true,today));
        QVERIFY(!marks.setCompleted(1,QDate(10000,1,1),true,QDate(10000,1,1)));
        QVERIFY(!marks.setCompleted(1,today,true,today));
        TrackingSnapshot existing; existing.habits.append({17,"sentinel"});
        QVERIFY(!marks.snapshot(existing));
        QCOMPARE(existing.habits.front().id,17);
    }
    void integrationIdempotenceAndCalendarDates()
    {
        if(qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS")!="1") QSKIP("Integration not requested");
        DatabaseConfig config; QString error;
        QVERIFY(DatabaseConfig::load(DatabaseConfig::defaultPath(),config,error));
        config.database=Database::expectedName(Database::Purpose::IntegrationTest);
        Database db(Database::Purpose::IntegrationTest); QVERIFY(db.open(config));
        HabitRepository habits(db); CompletionRepository marks(db);
        qint64 id=0; QVERIFY(habits.create("completion-sentinel",&id));
        const auto cleanup=qScopeGuard([&]{habits.remove(id);});
        const QDate today(2026,11,6);
        QVERIFY(marks.setCompleted(id,today,true,today));
        QVERIFY(marks.setCompleted(id,today,true,today));
        TrackingSnapshot data; QVERIFY(marks.snapshot(data));
        QCOMPARE(data.completions.value(id).size(),1);
        QVERIFY(!marks.setCompleted(id,today.addDays(1),true,today));
        QVERIFY(!marks.setCompleted(id,today,false,today.addDays(-1)));
        QVERIFY(marks.snapshot(data)); QVERIFY(data.completions.value(id).contains(today));
        QVERIFY(marks.setCompleted(id,today,false,today));
        QVERIFY(marks.setCompleted(id,today,false,today));
        QVERIFY(marks.snapshot(data)); QVERIFY(data.completions.value(id).isEmpty());
        for(const auto date:{QDate(2024,2,28),QDate(2024,2,29),QDate(2024,3,1)})
            QVERIFY(marks.setCompleted(id,date,true,today));
        QVERIFY(marks.setCompleted(id,QDate(1,1,1),true,today));
        QVERIFY(marks.setCompleted(id,QDate(9999,12,31),true,QDate(9999,12,31)));
        QVERIFY(marks.snapshot(data));
        QCOMPARE(Calendar::streak(data.completions.value(id),QDate(2024,3,1)),3);
        QVERIFY(data.completions.value(id).contains(QDate(1,1,1)));
        QVERIFY(data.completions.value(id).contains(QDate(9999,12,31)));
        QVERIFY(!marks.setCompleted(-1,today,true,today));
        Database reopened(Database::Purpose::IntegrationTest); QVERIFY(reopened.open(config));
        CompletionRepository other(reopened); TrackingSnapshot persisted;
        QVERIFY(other.snapshot(persisted));
        QCOMPARE(persisted.completions.value(id),data.completions.value(id));
    }
};
QTEST_GUILESS_MAIN(CompletionRepositoryTest)
#include "CompletionRepositoryTest.moc"
