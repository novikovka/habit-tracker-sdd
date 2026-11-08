#include "MainWindow.h"
#include "HabitDialog.h"
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>

class HabitUiTest : public QObject {
    Q_OBJECT
private slots:
    void validationCancelAndFailedSave()
    {
        int writes = 0;
        HabitDialog dialog(QStringLiteral("Добавить привычку"), {}, [&writes](const QString &, QString &error) {
            ++writes;
            error = QStringLiteral("Проверочная ошибка сохранения");
            return false;
        });
        dialog.show();
        auto *input = dialog.findChild<QLineEdit *>("habitName");
        auto *save = dialog.findChild<QPushButton *>("saveHabit");
        QVERIFY(input && save);
        QVERIFY(!save->isEnabled());
        input->setText("   ");
        QVERIFY(!save->isEnabled());
        input->setText(QString(101, QChar('x')));
        QVERIFY(!save->isEnabled());
        QCOMPARE(input->text().size(), 101); // No silent truncation.
        input->setText(QStringLiteral("Чтение"));
        QVERIFY(save->isEnabled());
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(writes, 1);
        QVERIFY(dialog.isVisible());
        QCOMPARE(input->text(), QStringLiteral("Чтение"));
        QVERIFY(dialog.findChild<QLabel *>("validationError")->text().contains(QStringLiteral("ошибка")));
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(!dialog.isVisible());
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }

    void integrationUserActionsAndReopen()
    {
        if (qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS") != "1") QSKIP("Integration not requested");
        DatabaseConfig config;
        QString error;
        QVERIFY2(DatabaseConfig::load(DatabaseConfig::defaultPath(), config, error), qUtf8Printable(error));
        config.database = Database::expectedName(Database::Purpose::IntegrationTest);
        Database db(Database::Purpose::IntegrationTest);
        QVERIFY(db.open(config));
        HabitRepository repo(db);
        QVector<qint64> created;
        const auto cleanup = qScopeGuard([&] { for (auto id : created) repo.remove(id); });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("connection.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue("database/host", config.host);
            settings.setValue("database/port", config.port);
            settings.setValue("database/name", config.database);
            settings.setValue("database/user", config.user);
            settings.sync();
        }
        const auto previous = qgetenv("HABIT_TRACKER_CONFIG");
        const auto restore = qScopeGuard([previous] {
            if (previous.isNull()) qunsetenv("HABIT_TRACKER_CONFIG"); else qputenv("HABIT_TRACKER_CONFIG", previous);
        });
        qputenv("HABIT_TRACKER_CONFIG", path.toUtf8());
        MainWindow window(nullptr, Database::Purpose::IntegrationTest);
        QSignalSpy connected(&window, &MainWindow::connectionFinished);
        window.show();
        QTRY_COMPARE_WITH_TIMEOUT(connected.count(), 1, 10000);
        QVERIFY(connected.at(0).at(0).toBool());
        auto *table = window.findChild<QTableWidget *>("habitsTable");
        auto *add = window.findChild<QPushButton *>("addHabit");
        QVERIFY(table && add && add->isEnabled());
        const int originalCount = table->rowCount();
        const QString name = QStringLiteral("Чтение ") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        bool handled = false;
        bool dialogImageSaved = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<HabitDialog *>("habitDialog");
            if (!dialog) return;
            dialog->findChild<QLineEdit *>("habitName")->setText("  " + name + "  ");
            dialogImageSaved = dialog->grab().save("stage2-dialog.png");
            handled = true;
            QTest::mouseClick(dialog->findChild<QPushButton *>("saveHabit"), Qt::LeftButton);
        });
        QTest::mouseClick(add, Qt::LeftButton);
        QVERIFY(handled);
        QVERIFY(dialogImageSaved);
        QCOMPARE(table->rowCount(), originalCount + 1);
        int row = -1;
        qint64 id = 0;
        for (int i=0; i<table->rowCount(); ++i) {
            if (table->item(i,0)->text() == name) { row=i; id=table->item(i,0)->data(Qt::UserRole).toLongLong(); break; }
        }
        if (id) created.append(id);
        QVERIFY(row >= 0);
        auto action = [&](const char *objectName) { return table->cellWidget(row,table->columnCount()-1)->findChild<QPushButton *>(objectName); };
        const QString renamed = QStringLiteral("Прогулка");
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<HabitDialog *>("habitDialog");
            if (!dialog) return;
            dialog->findChild<QLineEdit *>("habitName")->setText(renamed);
            QTest::keyClick(dialog->findChild<QLineEdit *>("habitName"), Qt::Key_Return);
        });
        QTest::mouseClick(action("renameHabit"), Qt::LeftButton);
        QCOMPARE(table->item(row,0)->text(), renamed);
        // Cancel rename and creation; no extra writes should appear.
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<HabitDialog *>("habitDialog");
            if (!dialog) return;
            dialog->findChild<QLineEdit *>("habitName")->setText("cancelled");
            QTest::mouseClick(dialog->findChild<QPushButton *>("cancelHabit"), Qt::LeftButton);
        });
        QTest::mouseClick(action("renameHabit"), Qt::LeftButton);
        QCOMPARE(table->item(row,0)->text(), renamed);
        QTimer::singleShot(0, &window, [&] {
            if (auto *dialog = window.findChild<HabitDialog *>("habitDialog")) dialog->reject();
        });
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(table->rowCount(), originalCount + 1);
        QVERIFY(window.grab().save("stage2-habits.png"));
        QVERIFY(window.close());
        QVERIFY(!window.isVisible());
        MainWindow reopened(nullptr, Database::Purpose::IntegrationTest);
        QSignalSpy reconnected(&reopened, &MainWindow::connectionFinished);
        reopened.show();
        QTRY_COMPARE_WITH_TIMEOUT(reconnected.count(), 1, 10000);
        QVERIFY(reconnected.at(0).at(0).toBool());
        auto *savedTable = reopened.findChild<QTableWidget *>("habitsTable");
        int savedRow = -1;
        for(int i=0; i<savedTable->rowCount(); ++i) if(savedTable->item(i,0)->data(Qt::UserRole).toLongLong()==id) savedRow=i;
        QVERIFY(savedRow >= 0);
        QCOMPARE(savedTable->item(savedRow,0)->text(), renamed);
        bool defaultCancel = false;
        QTimer::singleShot(0, &reopened, [&] {
            auto *box = reopened.findChild<QMessageBox *>("deleteConfirmation");
            if (!box) return;
            auto *cancel = box->findChild<QPushButton *>("cancelDeletion");
            defaultCancel = box->defaultButton() == cancel;
            QTest::mouseClick(cancel, Qt::LeftButton);
        });
        QTest::mouseClick(savedTable->cellWidget(savedRow,savedTable->columnCount()-1)->findChild<QPushButton *>("deleteHabit"), Qt::LeftButton);
        QVERIFY(defaultCancel);
        QCOMPARE(savedTable->rowCount(), originalCount + 1);
        QTimer::singleShot(0, &reopened, [&] {
            auto *box = reopened.findChild<QMessageBox *>("deleteConfirmation");
            if (box) QTest::mouseClick(box->findChild<QPushButton *>("confirmDeletion"), Qt::LeftButton);
        });
        QTest::mouseClick(savedTable->cellWidget(savedRow,savedTable->columnCount()-1)->findChild<QPushButton *>("deleteHabit"), Qt::LeftButton);
        QCOMPARE(savedTable->rowCount(), originalCount);
        created.removeAll(id);
        QVERIFY(reopened.close());
    }
};
QTEST_MAIN(HabitUiTest)
#include "HabitUiTest.moc"
