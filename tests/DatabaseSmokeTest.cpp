#include "data/Database.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

class DatabaseSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void driverLoads()
    {
        {
            auto driver = QSqlDatabase::addDatabase("QPSQL", "driver-check");
            QVERIFY2(driver.isValid(), "QPSQL cannot load; check libpq and its DLL dependencies");
        }
        QSqlDatabase::removeDatabase("driver-check");
    }

    void refusesOtherDatabase()
    {
        Database db;
        QVERIFY(!db.open({"localhost", 5432, "postgres", "postgres"}));
        QVERIFY(!db.isOpen());
        QVERIFY(db.error().contains(QStringLiteral("неподходящая")));
        Database testDb(Database::Purpose::IntegrationTest);
        QVERIFY(!testDb.open({"localhost", 5432, "habit_tracker_sdd", "unused"}));
    }

    void failureDoesNotExposeConnectionDetails()
    {
        Database db;
        QVERIFY(!db.open({"127.0.0.1", 1, "habit_tracker_sdd", "private-role-sentinel"}));
        QVERIFY(!db.error().contains("private-role-sentinel"));
        const auto secret = qEnvironmentVariable("HABIT_TRACKER_DB_PASSWORD");
        QVERIFY(secret.isEmpty() || !db.error().contains(secret));
        QVERIFY(!db.error().isEmpty());
    }

    void configurationRejectsSecretAndInvalidPort()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("connection.ini");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[database]\npassword=sentinel\n");
        file.close();
        DatabaseConfig config;
        QString error;
        QVERIFY(!DatabaseConfig::load(path, config, error));
        QVERIFY(!error.contains("sentinel"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("[database]\nhost=localhost\nport=70000\n");
        file.close();
        QVERIFY(!DatabaseConfig::load(path, config, error));
    }

    void integration()
    {
        if (qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS") != "1")
            QSKIP("Database integration not requested; no database modified");
        DatabaseConfig config;
        QString error;
        QVERIFY2(DatabaseConfig::load(DatabaseConfig::defaultPath(), config, error), qUtf8Printable(error));
        config.database = Database::expectedName(Database::Purpose::IntegrationTest);
        Database db(Database::Purpose::IntegrationTest);
        QVERIFY2(db.open(config), qUtf8Printable(db.error()));
        // Queries are scoped so no handles survive close()/open().
        {
            QSqlQuery query(db.connection());
            QVERIFY(query.exec("SELECT version FROM public.schema_version"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
        }
        QVERIFY2(db.open(config), "Second initialization must preserve the existing schema");
        qlonglong persistentId = 0;
        {
            QSqlQuery query(db.connection());
            QVERIFY(query.exec("INSERT INTO public.habits(name) VALUES ('bootstrap-sentinel') RETURNING id"));
            QVERIFY(query.next());
            persistentId = query.value(0).toLongLong();
        }
        {
            Database second(Database::Purpose::IntegrationTest);
            const bool opened = second.open(config);
            bool preserved = false;
            if (opened) {
                QSqlQuery query(second.connection());
                query.prepare("SELECT name FROM public.habits WHERE id = ?");
                query.addBindValue(persistentId);
                preserved = query.exec() && query.next() && query.value(0).toString() == "bootstrap-sentinel";
            }
            QSqlQuery cleanup(db.connection());
            cleanup.prepare("DELETE FROM public.habits WHERE id = ?");
            cleanup.addBindValue(persistentId);
            QVERIFY(cleanup.exec());
            QVERIFY2(opened && preserved, "Repeated initialization must preserve committed data");
        }
        {
            auto connection = db.connection();
            QVERIFY(connection.transaction());
            QSqlQuery query(connection);
            QVERIFY(query.exec("INSERT INTO public.habits(name) VALUES ('smoke-sentinel') RETURNING id"));
            QVERIFY(query.next());
            const auto id = query.value(0).toLongLong();
            query.prepare("INSERT INTO public.habit_completions(habit_id, completed_on) VALUES (?, DATE '2024-02-29') ON CONFLICT DO NOTHING");
            query.addBindValue(id);
            QVERIFY(query.exec());
            QVERIFY(query.exec());
            query.prepare("SELECT count(*) FROM public.habit_completions WHERE habit_id = ?");
            query.addBindValue(id);
            QVERIFY(query.exec());
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            query.prepare("DELETE FROM public.habits WHERE id = ?");
            query.addBindValue(id);
            QVERIFY(query.exec());
            query.prepare("SELECT count(*) FROM public.habit_completions WHERE habit_id = ?");
            query.addBindValue(id);
            QVERIFY(query.exec());
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 0);
            QVERIFY(connection.rollback());
        }
        // A newer schema must be rejected, without silently downgrading it.
        {
            QSqlQuery query(db.connection());
            QVERIFY(query.exec("UPDATE public.schema_version SET version = 2"));
        }
        Database incompatible(Database::Purpose::IntegrationTest);
        const bool accepted = incompatible.open(config);
        // Restore before asserting so a failed assertion does not poison later runs.
        {
            QSqlQuery query(db.connection());
            QVERIFY(query.exec("UPDATE public.schema_version SET version = 1"));
        }
        QVERIFY(!accepted);
    }
};
QTEST_GUILESS_MAIN(DatabaseSmokeTest)
#include "DatabaseSmokeTest.moc"
