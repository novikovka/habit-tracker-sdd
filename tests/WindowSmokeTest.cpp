#include "MainWindow.h"
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class WindowSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void connectionFailureAndRetry()
    {
        const auto actualConfigPath = DatabaseConfig::defaultPath();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("connection.ini");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[database]\nhost=127.0.0.1\nport=1\nname=habit_tracker_sdd_test\nuser=unused\n");
        file.close();
        const auto previous = qgetenv("HABIT_TRACKER_CONFIG");
        const auto restoreConfig = qScopeGuard([previous] {
            if (previous.isNull()) qunsetenv("HABIT_TRACKER_CONFIG");
            else qputenv("HABIT_TRACKER_CONFIG", previous);
        });
        qputenv("HABIT_TRACKER_CONFIG", path.toUtf8());
        MainWindow window(nullptr, Database::Purpose::IntegrationTest);
        QSignalSpy spy(&window, &MainWindow::connectionFinished);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 1, 10000);
        QVERIFY(!spy.at(0).at(0).toBool());
        auto *retry = window.findChild<QPushButton *>("retry");
        QVERIFY(retry && retry->isVisible() && retry->isEnabled());
        const auto *message = window.findChild<QLabel *>("message");
        QVERIFY(message && !message->text().isEmpty());
        QVERIFY(window.grab().save("window-error.png"));
        QTest::mouseClick(retry, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 2, 10000);
        QVERIFY(!spy.at(1).at(0).toBool());
        const auto *statistics = window.findChild<QPushButton *>("statistics");
        QVERIFY(statistics && !statistics->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("addHabit")->isEnabled());
        if (qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS") == "1") {
            DatabaseConfig actual;
            QString error;
            QVERIFY2(DatabaseConfig::load(actualConfigPath, actual, error), qUtf8Printable(error));
            {
                QSettings settings(path, QSettings::IniFormat);
                settings.setValue("database/host", actual.host);
                settings.setValue("database/port", actual.port);
                settings.setValue("database/name", Database::expectedName(Database::Purpose::IntegrationTest));
                settings.setValue("database/user", actual.user);
                settings.sync();
            }
            QTest::mouseClick(retry, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 3, 10000);
            QVERIFY(spy.at(2).at(0).toBool());
            QVERIFY(!retry->isVisible());
            QVERIFY(window.findChild<QPushButton *>("addHabit")->isEnabled());
            QVERIFY(window.findChild<QPushButton *>("statistics")->isEnabled());
        }
    }

    void connectedWindow()
    {
        if (qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS") != "1")
            QSKIP("Connected window requires the isolated integration database");
        DatabaseConfig config;
        QString error;
        QVERIFY2(DatabaseConfig::load(DatabaseConfig::defaultPath(), config, error), qUtf8Printable(error));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("connection.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue("database/host", config.host);
            settings.setValue("database/port", config.port);
            settings.setValue("database/name", Database::expectedName(Database::Purpose::IntegrationTest));
            settings.setValue("database/user", config.user);
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }
        const auto previous = qgetenv("HABIT_TRACKER_CONFIG");
        const auto restoreConfig = qScopeGuard([previous] {
            if (previous.isNull()) qunsetenv("HABIT_TRACKER_CONFIG");
            else qputenv("HABIT_TRACKER_CONFIG", previous);
        });
        qputenv("HABIT_TRACKER_CONFIG", path.toUtf8());
        MainWindow window(nullptr, Database::Purpose::IntegrationTest);
        QSignalSpy spy(&window, &MainWindow::connectionFinished);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() == 1, 10000);
        QVERIFY2(spy.at(0).at(0).toBool(), qUtf8Printable(window.findChild<QLabel *>("message")->text()));
        QVERIFY(!window.findChild<QPushButton *>("retry")->isVisible());
        QVERIFY(!window.findChild<QLabel *>("weekRange")->text().isEmpty());
        QVERIFY(window.grab().save("window-connected.png"));
    }
};
QTEST_MAIN(WindowSmokeTest)
#include "WindowSmokeTest.moc"
