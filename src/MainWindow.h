#pragma once

#include "data/Database.h"
#include "data/HabitRepository.h"
#include <QMainWindow>

class QLabel;
class QPushButton;
class QTableWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr,
                        Database::Purpose purpose = Database::Purpose::Application);
signals:
    void connectionFinished(bool success);
private:
    void connectDatabase();
    bool loadHabits();
    void showDataError(const QString &message);
    void addHabit();
    void renameHabit(const Habit &habit);
    void deleteHabit(const Habit &habit);
    Database database_;
    HabitRepository repository_;
    QTableWidget *table_ = nullptr;
    QWidget *statePanel_ = nullptr;
    QPushButton *add_ = nullptr;
    QPushButton *emptyAdd_ = nullptr;
    QLabel *message_ = nullptr;
    QPushButton *retry_ = nullptr;
    QPushButton *close_ = nullptr;
};
