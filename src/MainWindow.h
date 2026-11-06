#pragma once

#include "data/Database.h"
#include <QMainWindow>

class QLabel;
class QPushButton;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr,
                        Database::Purpose purpose = Database::Purpose::Application);
signals:
    void connectionFinished(bool success);
private:
    void connectDatabase();
    Database database_;
    QLabel *message_ = nullptr;
    QPushButton *retry_ = nullptr;
    QPushButton *close_ = nullptr;
};
