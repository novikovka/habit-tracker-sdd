#include "domain/Calendar.h"
#include <QtTest>

class CalendarTest : public QObject {
    Q_OBJECT
private slots:
    void weeks_data()
    {
        QTest::addColumn<QDate>("date"); QTest::addColumn<QDate>("start");
        QTest::newRow("monday") << QDate(2026,11,2) << QDate(2026,11,2);
        QTest::newRow("sunday") << QDate(2026,11,8) << QDate(2026,11,2);
        QTest::newRow("year-crossing") << QDate(2021,1,1) << QDate(2020,12,28);
        QTest::newRow("leap-day") << QDate(2024,2,29) << QDate(2024,2,26);
        QTest::newRow("spring-clock-change") << QDate(2024,3,31) << QDate(2024,3,25);
        QTest::newRow("autumn-clock-change") << QDate(2024,10,27) << QDate(2024,10,21);
        QTest::newRow("first-day") << QDate(1,1,1) << QDate(1,1,1);
        QTest::newRow("last-day") << QDate(9999,12,31) << QDate(9999,12,27);
        QTest::newRow("invalid") << QDate() << QDate();
        QTest::newRow("outside") << QDate(10000,1,1) << QDate();
    }
    void weeks() { QFETCH(QDate,date); QFETCH(QDate,start); QCOMPARE(Calendar::monday(date),start); }
    void navigationLimits()
    {
        QVERIFY(!Calendar::shiftWeek(QDate(1,1,1),-1).isValid());
        QVERIFY(!Calendar::shiftWeek(QDate(9999,12,27),1).isValid());
        QCOMPARE(Calendar::shiftWeek(QDate(2020,12,28),1),QDate(2021,1,4));
        QVERIFY(!Calendar::shiftWeek(QDate(2026,11,3),1).isValid());
        QVERIFY(!Calendar::supported(QDate(10000,1,1)));
        QVERIFY(Calendar::supported(QDate(9999,12,31)));
    }
    void streaks()
    {
        const QDate today(2026,11,6);
        QSet<QDate> marks{today.addDays(-2),today.addDays(-1),today};
        QCOMPARE(Calendar::streak(marks,today),3);
        marks.remove(today); QCOMPARE(Calendar::streak(marks,today),2);
        marks.remove(today.addDays(-1)); QCOMPARE(Calendar::streak(marks,today),0);
        marks.insert(today); QCOMPARE(Calendar::streak(marks,today),1);
        marks.insert(today.addDays(1)); QCOMPARE(Calendar::streak(marks,today),1);
        QCOMPARE(Calendar::streak({},today),0);
        QCOMPARE(Calendar::streak({QDate(2024,2,28),QDate(2024,2,29),QDate(2024,3,1)},QDate(2024,3,1)),3);
        QCOMPARE(Calendar::streak({QDate(2020,12,31),QDate(2021,1,1)},QDate(2021,1,1)),2);
        QCOMPARE(Calendar::streak({QDate(1,1,1)},QDate(1,1,1)),1);
        QCOMPARE(Calendar::streak({},QDate(1,1,1)),0);
        QSet<QDate> longRun;
        for(int i=0;i<70;++i) longRun.insert(today.addDays(-i));
        QCOMPARE(Calendar::streak(longRun,today),70);
        QCOMPARE(Calendar::streak(longRun,today.addDays(1)),70);
        QCOMPARE(Calendar::streak(longRun,today.addDays(2)),0);
    }
};
QTEST_GUILESS_MAIN(CalendarTest)
#include "CalendarTest.moc"
