#pragma once

#include "data/Database.h"
#include "data/HabitRepository.h"
#include "data/CompletionRepository.h"
#include <QMainWindow>
#include <functional>

class QLabel;
class QPushButton;
class QTableWidget;
class WeekHeaderView;
class QStackedWidget;
class StatisticsPage;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    using DateProvider = std::function<QDate()>;
    explicit MainWindow(QWidget *parent = nullptr,
                        Database::Purpose purpose = Database::Purpose::Application,
                        DateProvider todayProvider = {});
signals:
    void connectionFinished(bool success);
private:
    bool event(QEvent *event) override;
    void checkDate();
    void showStatistics();
    void refreshStatistics();
    void returnToWeek();
    void navigateWeek(int weeks);
    void updateWeekHeader();
    void changeCompletion(qint64 id, const QDate &date, bool completed, QPushButton *button);
    void connectDatabase();
    bool loadHabits();
    void showDataError(const QString &message);
    void addHabit();
    void renameHabit(const Habit &habit);
    void deleteHabit(const Habit &habit);
    Database database_;
    HabitRepository repository_;
    CompletionRepository completions_;
    DateProvider todayProvider_;
    QDate today_;
    QDate weekStart_;
    bool connected_ = false;
    QStackedWidget *pages_ = nullptr;
    QWidget *weekPage_ = nullptr;
    StatisticsPage *statisticsPage_ = nullptr;
    QPushButton *statisticsButton_ = nullptr;
    WeekHeaderView *weekHeader_ = nullptr;
    QLabel *weekLabel_ = nullptr;
    QPushButton *previous_ = nullptr;
    QPushButton *next_ = nullptr;
    QTableWidget *table_ = nullptr;
    QWidget *statePanel_ = nullptr;
    QPushButton *add_ = nullptr;
    QPushButton *emptyAdd_ = nullptr;
    QLabel *message_ = nullptr;
    QPushButton *retry_ = nullptr;
    QPushButton *close_ = nullptr;
};
