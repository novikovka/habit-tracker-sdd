#include "Database.h"

#include <QFile>
#include <QSettings>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

// Referencing the generated initializer keeps the resource in the static library.
static void initializeResources() { Q_INIT_RESOURCE(schema); }

QString DatabaseConfig::defaultPath()
{
    const auto overridePath = qEnvironmentVariable("HABIT_TRACKER_CONFIG");
    if (!overridePath.isEmpty()) return overridePath;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/HabitTrackerSDD/connection.ini";
}

bool DatabaseConfig::load(const QString &path, DatabaseConfig &result, QString &error)
{
    error.clear();
    if (!QFile::exists(path)) {
        error = QStringLiteral("Не найдена конфигурация подключения. Выполните настройку по README.");
        return false;
    }
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup("database");
    if (settings.contains("password")) {
        error = QStringLiteral("Удалите пароль из конфигурации: он передаётся только через окружение процесса.");
        return false;
    }
    DatabaseConfig config;
    config.host = settings.value("host", "localhost").toString().trimmed();
    bool validPort = false;
    config.port = settings.value("port", 5432).toInt(&validPort);
    config.database = settings.value("name", "habit_tracker_sdd").toString().trimmed();
    config.user = settings.value("user", "habit_tracker_sdd_app").toString().trimmed();
    if (settings.status() != QSettings::NoError || !validPort
        || config.port < 1 || config.port > 65535 || config.user.isEmpty()
        || config.database.isEmpty()
        || (config.host != "localhost" && config.host != "127.0.0.1" && config.host != "::1")) {
        error = QStringLiteral("Проверьте настройки локального сервера, порта, базы и роли в конфигурации.");
        return false;
    }
    result = config;
    return true;
}

Database::Database(Purpose purpose) : purpose_(purpose),
    connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    initializeResources();
}

Database::~Database() { close(); }

QString Database::expectedName(Purpose purpose)
{
    return purpose == Purpose::Application ? QStringLiteral("habit_tracker_sdd")
                                          : QStringLiteral("habit_tracker_sdd_test");
}

void Database::close()
{
    if (db_.isValid()) db_.close();
    db_ = QSqlDatabase();
    if (QSqlDatabase::contains(connectionName_)) QSqlDatabase::removeDatabase(connectionName_);
}

bool Database::isOpen() const { return db_.isValid() && db_.isOpen(); }

bool Database::fail(const QString &message)
{
    error_ = message;
    if (db_.isValid()) db_.close();
    return false;
}

bool Database::open(const DatabaseConfig &config)
{
    close();
    error_.clear();
    // Check before any network request, including in the test runner.
    if (config.database != expectedName(purpose_)) {
        return fail(QStringLiteral("Выбрана неподходящая база данных. Подключение отменено."));
    }
    if (!QSqlDatabase::isDriverAvailable("QPSQL")) {
        return fail(QStringLiteral("Недоступен драйвер PostgreSQL. Проверьте установку Qt и библиотек PostgreSQL."));
    }
    db_ = QSqlDatabase::addDatabase("QPSQL", connectionName_);
    if (!db_.isValid()) {
        return fail(QStringLiteral("Не удалось загрузить драйвер PostgreSQL. Проверьте библиотеки приложения."));
    }
    db_.setHostName(config.host);
    db_.setPort(config.port);
    db_.setDatabaseName(config.database);
    db_.setUserName(config.user);
    db_.setConnectOptions("connect_timeout=3");
    const bool opened = db_.open(config.user, qEnvironmentVariable("HABIT_TRACKER_DB_PASSWORD"));
    db_.setPassword(QString());
    if (!opened) {
        return fail(QStringLiteral("Не удалось подключиться к базе данных. Проверьте сервер, настройки и пароль."));
    }
    {
        QSqlQuery query(db_);
        if (!query.exec("SELECT current_database()") || !query.next()
            || query.value(0).toString() != expectedName(purpose_)) {
            return fail(QStringLiteral("Сервер подключил неподходящую базу. Работа остановлена."));
        }
    }
    return initializeSchema();
}

bool Database::initializeSchema()
{
    if (!db_.transaction()) return fail(QStringLiteral("Не удалось начать проверку схемы базы данных."));
    bool ok = false;
    {
        QSqlQuery query(db_);
        // Serializes startup of two processes without changing the data.
        ok = query.exec("SELECT pg_advisory_xact_lock(733048201)")
             && query.exec("SELECT to_regclass('public.schema_version') IS NOT NULL")
             && query.next();
        if (ok && !query.value(0).toBool()) {
            ok = query.exec("SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public')")
                 && query.next() && !query.value(0).toBool();
            if (ok) {
                QFile migration(":/db/001_initial.sql");
                ok = migration.open(QIODevice::ReadOnly)
                     && query.exec(QString::fromUtf8(migration.readAll()));
            }
        }
        if (ok) {
            ok = query.exec("SELECT version FROM public.schema_version WHERE singleton = TRUE")
                 && query.next() && query.value(0).toInt() == 1 && !query.next();
        }
        if (ok) {
            ok = query.exec("SELECT id, name, created_at FROM public.habits LIMIT 0")
                 && query.exec("SELECT habit_id, completed_on FROM public.habit_completions LIMIT 0");
        }
    }
    if (!ok) {
        db_.rollback();
        return fail(QStringLiteral("Не удалось проверить схему базы данных. Нужны версия 1 и права на выделенную базу."));
    }
    if (!db_.commit()) {
        db_.rollback();
        return fail(QStringLiteral("Не удалось сохранить начальную схему базы данных."));
    }
    return true;
}
