#include "MainWindow.h"
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class WindowSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void connectionFailureAndRetry()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("connection.ini");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[database]\nhost=127.0.0.1\nport=1\nname=habit_tracker_sdd\nuser=unused\n");
        file.close();
        const auto previous = qgetenv("HABIT_TRACKER_CONFIG");
        qputenv("HABIT_TRACKER_CONFIG", path.toUtf8());
        MainWindow window;
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
        if (previous.isNull()) qunsetenv("HABIT_TRACKER_CONFIG");
        else qputenv("HABIT_TRACKER_CONFIG", previous);
    }
};
QTEST_MAIN(WindowSmokeTest)
#include "WindowSmokeTest.moc"
