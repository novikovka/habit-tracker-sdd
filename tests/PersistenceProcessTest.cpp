#include "data/CompletionRepository.h"
#include "domain/Statistics.h"
#include <QCoreApplication>
#include <QProcess>
#include <QScopeGuard>
#include <QUuid>
#include <QtTest>

static int childRun(const QStringList &arguments)
{
    if(arguments.size()!=4) return 2;
    bool valid=false; const auto id=arguments[2].toLongLong(&valid);
    if(!valid || id<=0) return 3;
    DatabaseConfig config; QString error;
    if(!DatabaseConfig::load(DatabaseConfig::defaultPath(),config,error)) return 4;
    config.database=Database::expectedName(Database::Purpose::IntegrationTest);
    Database database(Database::Purpose::IntegrationTest);
    if(!database.open(config)) return 5;
    HabitRepository habits(database); CompletionRepository marks(database);
    const QDate today(2026,11,6);
    const QString savedName=arguments[3]+QStringLiteral(" сохранено");
    if(arguments[1]=="--writer") {
        if(!habits.rename(id,savedName)) return 6;
        if(!marks.setCompleted(id,today,true,today) || !marks.setCompleted(id,today.addDays(-1),true,today)) return 7;
        return 0;
    }
    TrackingSnapshot snapshot;
    if(!marks.snapshot(snapshot)) return 8;
    const auto statistics=Statistics::calculate(snapshot,today);
    for(const auto &habit:statistics.habits) {
        if(habit.id!=id) continue;
        return habit.name==savedName && habit.streak==2 && habit.days7==2 && habit.days30==2 ? 0 : 9;
    }
    return 10;
}

class PersistenceProcessTest : public QObject {
    Q_OBJECT
private slots:
    void savedBetweenIndependentProcesses()
    {
        if(qEnvironmentVariable("HABIT_TRACKER_RUN_DB_TESTS")!="1") QSKIP("Integration not requested");
        DatabaseConfig config; QString error;
        QVERIFY(DatabaseConfig::load(DatabaseConfig::defaultPath(),config,error));
        config.database=Database::expectedName(Database::Purpose::IntegrationTest);
        Database database(Database::Purpose::IntegrationTest); QVERIFY(database.open(config));
        HabitRepository habits(database); qint64 id=0;
        const auto name=QStringLiteral("process-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(habits.create(name,&id));
        const auto cleanup=qScopeGuard([&]{habits.remove(id);});
        QVector<qint64> processIds;
        for(const auto &mode:{QStringLiteral("--writer"),QStringLiteral("--reader")}) {
            QProcess child;
            const auto stop=qScopeGuard([&]{if(child.state()!=QProcess::NotRunning){child.kill();child.waitForFinished(3000);}});
            child.setProgram(QCoreApplication::applicationFilePath());
            child.setArguments({mode,QString::number(id),name});
            child.start();
            QVERIFY(child.waitForStarted(5000));
            processIds.append(child.processId());
            QVERIFY(child.waitForFinished(15000));
            QCOMPARE(child.exitStatus(),QProcess::NormalExit);
            QCOMPARE(child.exitCode(),0);
        }
        QVERIFY(processIds[0]>0 && processIds[1]>0 && processIds[0]!=processIds[1]);
    }
};

int main(int argc,char *argv[])
{
    QCoreApplication app(argc,argv);
    const auto arguments=QCoreApplication::arguments();
    if(arguments.size()>1 && (arguments[1]=="--writer" || arguments[1]=="--reader")) return childRun(arguments);
    PersistenceProcessTest test;
    return QTest::qExec(&test,argc,argv);
}
#include "PersistenceProcessTest.moc"
