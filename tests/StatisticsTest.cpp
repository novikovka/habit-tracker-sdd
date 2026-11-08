#include "domain/Statistics.h"
#include <QtTest>

class StatisticsTest : public QObject {
    Q_OBJECT
private slots:
    void acceptanceDataset()
    {
        const QDate today(2026,11,6);
        TrackingSnapshot source;
        source.habits={{1,QStringLiteral("Чтение")},{2,QStringLiteral("Прогулка")},{3,QStringLiteral("Без отметок")}};
        for(int offset:{0,1,6,7,29,30}) source.completions[1].insert(today.addDays(-offset));
        source.completions[1].insert(today.addDays(1));
        source.completions[2]={today,today.addDays(-2)};
        source.completions[999]={today}; // A deleted habit must not contribute.
        auto result=Statistics::calculate(source,today);
        QCOMPARE(result.seven.days.size(),7); QCOMPARE(result.thirty.days.size(),30);
        QCOMPARE(result.seven.days.front().date,today.addDays(-6));
        QCOMPARE(result.thirty.days.front().date,today.addDays(-29));
        QCOMPARE(result.seven.days.back().date,today);
        QCOMPARE(result.seven.total,5); QCOMPARE(result.thirty.total,7);
        QCOMPARE(result.seven.days.back().count,2);
        QCOMPARE(result.seven.days[3].count,0);
        QCOMPARE(result.habits[0].days7,3); QCOMPARE(result.habits[0].days30,5); QCOMPARE(result.habits[0].streak,2);
        QCOMPARE(result.habits[1].days7,2); QCOMPARE(result.habits[1].days30,2); QCOMPARE(result.habits[1].streak,1);
        QCOMPARE(result.habits[2].days7,0); QCOMPARE(result.habits[2].days30,0);
        int sum=0; for(const auto &day:result.thirty.days) sum+=day.count;
        QCOMPARE(sum,result.thirty.total);
        source.habits[0].name=QStringLiteral("Новое название");
        result=Statistics::calculate(source,today);
        QCOMPARE(result.habits[0].name,QStringLiteral("Новое название")); QCOMPARE(result.seven.total,5);
        source.habits.removeAt(0);
        result=Statistics::calculate(source,today);
        QCOMPARE(result.seven.total,2); QCOMPARE(result.thirty.total,2);
    }
    void emptyAndCalendarBoundaries()
    {
        auto result=Statistics::calculate({},QDate(2026,11,6));
        QCOMPARE(result.seven.total,0); QCOMPARE(result.thirty.total,0);
        QCOMPARE(result.seven.days.size(),7); QCOMPARE(result.thirty.days.size(),30);
        QVERIFY(result.habits.isEmpty());
        for(const auto &day:result.thirty.days) QCOMPARE(day.count,0);
        TrackingSnapshot source; source.habits={{1,"habit"}};
        source.completions[1]={QDate(2024,2,29),QDate(2024,3,1)};
        result=Statistics::calculate(source,QDate(2024,3,1));
        QCOMPARE(result.seven.total,2); QCOMPARE(result.seven.days[5].date,QDate(2024,2,29));
        source.completions[1]={QDate(2020,12,31),QDate(2021,1,1)};
        result=Statistics::calculate(source,QDate(2021,1,1)); QCOMPARE(result.seven.total,2);
        source.completions[1]={QDate(1,1,1)};
        result=Statistics::calculate(source,QDate(1,1,1));
        QCOMPARE(result.seven.days.size(),1); QCOMPARE(result.thirty.days.size(),1); QCOMPARE(result.seven.total,1);
        result=Statistics::calculate({},QDate(9999,12,31)); QCOMPARE(result.thirty.days.size(),30);
        QVERIFY(Statistics::calculate({},QDate()).seven.days.isEmpty());
    }
    void longRunAndMidnight()
    {
        const QDate today(2026,11,6);
        TrackingSnapshot source; source.habits={{1,"habit"}};
        for(int i=0;i<70;++i) source.completions[1].insert(today.addDays(-i));
        auto result=Statistics::calculate(source,today);
        QCOMPARE(result.habits[0].streak,70); QCOMPARE(result.seven.total,7); QCOMPARE(result.thirty.total,30);
        result=Statistics::calculate(source,today.addDays(1));
        QCOMPARE(result.habits[0].streak,70); QCOMPARE(result.seven.total,6); QCOMPARE(result.thirty.total,29);
        QCOMPARE(result.seven.days.back().count,0);
    }
};
QTEST_GUILESS_MAIN(StatisticsTest)
#include "StatisticsTest.moc"
