#pragma once

#include <QSqlDatabase>
#include <QString>

struct DatabaseConfig {
    QString host;
    int port = 5432;
    QString database;
    QString user;

    static QString defaultPath();
    static bool load(const QString &path, DatabaseConfig &result, QString &error);
};

class Database final {
public:
    enum class Purpose { Application, IntegrationTest };
    explicit Database(Purpose purpose = Purpose::Application);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(const DatabaseConfig &config);
    void close();
    bool isOpen() const;
    QString error() const { return error_; }
    QSqlDatabase connection() const { return db_; }
    static QString expectedName(Purpose purpose);

private:
    bool initializeSchema();
    bool fail(const QString &message);
    Purpose purpose_;
    QString connectionName_;
    QSqlDatabase db_;
    QString error_;
};
