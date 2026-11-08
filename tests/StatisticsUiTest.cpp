#include "MainWindow.h"
#include "StatisticsPage.h"
#include "BarChart.h"
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class StatisticsUiTest : public QObject {
    Q_OBJECT
private slots:
    void errorDoesNotPretendToBeZero()
    {
        StatisticsPage page;
        TrackingSnapshot source; source.habits={{1,"habit"}};
        const QDate today(2026,11,6); source.completions[1]={today};
        page.setData(Statistics::calculate(source,today)); page.show();
        QCOMPARE(page.findChild<QLabel *>("total7")->property("count").toInt(),1);
        page.showError(QStringLiteral("Проверочная ошибка чтения"));
        QVERIFY(!page.findChild<BarChart *>("sevenChart")->isVisible());
        QVERIFY(page.findChild<QWidget *>("statisticsError")->isVisible());
        QCOMPARE(page.findChild<QLabel *>("total7")->property("count").toInt(),1);
        QSignalSpy retry(&page,&StatisticsPage::retryRequested);
        QTest::mouseClick(page.findChild<QPushButton *>("retryStatistics"),Qt::LeftButton);
        QCOMPARE(retry.count(),1);
        page.close();
    }
    void integrationStatisticsAndReturn()
    {
        if(qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS")!="1") QSKIP("Integration not requested");
        DatabaseConfig config; QString error;
        QVERIFY(DatabaseConfig::load(DatabaseConfig::defaultPath(),config,error));
        config.database=Database::expectedName(Database::Purpose::IntegrationTest);
        Database db(Database::Purpose::IntegrationTest); QVERIFY(db.open(config));
        HabitRepository habits(db); CompletionRepository marks(db);
        TrackingSnapshot existing; QVERIFY(marks.snapshot(existing));
        QDate today(2026,11,6);
        const auto baseline=Statistics::calculate(existing,today);
        QVector<qint64> ids;
        const auto cleanup=qScopeGuard([&]{for(auto id:ids) habits.remove(id);});
        QTemporaryDir dir; QVERIFY(dir.isValid()); const auto path=dir.filePath("connection.ini");
        {
            QSettings settings(path,QSettings::IniFormat);
            settings.setValue("database/host",config.host); settings.setValue("database/port",config.port);
            settings.setValue("database/name",config.database); settings.setValue("database/user",config.user); settings.sync();
        }
        const auto previous=qgetenv("HABIT_TRACKER_CONFIG");
        const auto restore=qScopeGuard([previous]{if(previous.isNull()) qunsetenv("HABIT_TRACKER_CONFIG"); else qputenv("HABIT_TRACKER_CONFIG",previous);});
        qputenv("HABIT_TRACKER_CONFIG",path.toUtf8());
        MainWindow window(nullptr,Database::Purpose::IntegrationTest,[&]{return today;});
        QSignalSpy connected(&window,&MainWindow::connectionFinished); window.show();
        QTRY_COMPARE_WITH_TIMEOUT(connected.count(),1,10000); QVERIFY(connected.at(0).at(0).toBool());
        auto *open=window.findChild<QPushButton *>("statistics"); QVERIFY(open->isEnabled());
        QTest::mouseClick(open,Qt::LeftButton);
        auto *page=window.findChild<StatisticsPage *>("statisticsPage"); QVERIFY(page->isVisible());
        auto total7=[&]{return page->findChild<QLabel *>("total7")->property("count").toInt();};
        auto total30=[&]{return page->findChild<QLabel *>("total30")->property("count").toInt();};
        auto *seven=page->findChild<BarChart *>("sevenChart");
        auto *thirty=page->findChild<BarChart *>("thirtyChart");
        QCOMPARE(total7(),baseline.seven.total); QCOMPARE(total30(),baseline.thirty.total);
        QCOMPARE(seven->days().size(),7); QCOMPARE(thirty->days().size(),30);
        if(existing.habits.isEmpty()) {
            QVERIFY(page->findChild<QLabel *>("emptyStatistics")->isVisible());
            QCoreApplication::processEvents();
            QVERIFY(window.grab().save("stage4-empty.png"));
        }
        auto *back=page->findChild<QPushButton *>("backToWeek"); QTest::mouseClick(back,Qt::LeftButton);
        QTest::mouseClick(window.findChild<QPushButton *>("previousWeek"),Qt::LeftButton);
        const auto selectedWeek=window.findChild<QLabel *>("weekRange")->text();
        const QString longName=QStringLiteral("Очень длинное название привычки для проверки отображения в таблице статистики ").repeated(2).left(100);
        for(const auto &name:{QStringLiteral("Чтение"),QStringLiteral("Прогулка"),longName}) {
            qint64 id=0; QVERIFY(habits.create(name,&id)); ids.append(id);
        }
        const qint64 reading=ids[0], walking=ids[1];
        for(int offset:{0,1,6,7,29,30}) QVERIFY(marks.setCompleted(reading,today.addDays(-offset),true,today));
        for(int offset:{0,2}) QVERIFY(marks.setCompleted(walking,today.addDays(-offset),true,today));
        QTest::mouseClick(open,Qt::LeftButton);
        QCOMPARE(total7(),baseline.seven.total+5); QCOMPARE(total30(),baseline.thirty.total+7);
        QCOMPARE(seven->days().back().count,baseline.seven.days.back().count+2);
        auto *table=page->findChild<QTableWidget *>("statisticsTable");
        auto rowFor=[&](qint64 id) {for(int i=0;i<table->rowCount();++i) if(table->item(i,0)->data(Qt::UserRole).toLongLong()==id) return i; return -1;};
        int row=rowFor(reading); QVERIFY(row>=0);
        QCOMPARE(table->item(row,1)->data(Qt::UserRole).toInt(),2);
        QCOMPARE(table->item(row,2)->text(),QStringLiteral("3")); QCOMPARE(table->item(row,3)->text(),QStringLiteral("5"));
        row=rowFor(ids[2]); QVERIFY(row>=0); QCOMPARE(table->item(row,0)->text().size(),100);
        QTRY_VERIFY(table->parentWidget()->height()>=table->height()+16);
        QTRY_COMPARE(page->findChild<QScrollArea *>("statisticsScroll")->horizontalScrollBar()->maximum(),0);
        if(existing.habits.isEmpty()) {
            QTRY_COMPARE(table->verticalScrollBar()->maximum(),0);
            QTRY_COMPARE(page->findChild<QScrollArea *>("statisticsScroll")->verticalScrollBar()->maximum(),0);
        }
        QVERIFY(window.grab().save("stage4-statistics.png"));
        // Data written by another connection must appear when the app regains focus.
        QVERIFY(marks.setCompleted(reading,today.addDays(-2),true,today));
        QEvent activate(QEvent::WindowActivate); QCoreApplication::sendEvent(&window,&activate);
        QTRY_COMPARE(total7(),baseline.seven.total+6); QCOMPARE(total30(),baseline.thirty.total+8);
        row=rowFor(reading); QCOMPARE(table->item(row,1)->data(Qt::UserRole).toInt(),3);
        QVERIFY(habits.rename(reading,QStringLiteral("Чтение 20 минут")));
        QCoreApplication::sendEvent(&window,&activate);
        QTRY_COMPARE(table->item(rowFor(reading),0)->text(),QStringLiteral("Чтение 20 минут"));
        QCOMPARE(total7(),baseline.seven.total+6);
        QVERIFY(habits.remove(reading)); ids.removeAll(reading);
        QCoreApplication::sendEvent(&window,&activate);
        QTRY_COMPARE(total7(),baseline.seven.total+2); QCOMPARE(total30(),baseline.thirty.total+2);
        QCOMPARE(rowFor(reading),-1);
        today=today.addDays(1);
        QTRY_COMPARE_WITH_TIMEOUT(seven->days().back().date,today,3000);
        const auto newBaseline=Statistics::calculate(existing,today);
        QCOMPARE(total7(),newBaseline.seven.total+2); QCOMPARE(seven->days().back().count,newBaseline.seven.days.back().count);
        window.resize(800,600); QCoreApplication::processEvents();
        QVERIFY(page->findChild<QScrollArea *>("statisticsScroll")->horizontalScrollBar()->maximum()>0);
        QTest::mouseClick(back,Qt::LeftButton);
        QCOMPARE(window.findChild<QLabel *>("weekRange")->text(),selectedWeek);
        QVERIFY(!page->isVisible()); QVERIFY(window.close());
    }
};
QTEST_MAIN(StatisticsUiTest)
#include "StatisticsUiTest.moc"
