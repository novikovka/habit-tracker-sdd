#include "MainWindow.h"
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class WeekUiTest : public QObject {
    Q_OBJECT
private slots:
    void integrationWeekMarksAndDayChange()
    {
        if(qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS")!="1") QSKIP("Integration not requested");
        DatabaseConfig config; QString error;
        QVERIFY(DatabaseConfig::load(DatabaseConfig::defaultPath(),config,error));
        config.database=Database::expectedName(Database::Purpose::IntegrationTest);
        Database db(Database::Purpose::IntegrationTest); QVERIFY(db.open(config));
        HabitRepository habits(db); CompletionRepository marks(db);
        QVector<qint64> ids;
        const auto cleanup=qScopeGuard([&]{for(auto id:ids) habits.remove(id);});
        for(const auto &name:{QStringLiteral("Чтение 20 минут"),QStringLiteral("Прогулка"),QStringLiteral("Растяжка")}) {
            qint64 id=0; QVERIFY(habits.create(name,&id)); ids.append(id);
        }
        QDate today(2026,11,6);
        QVERIFY(marks.setCompleted(ids[0],QDate(2026,11,4),true,today));
        QVERIFY(marks.setCompleted(ids[0],QDate(2026,11,5),true,today));
        QVERIFY(marks.setCompleted(ids[1],today,true,today));
        QTemporaryDir dir; QVERIFY(dir.isValid()); const auto path=dir.filePath("connection.ini");
        {
            QSettings settings(path,QSettings::IniFormat);
            settings.setValue("database/host",config.host); settings.setValue("database/port",config.port);
            settings.setValue("database/name",config.database); settings.setValue("database/user",config.user);
            settings.sync();
        }
        const auto previous=qgetenv("HABIT_TRACKER_CONFIG");
        const auto restore=qScopeGuard([previous]{if(previous.isNull()) qunsetenv("HABIT_TRACKER_CONFIG"); else qputenv("HABIT_TRACKER_CONFIG",previous);});
        qputenv("HABIT_TRACKER_CONFIG",path.toUtf8());
        MainWindow window(nullptr,Database::Purpose::IntegrationTest,[&]{return today;});
        QSignalSpy connected(&window,&MainWindow::connectionFinished); window.show();
        QTRY_COMPARE_WITH_TIMEOUT(connected.count(),1,10000); QVERIFY(connected.at(0).at(0).toBool());
        auto *table=window.findChild<QTableWidget *>("habitsTable"); QVERIFY(table); QCOMPARE(table->columnCount(),10);
        int row=-1; for(int i=0;i<table->rowCount();++i) if(table->item(i,0)->data(Qt::UserRole).toLongLong()==ids[0]) row=i;
        QVERIFY(row>=0);
        auto cell=[&](int column){return table->cellWidget(row,column)->findChild<QPushButton *>("completion");};
        auto series=[&]{return table->item(row,1)->data(Qt::UserRole).toInt();};
        QCOMPARE(series(),2); QVERIFY(cell(6)->isEnabled()); QVERIFY(!cell(7)->isEnabled());
        QVERIFY(table->horizontalHeaderItem(6)->text().contains("06.11"));
        cell(6)->setFocus(); QTest::keyClick(cell(6),Qt::Key_Space);
        QTRY_COMPARE(series(),3); QVERIFY(cell(6)->isChecked());
        QTRY_VERIFY(QApplication::focusWidget()==cell(6));
        QTest::keyClick(QApplication::focusWidget(),Qt::Key_Space);
        QTRY_COMPARE(series(),2); QVERIFY(!cell(6)->isChecked());
        QTest::mouseClick(cell(6),Qt::LeftButton); QTRY_COMPARE(series(),3);
        QTest::mouseClick(cell(5),Qt::LeftButton); QTRY_COMPARE(series(),1);
        QTest::mouseClick(cell(5),Qt::LeftButton); QTRY_COMPARE(series(),3);
        auto *previousWeek=window.findChild<QPushButton *>("previousWeek");
        auto *nextWeek=window.findChild<QPushButton *>("nextWeek");
        QTest::mouseClick(previousWeek,Qt::LeftButton);
        QVERIFY(table->horizontalHeaderItem(2)->text().contains("26.10")); QCOMPARE(series(),3);
        QTest::mouseClick(cell(8),Qt::LeftButton);
        QTRY_VERIFY(cell(8)->isChecked()); // Before the habit's actual creation date.
        QTest::mouseClick(nextWeek,Qt::LeftButton); QVERIFY(cell(6)->isChecked());
        QTest::mouseClick(nextWeek,Qt::LeftButton);
        for(int column=2;column<9;++column) QVERIFY(!cell(column)->isEnabled());
        QCOMPARE(series(),3);
        QTest::mouseClick(previousWeek,Qt::LeftButton);
        today=QDate(2026,11,7);
        QTRY_VERIFY_WITH_TIMEOUT(cell(7)->isEnabled(),3000);
        QCOMPARE(series(),3); // Yesterday's run survives the first midnight.
        today=QDate(2026,11,8);
        QTRY_COMPARE_WITH_TIMEOUT(series(),0,3000);
        QVERIFY(table->horizontalHeaderItem(2)->text().contains("02.11"));
        QEvent activate(QEvent::WindowActivate);
        today=QDate(2026,11,9); QCoreApplication::sendEvent(&window,&activate);
        QTRY_COMPARE(table->item(row,8)->background().color(),QColor("#FFFDFC"));
        QVERIFY(table->horizontalHeaderItem(2)->text().contains("02.11")); // No automatic jump to Monday.
        today=QDate(2026,11,5);
        QTest::mouseClick(cell(6),Qt::LeftButton); // The enabled cell is stale after a clock rollback.
        QTRY_COMPARE(series(),2); QVERIFY(!cell(6)->isEnabled()); QVERIFY(cell(6)->isChecked());
        today=QDate(2026,11,6); QCoreApplication::sendEvent(&window,&activate);
        QTRY_COMPARE(series(),3); QVERIFY(cell(6)->isChecked());
        QVERIFY(window.grab().save("stage3-week.png"));
        window.resize(800,600); QCoreApplication::processEvents();
        QVERIFY(table->horizontalScrollBar()->maximum()>0);
        QVERIFY(window.close());
        MainWindow reopened(nullptr,Database::Purpose::IntegrationTest,[&]{return today;});
        QSignalSpy reconnected(&reopened,&MainWindow::connectionFinished); reopened.show();
        QTRY_COMPARE_WITH_TIMEOUT(reconnected.count(),1,10000); QVERIFY(reconnected.at(0).at(0).toBool());
        auto *persisted=reopened.findChild<QTableWidget *>("habitsTable");
        int savedRow=-1; for(int i=0;i<persisted->rowCount();++i) if(persisted->item(i,0)->data(Qt::UserRole).toLongLong()==ids[0]) savedRow=i;
        QVERIFY(savedRow>=0); QCOMPARE(persisted->item(savedRow,1)->data(Qt::UserRole).toInt(),3);
        QVERIFY(persisted->cellWidget(savedRow,6)->findChild<QPushButton *>("completion")->isChecked());
        QVERIFY(reopened.close());
    }
};
QTEST_MAIN(WeekUiTest)
#include "WeekUiTest.moc"
